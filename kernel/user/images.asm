bits 64
section .rodata
align 16
global user_hello_start, user_hello_end, user_probe_start, user_probe_end
%strcat hello_file USER_BUILD, "/hello.elf"
%strcat probe_file USER_BUILD, "/probe.elf"
user_hello_start:
    incbin hello_file
user_hello_end:
align 16
user_probe_start:
    incbin probe_file
user_probe_end:
align 16
global user_fstest_start, user_fstest_end
%strcat fstest_file USER_BUILD, "/fstest.elf"
user_fstest_start:
    incbin fstest_file
user_fstest_end:
section .note.GNU-stack noalloc noexec nowrite progbits
