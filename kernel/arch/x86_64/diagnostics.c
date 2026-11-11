#include "diagnostics.h"
#include "io.h"

void arch_disable_interrupts(void) {
    __asm__ volatile ("cli" : : : "memory");
}

_Noreturn void arch_panic_stop(void) {
    cpu_halt();
}
