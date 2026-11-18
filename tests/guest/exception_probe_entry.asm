bits 64
default rel
section .text
global exception_divide, exception_invalid_opcode, exception_general_protection
global exception_page_fault, exception_double_fault
global probe_divide_instruction, probe_ud_instruction, probe_gp_instruction
global probe_pf_instruction, probe_df_instruction
global exception_write_ro, exception_execute_nx, probe_ro_instruction

exception_divide:
    xor edx, edx
    xor eax, eax
    xor ecx, ecx
probe_divide_instruction:
    div rcx
    ret
exception_invalid_opcode:
probe_ud_instruction:
    ud2
    ret
exception_general_protection:
    mov ax, 0x38                ; Beyond the owned GDT's 0x37 limit.
probe_gp_instruction:
    mov ds, ax
    ret
exception_page_fault:
    mov rax, 0xffffd00000000000 ; Unmapped lower heap guard.
probe_pf_instruction:
    mov rax, [rax]
    ret
exception_double_fault:
    mov rsp, 0xffffd00000000800 ; No usable normal stack for #PF delivery.
probe_df_instruction:
    mov rax, [rsp]             ; Nested #PF delivery becomes #DF on IST1.
    ud2
exception_write_ro:
probe_ro_instruction:
    mov byte [rdi], 0
    ret
exception_execute_nx:
    call rdi                   ; The CPU's fault RIP must be the NX heap address.
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
