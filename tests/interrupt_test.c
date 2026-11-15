#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "arch/x86_64/interrupts.h"
#include "arch/x86_64/controllers.h"

struct access { uint16_t port; uint8_t value; };
struct fake_ports { struct access writes[128]; size_t count; uint8_t master_isr, slave_isr; };
static void fake_write(void *context, uint16_t port, uint8_t value) {
    struct fake_ports *ports = context;
    assert(ports->count < sizeof(ports->writes) / sizeof(ports->writes[0]));
    ports->writes[ports->count++] = (struct access){port, value};
}
static uint8_t fake_read(void *context, uint16_t port) {
    struct fake_ports *ports = context;
    assert(port == 0x20 || port == 0xa0);
    assert(ports->count != 0 && ports->writes[ports->count - 1].port == port &&
           ports->writes[ports->count - 1].value == 0x0b);
    return port == 0x20 ? ports->master_isr : ports->slave_isr;
}
static void expect(const struct fake_ports *ports, size_t index, uint16_t port, uint8_t value) {
    assert(index < ports->count && ports->writes[index].port == port && ports->writes[index].value == value);
}
static void descriptors_test(void) {
    struct x86_interrupt_tables tables, original;
    uintptr_t handlers[256];
    uint64_t stacks[3] = {UINT64_C(0xffffffff80020000), UINT64_C(0xffffffff80024000), UINT64_C(0xffffffff80028000)};
    for (size_t i = 0; i < 256; ++i) { handlers[i] = UINT64_C(0xffffffff80001000) + 16 * i; }
    memset(&tables, 0xa5, sizeof(tables));
    original = tables;
    assert(!x86_interrupt_tables_build(NULL, handlers, stacks[0], stacks));
    assert(!x86_interrupt_tables_build(&tables, NULL, stacks[0], stacks));
    assert(!x86_interrupt_tables_build(&tables, handlers, 0, stacks));
    assert(!x86_interrupt_tables_build(&tables, handlers, stacks[0] + 1, stacks));
    handlers[200] = UINT64_C(0x0000800000000000);
    assert(!x86_interrupt_tables_build(&tables, handlers, stacks[0], stacks));
    handlers[200] = UINT64_C(0xffffffff80001000) + 16 * 200;
    uint64_t bad_stacks[3] = {stacks[0], 16, stacks[2]};
    assert(!x86_interrupt_tables_build(&tables, handlers, stacks[0], bad_stacks));
    assert(memcmp(&tables, &original, sizeof(tables)) == 0);
    assert(x86_interrupt_tables_build(&tables, handlers, stacks[0], stacks));
    assert(tables.gdt[0] == 0 && tables.gdt[1] == UINT64_C(0x00af9a000000ffff));
    assert(tables.gdt[2] == UINT64_C(0x00cf92000000ffff));
    uint64_t descriptor = tables.gdt[3];
    uint64_t base = ((descriptor >> 16) & UINT64_C(0xffffff)) |
                    ((descriptor >> 32) & UINT64_C(0xff000000)) | (tables.gdt[4] << 32);
    assert(base == (uintptr_t)&tables.tss);
    assert((descriptor & 0xffffu) == sizeof(struct x86_tss) - 1);
    assert(((descriptor >> 40) & 0xffu) == 0x89 && ((descriptor >> 48) & 0xffu) == 0);
    assert(tables.gdt[4] <= UINT32_MAX && tables.tss.rsp[0] == stacks[0]);
    assert(tables.tss.rsp[1] == 0 && tables.tss.rsp[2] == 0);
    assert(tables.tss.iomap_base == sizeof(struct x86_tss));
    for (size_t i = 0; i < 7; ++i) { assert(tables.tss.ist[i] == (i < 3 ? stacks[i] : 0)); }
    for (size_t i = 0; i < 256; ++i) {
        const struct x86_idt_gate *gate = &tables.idt[i];
        uint64_t address = gate->low | ((uint64_t)gate->middle << 16) | ((uint64_t)gate->high << 32);
        assert(address == handlers[i] && gate->selector == X86_KERNEL_CODE);
        assert(gate->attributes == 0x8e && gate->reserved == 0);
        assert(gate->ist == (i == 8 ? 1 : i == 2 ? 2 : i == 18 ? 3 : 0));
    }
    assert(strcmp(x86_exception_name(0), "divide error") == 0);
    assert(strcmp(x86_exception_name(13), "general protection") == 0);
    assert(strcmp(x86_exception_name(14), "page fault") == 0);
    assert(strcmp(x86_exception_name(UINT64_MAX), "unexpected interrupt") == 0);
    puts("PASS: GDT/TSS encoding, all 256 IDT gates, IST selection and invalid descriptor inputs");
}
static void controllers_test(void) {
    struct fake_ports ports = {0};
    const struct port_io io = {&ports, fake_read, fake_write};
    struct pic8259 pic = {0};
    assert(!pic8259_init(NULL, &io) && !pic8259_init(&pic, NULL));
    assert(!pic8259_mask(&pic, 0) && pic8259_delivery(&pic, 0) == PIC_INVALID);
    assert(!pic8259_eoi(&pic, 0, PIC_REAL) && ports.count == 0);
    assert(pic8259_init(&pic, &io) && pic.mask == UINT16_MAX);
    const struct access init[] = {{0x21, 0xff}, {0xa1, 0xff}, {0x20, 0x11}, {0xa0, 0x11},
        {0x21, 32}, {0xa1, 40}, {0x21, 4}, {0xa1, 2}, {0x21, 1}, {0xa1, 1}};
    assert(ports.count == 22);
    for (size_t i = 0; i < 10; ++i) {
        expect(&ports, 2 * i, init[i].port, init[i].value);
        expect(&ports, 2 * i + 1, 0x80, 0);
    }
    expect(&ports, 20, 0x21, 0xff); expect(&ports, 21, 0xa1, 0xff);
    assert(!pic8259_init(&pic, &io) && ports.count == 22);
    ports.count = 0;
    assert(pic8259_mask(&pic, 0xfffe) && pic.mask == 0xfffe);
    expect(&ports, 0, 0x21, 0xfe); expect(&ports, 1, 0xa1, 0xff);
    ports.count = 0;
    assert(pic8259_mask(&pic, 0xfeff) && pic.mask == 0xfefb); /* Open cascade for slave IRQ8. */
    expect(&ports, 0, 0x21, 0xfb); expect(&ports, 1, 0xa1, 0xfe);
    ports.count = 0;
    assert(pic8259_delivery(&pic, 0) == PIC_REAL && ports.count == 0);
    assert(pic8259_eoi(&pic, 0, PIC_REAL) && ports.count == 1);
    expect(&ports, 0, 0x20, 0x20);
    ports.count = 0;
    assert(pic8259_eoi(&pic, 8, PIC_REAL) && ports.count == 2);
    expect(&ports, 0, 0xa0, 0x20); expect(&ports, 1, 0x20, 0x20); /* Slave first. */
    ports.count = 0;
    assert(pic8259_delivery(&pic, 7) == PIC_SPURIOUS_MASTER);
    assert(pic8259_eoi(&pic, 7, PIC_SPURIOUS_MASTER) && ports.count == 1); /* No EOI. */
    expect(&ports, 0, 0x20, 0x0b);
    ports.count = 0;
    assert(pic8259_delivery(&pic, 15) == PIC_SPURIOUS_SLAVE);
    assert(pic8259_eoi(&pic, 15, PIC_SPURIOUS_SLAVE) && ports.count == 2);
    expect(&ports, 0, 0xa0, 0x0b); expect(&ports, 1, 0x20, 0x20);
    ports.master_isr = ports.slave_isr = 0x80;
    assert(pic8259_delivery(&pic, 7) == PIC_REAL && pic8259_delivery(&pic, 15) == PIC_REAL);
    ports.count = 0;
    assert(pic8259_delivery(&pic, 16) == PIC_INVALID);
    assert(!pic8259_eoi(&pic, 16, PIC_REAL) && !pic8259_eoi(&pic, 0, PIC_INVALID));
    assert(!pic8259_eoi(&pic, 0, PIC_SPURIOUS_SLAVE));
    assert(!pic8259_eoi(&pic, 7, (enum pic_delivery)99) && ports.count == 0);
    uint32_t divisor = 123;
    assert(!pit8254_program(&io, 0, &divisor) && divisor == 123);
    assert(!pit8254_program(&io, 18, &divisor));
    assert(!pit8254_program(&io, UINT32_MAX, &divisor));
    assert(!pit8254_program(&io, PIT_INPUT_HZ, &divisor));
    assert(!pit8254_program(NULL, 100, &divisor) && !pit8254_program(&io, 100, NULL) && ports.count == 0);
    assert(pit8254_program(&io, 100, &divisor) && divisor == 11932 && ports.count == 3);
    expect(&ports, 0, 0x43, 0x34); expect(&ports, 1, 0x40, 0x9c); expect(&ports, 2, 0x40, 0x2e);
    ports.count = 0;
    assert(pit8254_program(&io, 19, &divisor) && divisor >= 2 && divisor <= 65536);
    assert(pit8254_program(&io, PIT_INPUT_HZ / 2, &divisor) && divisor == 2);
    puts("PASS: PIC initialization/masks, real and spurious EOI ordering, PIT divisor bounds and port bytes");
}
int main(void) { descriptors_test(); controllers_test(); return 0; }
