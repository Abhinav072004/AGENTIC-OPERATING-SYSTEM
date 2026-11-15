bits 64
default rel
section .text
global memory_execute_address

memory_execute_address:
    call rdi                    ; Must fault on an NX leaf.
    ret                         ; Broken NX returns to the explicit failure path.

section .note.GNU-stack noalloc noexec nowrite progbits
