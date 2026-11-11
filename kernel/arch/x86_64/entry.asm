bits 64
default rel
section .text
global _start
extern kernel_main

_start:
    cli                         ; No IDT yet: keep maskable interrupts disabled.
    cld                         ; C code assumes forward string operations.
    lea rsp, [stack_top]         ; Use a kernel-owned, 16-byte-aligned stack.
    xor ebp, ebp                ; Terminate a debugger's frame-pointer chain.
    call kernel_main            ; SysV ABI: RSP is 16-byte aligned before CALL.
.halt:
    cli
    hlt
    jmp .halt

section .bss
align 16
stack_bottom:
    resb 65536
stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits
