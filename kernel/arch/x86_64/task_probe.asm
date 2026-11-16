bits 64
default rel
section .text
global arch_task_probe
extern scheduler_preempt_signal

; Bootstrap verification only. false: yield with all registers live.
; true: never yield; another task must run through real timer preemption.
arch_task_probe:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 32
    mov [rsp], rsp
    mov [rsp + 16], rdi
    pushfq
    pop qword [rsp + 24]
%macro set_register 2
    mov %1, 0x10000000000 + %2
%endmacro
    set_register rax, 1
    set_register rbx, 2
    set_register rcx, 3
    set_register rdx, 4
    set_register rbp, 5
    set_register rsi, 6
    set_register rdi, 7
    set_register r8, 8
    set_register r9, 9
    set_register r10, 10
    set_register r11, 11
    set_register r12, 12
    set_register r13, 13
    set_register r14, 14
    set_register r15, 15
    cmp qword [rsp + 16], 0
    jne .timer
    std
    stc
    int 0x81
    jmp .verify
.timer:
    std
.wait:
    pause
    cmp qword [rel scheduler_preempt_signal], 0
    je .wait
.verify:
    pushfq
    pop qword [rsp + 8]
    cld
%assign index 1
%macro verify_register 1
    btc %1, 40
    cmp %1, index
    jne .failed
%assign index index + 1
%endmacro
    verify_register rax
    verify_register rbx
    verify_register rcx
    verify_register rdx
    verify_register rbp
    verify_register rsi
    verify_register rdi
    verify_register r8
    verify_register r9
    verify_register r10
    verify_register r11
    verify_register r12
    verify_register r13
    verify_register r14
    verify_register r15
    cmp rsp, [rsp]
    jne .failed
    mov rax, [rsp + 24]
    and eax, 0x200
    or eax, 0x400              ; Saved IF and the deliberately set DF must survive.
    mov rdx, [rsp + 8]
    and edx, 0x600
    cmp eax, edx
    jne .failed
    cmp qword [rsp + 16], 0
    jne .passed
    test qword [rsp + 8], 1    ; Yield probe additionally checks original CF.
    jz .failed
.passed:
    mov eax, 1
    jmp .done
.failed:
    xor eax, eax
.done:
    add rsp, 32
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
