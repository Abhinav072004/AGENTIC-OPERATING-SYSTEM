#ifndef AGENTICOS_DIAGNOSTICS_H
#define AGENTICOS_DIAGNOSTICS_H

/* Out-of-line architecture boundary lets host tests replace privileged code. */
void arch_disable_interrupts(void);
_Noreturn void arch_panic_stop(void);

#endif
