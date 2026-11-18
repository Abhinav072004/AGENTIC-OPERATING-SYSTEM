# AgenticOS roadmap

Implement phases in order. Each phase needs a build, relevant tests, a QEMU boot,
documented evidence before starting the next phase. On 2026-11-18 the owner
authorized all remaining phases and sequential pushes to GitHub main. Earlier
test suites remain regression checks. Reserved folders are not completed work.

| Phase | Scope and completion evidence | Status |
| --- | --- | --- |
| 1 | Limine, x86_64 entry, linker, visible welcome, ISO; real QEMU boot | Verified: BIOS + UEFI |
| 2 | Initialization, INFO/WARNING/ERROR/DEBUG/PANIC logging, panic/assertions, CPU info, serial console, kernel configuration | Week 2 verified: BIOS/UEFI normal and fatal paths |
| 3 | Boot memory map, physical allocator, paging/VMM, heap, mappings and protection; allocation/free and invalid-access tests | Week 3 verified: host sanitizers + BIOS/UEFI memory matrix |
| 4 | GDT, IDT, ISRs, divide/invalid-opcode/GP/page-fault diagnostics, PIC/APIC as needed, measurable timer IRQs | Week 4 verified: PIC/PIT, host sanitizers + 18 BIOS/UEFI interrupt checks |
| 5 | PCB/TCB, contexts, cooperative then preemptive round-robin tasks, termination and synchronization | Week 5 verified: sanitized policy/rollback tests + 12 BIOS/UEFI scheduler checks |
| 6 | Ring 3, isolation, validated write/read/exit/getpid/yield syscalls and native user programs | Week 6 verified: native ELF/CPL3, host sanitizers + 12 BIOS/UEFI user checks |
| 7 | VFS, descriptors, RAM filesystem/initramfs, create/read/write/delete/list/rename; persistent format later | Week 7 verified: sanitized VFS + native BIOS/UEFI checks and user regression matrix |
| 8 | Extend tested QEMU drivers: keyboard, timer, storage, mouse, PCI, network; serial/display are initial prerequisites | Week 8 verified: real PS/2, ATA scratch I/O and 24-frame e1000 BIOS/UEFI tests |
| 9 | Shell parsing, helpful errors, system/process/file commands; move shell into user space | Week 9 verified: parser fuzz, native command batch, interactive BIOS/UEFI and user regression |
| 10 | Ethernet -> ARP -> IPv4 -> ICMP -> UDP -> TCP -> sockets, one tested layer at a time | Week 10 verified: packet sanitizers + native BIOS/UEFI peer, loss/retry, client/server and failure checks |
| 11 | Native framebuffer renderer, cursor, movable windows, taskbar, launcher, terminal/file manager/calculator | Week 11 verified: drawing/model sanitizers + real BIOS/UEFI graphical workflows |
| 12 | Permissions, syscall validation, process isolation, resource cleanup, error recovery and security review | Week 12 verified: UID/capability/quotas/orphan/malformed-call checks, sanitizers and regressions; limits reviewed |
| 13 | Native editor, calculator, file manager, system monitor, terminal and settings | Week 13 verified: real BIOS/UEFI apps, exact file/preference readback, full-quota save protection and sanitizers |
| 14 | Repeatable release builds, BIOS/UEFI regression coverage, manuals and explicit hardware-test procedure | Week 14 verified: complete core regression, identical clean builds/relocated ISO and exact packaged BIOS/UEFI boots |

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
verified, continue the owner-authorized sequence for:

1. **Verified (agent milestone 1):** typed intent/native tool contracts, bounded
   grammar, canonical workspace validation, 20,000 sanitized inputs and native
   zero-capability BIOS/UEFI acceptance.
2. Broker-enforced permissions, action validation and audit records.
3. A bounded user-space agent runtime with cancellation and resource limits.
4. A selected inference backend (local or networked), tested failure handling.
5. Native shell/desktop integration and reviewable multi-step workflows.

No model integration or provider choice is required for first boot. Keep the
OS functional without inference access. Physical installation always requires
separate explicit approval and must never overwrite an existing OS or disk.
