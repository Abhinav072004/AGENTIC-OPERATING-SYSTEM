#include "interrupts.h"
#include "arch/x86_64/interrupts.h"
#include "arch/x86_64/controllers.h"
#include "arch/x86_64/io.h"
#include "arch/x86_64/serial.h"
#include "debug/log.h"
#include "debug/panic.h"
#include "memory/probe.h"
#include <stdatomic.h>
#include "scheduler/task.h"
#include "user/user.h"

static struct pic8259 pic;
static bool initialized;
static volatile bool returning_probe;
static atomic_bool fatal_active;
_Static_assert(ATOMIC_BOOL_LOCK_FREE == 2, "fatal reentry guard must not need a runtime lock");
/* Only the bootstrap CPU writes these; interrupt gates serialize IRQ handlers. */
static volatile uint64_t ticks, timer_eois, timer_saved_if, unexpected_irqs;
static volatile uint64_t software_returns, spurious_master, spurious_slave;
static uint8_t read_port(void *context, uint16_t port) { (void)context; return inb(port); }
static void write_port(void *context, uint16_t port, uint8_t value) { (void)context; outb(port, value); }
static const struct port_io ports = {NULL, read_port, write_port};
static uint64_t flags_read(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; pop %0" : "=r"(flags) : : "memory");
    return flags;
}

void interrupts_init(void) {
    KASSERT(!initialized && (flags_read() & UINT64_C(0x200)) == 0);
    KASSERT(arch_interrupts_init());
    KASSERT(pic8259_init(&pic, &ports));
    initialized = true;
    klog(LOG_INFO, "interrupt", "PIC vectors=32..47; all IRQs masked; IF=0");
}

static _Noreturn void fatal_exception(const struct interrupt_frame *frame) {
    if (atomic_exchange_explicit(&fatal_active, true, memory_order_relaxed)) {
        serial_write("AGENTICOS: recursive exception; halted\n");
        cpu_halt();
    }
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
#if CONFIG_SELFTEST >= 4 && CONFIG_SELFTEST <= 6
    if (frame->vector == 14) { memory_fault_report(cr2, frame->error, frame->rip); }
#endif
    klog(LOG_PANIC, "exception", "vector=%llu name=%s error=0x%llx rip=%p cr2=%p",
         (unsigned long long)frame->vector, x86_exception_name(frame->vector),
         (unsigned long long)frame->error, (void *)(uintptr_t)frame->rip, (void *)(uintptr_t)cr2);
    klog(LOG_PANIC, "exception", "cs=0x%llx ss=0x%llx rflags=0x%llx rsp=%p stack=%u",
         (unsigned long long)frame->cs, (unsigned long long)frame->ss,
         (unsigned long long)frame->rflags, (void *)(uintptr_t)frame->rsp,
         arch_interrupt_stack((uintptr_t)frame));
    klog(LOG_PANIC, "exception", "rax=0x%llx rbx=0x%llx rcx=0x%llx rdx=0x%llx",
         (unsigned long long)frame->rax, (unsigned long long)frame->rbx,
         (unsigned long long)frame->rcx, (unsigned long long)frame->rdx);
    klog(LOG_PANIC, "exception", "rsi=0x%llx rdi=0x%llx rbp=0x%llx",
         (unsigned long long)frame->rsi, (unsigned long long)frame->rdi, (unsigned long long)frame->rbp);
    klog(LOG_PANIC, "exception", "r8=0x%llx r9=0x%llx r10=0x%llx r11=0x%llx",
         (unsigned long long)frame->r8, (unsigned long long)frame->r9,
         (unsigned long long)frame->r10, (unsigned long long)frame->r11);
    klog(LOG_PANIC, "exception", "r12=0x%llx r13=0x%llx r14=0x%llx r15=0x%llx",
         (unsigned long long)frame->r12, (unsigned long long)frame->r13,
         (unsigned long long)frame->r14, (unsigned long long)frame->r15);
    if (frame->vector == 14) {
        klog(LOG_PANIC, "page-fault", "present=%u write=%u user=%u reserved=%u fetch=%u pkey=%u shadow_stack=%u sgx=%u",
             (unsigned int)(frame->error & 1u), (unsigned int)((frame->error >> 1) & 1u),
             (unsigned int)((frame->error >> 2) & 1u), (unsigned int)((frame->error >> 3) & 1u),
             (unsigned int)((frame->error >> 4) & 1u), (unsigned int)((frame->error >> 5) & 1u),
             (unsigned int)((frame->error >> 6) & 1u), (unsigned int)((frame->error >> 15) & 1u));
    }
    serial_write("AGENTICOS: EXCEPTION HALT\n");
    cpu_halt();
}

