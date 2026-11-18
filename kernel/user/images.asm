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
align 16
global user_shell_start, user_shell_end
%strcat shell_file USER_BUILD, "/shell.elf"
user_shell_start:
    incbin shell_file
user_shell_end:
align 16
global user_nettest_start, user_nettest_end
%strcat nettest_file USER_BUILD, "/nettest.elf"
user_nettest_start:
    incbin nettest_file
user_nettest_end:
align 16
global user_desktop_start, user_desktop_end
%strcat desktop_file USER_BUILD, "/desktop-image.elf"
user_desktop_start:
    incbin desktop_file
user_desktop_end:
section .note.GNU-stack noalloc noexec nowrite progbits
