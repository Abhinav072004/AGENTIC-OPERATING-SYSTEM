/* Deliberate CPU faults exist only in explicitly selected test images. */
#include <stdint.h>
#include "arch/x86_64/io.h"
#include "arch/x86_64/serial.h"
#include "debug/panic.h"
#include "memory/init.h"
_Noreturn void interrupt_exception_probe(unsigned int mode);
void exception_divide(void);
void exception_invalid_opcode(void);
void exception_general_protection(void);
void exception_page_fault(void);
void exception_double_fault(void);
void exception_write_ro(uintptr_t address);
void exception_execute_nx(uintptr_t address);
extern unsigned char __rodata_start[];

_Noreturn void interrupt_exception_probe(unsigned int mode) {
    if (mode == 8) { exception_divide(); }
    else if (mode == 9) { exception_invalid_opcode(); }
    else if (mode == 10) { exception_general_protection(); }
    else if (mode == 11) { exception_page_fault(); }
    else if (mode == 12) { exception_double_fault(); }
    else if (mode == 13) {
        serial_write("AGENTICOS: NMI WAIT\n");
        cpu_halt(); /* The host injects an NMI through QMP with maskable IF clear. */
    } else if (mode == 14) { exception_write_ro((uintptr_t)__rodata_start); }
    else if (mode == 15) {
        unsigned char *code = kmalloc(16);
        KASSERT(code != NULL);
        *code = 0xc3; /* RET would return if NX protection were broken. */
        exception_execute_nx((uintptr_t)code);
    }
    PANIC("exception probe returned without its expected CPU exception");
}
