# Agent milestone 3 — bounded user-space runtime

Verified 2026-11-18 with `./scripts/env.sh make test`: strict freestanding cross
build, ELF validation, all host ASan/UBSan suites and real BIOS/UEFI boots pass.
Both firmware logs contain six `[agent-test] case=N status=0` records followed by
AGENT3 PASS and READY. Each test compares free physical frames and RAM filesystem
blocks before and after reaping; all return exactly to their starting counts.

The native worker uses a distinct CR3, CPL3, zero capabilities, 64 user CPU ticks,
2048 syscalls and a 60-second job deadline. Every worker proves direct file,
socket and self-approval calls are denied. It attempts an invalid user pointer
and a forged /etc proposal before submitting its valid plan. Kernel validation
rejects both while allowing the subsequent valid request.

Acceptance covers immutable review and approved two-step real file execution;
cancellation between two writes, with the second file absent; pending review
cancellation without effects; a deliberately shortened review deadline returning
ETIMEDOUT; owner exit while a worker is waiting for approval; and simultaneous
two-job capacity with rejection of a third. Forged worker calls from the owner,
invalid controller output pointers and stale released IDs are rejected.

The broker remains the only tool executor. Workers are never granted their
owner's capabilities. See [AGENT_RUNTIME.md](../AGENT_RUNTIME.md) for syscall
semantics, quotas, ownership and cleanup. No external inference is claimed yet.
