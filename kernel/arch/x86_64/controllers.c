#include "controllers.h"
#include <stddef.h>

static bool valid_io(const struct port_io *io) { return io != NULL && io->read != NULL && io->write != NULL; }
static void write_port(const struct port_io *io, uint16_t port, uint8_t value) {
    io->write(io->context, port, value);
}
static void slow_write(const struct port_io *io, uint16_t port, uint8_t value) {
    write_port(io, port, value);
    write_port(io, 0x80, 0); /* Legacy I/O delay between PIC initialization words. */
}
bool pic8259_init(struct pic8259 *pic, const struct port_io *io) {
    if (pic == NULL || !valid_io(io) || pic->ready) { return false; }
    pic->io = *io;
    slow_write(io, 0x21, 0xff);
    slow_write(io, 0xa1, 0xff);
    slow_write(io, 0x20, 0x11); /* ICW1: initialize, cascaded, ICW4 follows. */
    slow_write(io, 0xa0, 0x11);
    slow_write(io, 0x21, PIC_VECTOR_BASE);
    slow_write(io, 0xa1, PIC_VECTOR_BASE + 8u);
    slow_write(io, 0x21, 4);    /* Slave on master IRQ2. */
    slow_write(io, 0xa1, 2);
    slow_write(io, 0x21, 1);    /* 8086 mode, explicit EOI (not auto-EOI). */
    slow_write(io, 0xa1, 1);
    pic->ready = true;
    return pic8259_mask(pic, UINT16_MAX);
}
bool pic8259_mask(struct pic8259 *pic, uint16_t mask) {
    if (pic == NULL || !pic->ready) { return false; }
    if ((mask & 0xff00u) != 0xff00u) { mask &= (uint16_t)~UINT16_C(4); }
    write_port(&pic->io, 0x21, (uint8_t)mask);
    write_port(&pic->io, 0xa1, (uint8_t)(mask >> 8));
    pic->mask = mask;
    return true;
}
enum pic_delivery pic8259_delivery(struct pic8259 *pic, unsigned int irq) {
    if (pic == NULL || !pic->ready || irq >= 16) { return PIC_INVALID; }
    if (irq == 7 || irq == 15) {
        uint16_t port = irq == 7 ? 0x20 : 0xa0;
        write_port(&pic->io, port, 0x0b); /* OCW3: read in-service register. */
        if ((pic->io.read(pic->io.context, port) & 0x80u) == 0) {
            return irq == 7 ? PIC_SPURIOUS_MASTER : PIC_SPURIOUS_SLAVE;
        }
    }
    return PIC_REAL;
}
bool pic8259_eoi(struct pic8259 *pic, unsigned int irq, enum pic_delivery delivery) {
    if (pic == NULL || !pic->ready || irq >= 16 || delivery == PIC_INVALID ||
        (delivery != PIC_REAL && delivery != PIC_SPURIOUS_MASTER && delivery != PIC_SPURIOUS_SLAVE) ||
        (delivery == PIC_SPURIOUS_MASTER && irq != 7) ||
        (delivery == PIC_SPURIOUS_SLAVE && irq != 15)) { return false; }
    if (delivery == PIC_SPURIOUS_MASTER) { return true; }
    if (irq >= 8 && delivery == PIC_REAL) { write_port(&pic->io, 0xa0, 0x20); }
    write_port(&pic->io, 0x20, 0x20); /* Spurious slave still acknowledges master IRQ2. */
    return true;
}
bool pit8254_program(const struct port_io *io, uint32_t hz, uint32_t *divisor) {
    if (!valid_io(io) || divisor == NULL || hz == 0 || hz > PIT_INPUT_HZ / 2) { return false; }
    uint32_t count = (PIT_INPUT_HZ + hz / 2) / hz;
    if (count < 2 || count > 65536) { return false; }
    write_port(io, 0x43, 0x34); /* Channel 0, lobyte/hibyte, periodic rate generator. */
    write_port(io, 0x40, (uint8_t)count);
    write_port(io, 0x40, (uint8_t)(count >> 8));
    *divisor = count;
    return true;
}
