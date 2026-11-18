bits 64
section .text.start
global _user_start
extern user_main
_user_start:
    cld
    and rsp, -16
    call user_main
    movsxd rdi, eax            ; Signed 32-bit native program exit status.
    mov eax, 3                 ; Native SYS_EXIT; not Linux libc.
    int 0x82
    ud2
section .note.GNU-stack noalloc noexec nowrite progbits
