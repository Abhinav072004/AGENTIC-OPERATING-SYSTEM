# AgenticOS roadmap

Implement phases in order. Each phase needs a build, relevant tests, a QEMU boot,
documented evidence and owner approval before starting the next phase. Earlier
test suites remain regression checks. Reserved folders are not completed work.

| Phase | Scope and completion evidence | Status |
| --- | --- | --- |
| 1 | Limine, x86_64 entry, linker, visible welcome, ISO; real QEMU boot | Verified: BIOS + UEFI |
| 2 | Initialization, INFO/WARNING/ERROR/DEBUG/PANIC logging, panic/assertions, CPU info, serial console, kernel configuration | Week 2 verified: BIOS/UEFI normal and fatal paths |
| 3 | Boot memory map, physical allocator, paging/VMM, heap, mappings and protection; allocation/free and invalid-access tests | Week 3 verified: host sanitizers + BIOS/UEFI memory matrix |
| 4 | GDT, IDT, ISRs, divide/invalid-opcode/GP/page-fault diagnostics, PIC/APIC as needed, measurable timer IRQs | Awaiting approval |
| 5 | PCB/TCB, contexts, cooperative then preemptive round-robin tasks, termination and synchronization | Planned |
| 6 | Ring 3, isolation, validated write/read/exit/getpid/yield syscalls and native user programs | Planned |
| 7 | VFS, descriptors, RAM filesystem/initramfs, create/read/write/delete/list/rename; persistent format later | Planned |
| 8 | Extend tested QEMU drivers: keyboard, timer, storage, mouse, PCI, network; serial/display are initial prerequisites | Planned |
| 9 | Shell parsing, helpful errors, system/process/file commands; move shell into user space | Planned |
| 10 | Ethernet -> ARP -> IPv4 -> ICMP -> UDP -> TCP -> sockets, one tested layer at a time | Planned |
| 11 | Native framebuffer renderer, cursor, movable windows, taskbar, launcher, terminal/file manager/calculator | Planned |
| 12 | Permissions, syscall validation, process isolation, resource cleanup, error recovery and security review | Planned |
| 13 | Native editor, calculator, file manager, system monitor, terminal and settings | Planned |
| 14 | Repeatable release builds, BIOS/UEFI regression coverage, manuals and explicit hardware-test procedure | Planned |

Some foundations overlap: minimal UART and framebuffer output are needed to
verify Phase 1. Phase 2 adds the diagnostic framework; general device drivers
remain future Phase 8 work.
Phase 3 protection probes use a test-only page-fault gate and halt after an
expected violation. The default kernel has no general exception recovery;
production GDT/IDT and interrupt ownership remain Phase 4 work.

## Agentic milestones

The AI objective extends this OS roadmap. After the required core phases are
verified, propose a separate, owner-approved sequence for:

1. User intent and deterministic native tool interfaces.
2. Broker-enforced permissions, action validation and audit records.
3. A bounded user-space agent runtime with cancellation and resource limits.
4. A selected inference backend (local or networked), tested failure handling.
5. Native shell/desktop integration and reviewable multi-step workflows.

No model integration or provider choice is required for first boot. Keep the
OS functional without inference access. Physical installation always requires
separate explicit approval and must never overwrite an existing OS or disk.
