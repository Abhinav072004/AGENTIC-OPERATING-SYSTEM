bits 64
default rel
section .text
global memory_page_fault_entry
global memory_execute_address
extern memory_fault_report

memory_page_fault_entry:
    cli
    cld
    mov rdi, cr2                 ; Faulting linear address.
    mov rsi, [rsp]               ; CPU-pushed error code.
    mov rdx, [rsp + 8]           ; CPU-pushed RIP.
    and rsp, -16                 ; SysV ABI alignment before CALL.
    call memory_fault_report    ; Reports and halts, never returns.
.halt:
    cli
    hlt
    jmp .halt

memory_execute_address:
    call rdi                    ; Must fault on an NX leaf.
    ret                         ; Broken NX returns to the explicit failure path.

section .note.GNU-stack noalloc noexec nowrite progbits