struct interrupt_frame *interrupt_dispatch(struct interrupt_frame *frame) {
    if (frame->vector >= PIC_VECTOR_BASE && frame->vector < PIC_VECTOR_BASE + 16u) {
        unsigned int irq = (unsigned int)frame->vector - PIC_VECTOR_BASE;
        enum pic_delivery delivery = pic8259_delivery(&pic, irq);
        if (delivery == PIC_INVALID) { fatal_exception(frame); }
        if (delivery == PIC_SPURIOUS_MASTER) { ++spurious_master; }
        else if (delivery == PIC_SPURIOUS_SLAVE) { ++spurious_slave; }
        else if (irq == 0) {
            ++ticks;
            if ((frame->rflags & UINT64_C(0x200)) != 0) { ++timer_saved_if; }
        } else { ++unexpected_irqs; }
        if (!pic8259_eoi(&pic, irq, delivery)) { fatal_exception(frame); }
        if (irq == 0 && delivery == PIC_REAL) { ++timer_eois; }
        if (irq == 0 && delivery == PIC_REAL) { user_timer_entry(frame); }
        return irq == 0 && delivery == PIC_REAL ? scheduler_interrupt(frame, true) : frame;
    }
    if ((frame->cs & 3u) == 3) {
        if (frame->vector == X86_SYSCALL_VECTOR) { return syscall_dispatch(frame); }
        if (frame->vector < 32 && frame->vector != 2 && frame->vector != 8 && frame->vector != 18) { return user_fault(frame); }
    }
    if (frame->vector == 128 && returning_probe) {
        KASSERT((flags_read() & UINT64_C(0x600)) == 0); /* Live DF and IF clear. */
        KASSERT(frame->error == 0 && frame->cs == X86_KERNEL_CODE && frame->ss == X86_KERNEL_DATA);
        KASSERT((frame->rflags & UINT64_C(0x601)) == UINT64_C(0x401));
        const uint64_t values[] = {frame->rax, frame->rbx, frame->rcx, frame->rdx,
            frame->rbp, frame->rsi, frame->rdi, frame->r8, frame->r9, frame->r10,
            frame->r11, frame->r12, frame->r13, frame->r14, frame->r15};
        for (size_t i = 0; i < 15; ++i) { KASSERT(values[i] == (UINT64_C(1) << 40) + i + 1); }
        ++software_returns;
        return frame;
    }
    if (frame->vector == 129) { return scheduler_interrupt(frame, false); }
    fatal_exception(frame);
}

void interrupts_self_test(void) {
    KASSERT(initialized && (flags_read() & UINT64_C(0x200)) == 0 && pic.mask == UINT16_MAX);
    returning_probe = true;
    KASSERT(arch_interrupt_roundtrip());
    returning_probe = false;
    KASSERT(software_returns == 1);
    __asm__ volatile ("int $0x27; int $0x2f" : : : "memory");
    KASSERT(spurious_master == 1 && spurious_slave == 1 && ticks == 0 && unexpected_irqs == 0);
    klog(LOG_INFO, "interrupt-test", "IRETQ preserved 15 GPRs, RSP, CF/DF; live DF/IF clear; spurious IRQ7/15 passed");
    serial_write("AGENTICOS: IRET PASS\n");
}

uint64_t interrupts_ticks(void) { return ticks; }
uint32_t interrupts_timer_start(void) {
    KASSERT(initialized && (flags_read() & UINT64_C(0x200)) == 0 && pic.mask == UINT16_MAX);
    uint32_t divisor;
    KASSERT(pit8254_program(&ports, 100, &divisor));
    KASSERT(pic8259_mask(&pic, UINT16_C(0xfffe)));
    return divisor;
}
void interrupts_timer_stop(void) {
    KASSERT(initialized && (flags_read() & UINT64_C(0x200)) == 0);
    KASSERT(pic8259_mask(&pic, UINT16_MAX));
}
void interrupts_timer_test(void) {
    uint32_t divisor = interrupts_timer_start();
    while (ticks < 8) {
        /* Atomic STI/HLT pair prevents a wakeup being lost before sleep. */
        __asm__ volatile ("sti; hlt; cli" : : : "memory");
    }
    interrupts_timer_stop();
    KASSERT(ticks == timer_eois && ticks == timer_saved_if && ticks >= 8 && unexpected_irqs == 0);
    klog(LOG_INFO, "timer", "PIT input_hz=%u divisor=%u ticks=%llu eoi=%llu saved_IF=1; PIC masked, IF=0",
         PIT_INPUT_HZ, divisor, (unsigned long long)ticks, (unsigned long long)timer_eois);
    serial_write("AGENTICOS: TIMER PASS\n");
}
