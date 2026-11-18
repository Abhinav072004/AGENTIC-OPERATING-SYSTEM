bits 64
section .rodata
align 16
global user_hello_start, user_hello_end, user_probe_start, user_probe_end
%strcat hello_file USER_BUILD, "/hello-image.elf"
%strcat probe_file USER_BUILD, "/probe-image.elf"
user_hello_start:
    incbin hello_file
user_hello_end:
align 16
user_probe_start:
    incbin probe_file
user_probe_end:
align 16
global user_fstest_start, user_fstest_end
%strcat fstest_file USER_BUILD, "/fstest-image.elf"
user_fstest_start:
    incbin fstest_file
user_fstest_end:
align 16
global user_shell_start, user_shell_end
%strcat shell_file USER_BUILD, "/shell-image.elf"
user_shell_start:
    incbin shell_file
user_shell_end:
align 16
global user_nettest_start, user_nettest_end
%strcat nettest_file USER_BUILD, "/nettest-image.elf"
user_nettest_start:
    incbin nettest_file
user_nettest_end:
align 16
global user_desktop_start, user_desktop_end
%strcat desktop_file USER_BUILD, "/desktop-image.elf"
user_desktop_start:
    incbin desktop_file
user_desktop_end:
align 16
global user_security_start, user_security_end
%strcat security_file USER_BUILD, "/security-image.elf"
user_security_start:
    incbin security_file
user_security_end:
align 16
global user_planner_start, user_planner_end
%strcat planner_file USER_BUILD, "/planner-image.elf"
user_planner_start:
    incbin planner_file
user_planner_end:
align 16
global user_agent_start, user_agent_end
%strcat agent_file USER_BUILD, "/agent-image.elf"
user_agent_start:
    incbin agent_file
user_agent_end:
align 16
global user_agenttest_start, user_agenttest_end
%strcat agenttest_file USER_BUILD, "/agenttest-image.elf"
user_agenttest_start:
    incbin agenttest_file
user_agenttest_end:
align 16
global user_agentinfer_start, user_agentinfer_end
%strcat agentinfer_file USER_BUILD, "/agentinfer-image.elf"
user_agentinfer_start:
    incbin agentinfer_file
user_agentinfer_end:
section .note.GNU-stack noalloc noexec nowrite progbits
