bits 64
default rel
section .text
global arch_load_gdt
global arch_interrupt_roundtrip
extern interrupt_dispatch

arch_load_gdt:
    lgdt [rdi]
    push qword 8
    lea rax, [.code_loaded]
    push rax
    retfq
.code_loaded:
    mov ax, 16
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor eax, eax
    mov fs, ax
    mov gs, ax
    mov ax, 24
    ltr ax
    ret

; The CPU supplies error codes only for these vectors. Other stubs add zero,
; so all 256 paths present the same frame to C. Long mode always saves SS:RSP.
%assign vector 0
%rep 256
interrupt_stub_%+vector:
%if vector != 8 && vector != 10 && vector != 11 && vector != 12 && vector != 13 && vector != 14 && vector != 17 && vector != 21 && vector != 29 && vector != 30
    push qword 0
%endif
    push strict dword vector
    jmp interrupt_common
%assign vector vector + 1
%endrep

interrupt_common:
    cld                         ; C ABI requires DF clear; IRET restores saved DF.
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    and rsp, -16                ; Interrupted code may have either stack alignment.
    call interrupt_dispatch
    mov rsp, rax                ; Dispatcher may select another task's saved frame.
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16                 ; Discard vector and normalized error code.
    iretq

; Prove every GPR, original RSP and CF/DF survive C dispatch and IRETQ.
; The caller runs with IF clear. Preserve the caller's own callee-saved GPRs.
arch_interrupt_roundtrip:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 16
    mov [rsp], rsp
    mov eax, 1
    mov ebx, 2
    mov ecx, 3
    mov edx, 4
    mov ebp, 5
    mov esi, 6
    mov edi, 7
    mov r8d, 8
    mov r9d, 9
    mov r10d, 10
    mov r11d, 11
    mov r12d, 12
    mov r13d, 13
    mov r14d, 14
    mov r15d, 15
%macro high_sentinel 1
    bts %1, 40
%endmacro
    high_sentinel rax
    high_sentinel rbx
    high_sentinel rcx
    high_sentinel rdx
    high_sentinel rbp
    high_sentinel rsi
    high_sentinel rdi
    high_sentinel r8
    high_sentinel r9
    high_sentinel r10
    high_sentinel r11
    high_sentinel r12
    high_sentinel r13
    high_sentinel r14
    high_sentinel r15
    std
    stc
    int 0x80
    pushfq
    pop qword [rsp + 8]
    cld
%assign index 1
%macro check_register 1
    btc %1, 40                  ; Check upper bits as well as the unique low value.
    cmp %1, index
    jne .failed
%assign index index + 1
%endmacro
    check_register rax
    check_register rbx
    check_register rcx
    check_register rdx
    check_register rbp
    check_register rsi
    check_register rdi
    check_register r8
    check_register r9
    check_register r10
    check_register r11
    check_register r12
    check_register r13
    check_register r14
    check_register r15
    cmp rsp, [rsp]
    jne .failed
    mov rax, [rsp + 8]
    and eax, 0x601              ; Original CF/DF set, IF clear.
    cmp eax, 0x401
    jne .failed
    mov eax, 1
    jmp .done
.failed:
    xor eax, eax
.done:
    add rsp, 16
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

section .rodata
align 8
global interrupt_stub_table
interrupt_stub_table:
%assign vector 0
%rep 256
    dq interrupt_stub_%+vector
%assign vector vector + 1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits
