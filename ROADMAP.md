# AgenticOS roadmap

Implement phases in order. Each phase needs a build, relevant tests, a QEMU boot,
documented evidence and owner approval before starting the next phase. Earlier
test suites remain regression checks. Reserved folders are not completed work.

| Phase | Scope and completion evidence | Status |
| --- | --- | --- |
| 1 | Limine, x86_64 entry, linker, visible welcome, ISO; real QEMU boot | Verified: BIOS + UEFI |
| 2 | Initialization, INFO/WARNING/ERROR/DEBUG/PANIC logging, panic/assertions, CPU info, serial console, kernel configuration | Week 2 verified: BIOS/UEFI normal and fatal paths |
| 3 | Boot memory map, physical allocator, paging/VMM, heap, mappings and protection; allocation/free and invalid-access tests | Week 3 verified: host sanitizers + BIOS/UEFI memory matrix |
| 4 | GDT, IDT, ISRs, divide/invalid-opcode/GP/page-fault diagnostics, PIC/APIC as needed, measurable timer IRQs | Week 4 verified: PIC/PIT, host sanitizers + 18 BIOS/UEFI interrupt checks |
| 5 | PCB/TCB, contexts, cooperative then preemptive round-robin tasks, termination and synchronization | Week 5 verified: sanitized policy/rollback tests + 12 BIOS/UEFI scheduler checks |
| 6 | Ring 3, isolation, validated write/read/exit/getpid/yield syscalls and native user programs | Week 6 verified: native ELF/CPL3, host sanitizers + 12 BIOS/UEFI user checks |
| 7 | VFS, descriptors, RAM filesystem/initramfs, create/read/write/delete/list/rename; persistent format later | Awaiting approval |
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
Phase 4 replaces the earlier test-only page-fault gate with kernel-owned
GDT/IDT/TSS and a common ISR path. PIC/PIT is the tested single-CPU interrupt
backend; APIC/SMP remain future work. Exceptions report and halt, while IRQs and
the internal software round-trip check return with IRETQ. There is no general
exception recovery yet. Phase 5 adds guarded kernel-task stacks, PCB/TCB state,
cooperative and timer-preempted round robin, sleep/join/cancel and synchronization.
Kernel groups retain their shared CR3. Phase 6 user processes own separate
lower-half mappings, borrow supervisor kernel mappings, and enter at CPL 3.
Their syscall buffers and return contexts are checked; user faults are contained.

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
