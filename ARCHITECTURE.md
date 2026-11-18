# AgenticOS architecture

AgenticOS has a custom C17/NASM x86_64 kernel booted by pinned Limine. The tested
machine is one QEMU q35 CPU. The kernel owns memory, descriptors, interrupts,
scheduling, files, device/network state and capability checks. Native apps use
an explicit ELF/syscall ABI and do not import a host runtime.

```mermaid
flowchart TB
  Boot[Limine BIOS or UEFI] --> Kernel[Kernel initialization and acceptance]
  Kernel --> Memory[PMM / page tables / guarded stacks / heap]
  Kernel --> Tasks[PIC/PIT / scheduler / private user roots]
  Apps[Native shell and desktop apps at CPL3] --> Gate[Validated syscall gate and copied buffers]
  Gate --> Policy[Capabilities / UID and ownership checks / budgets]
  Policy --> VFS[RAM VFS and descriptors]
  Policy --> Devices[PCI / PS2 / ATA / e1000]
  Policy --> Network[Bounded IPv4 and sockets]
  Policy --> Display[Clipped drawing and supervisor back buffer]
  Tasks --> Apps
```

The kernel's upper mappings are supervisor-only. Every user process owns its
lower page tree and leaf pages; upper kernel subtrees are borrowed for the
kernel owner's lifetime. Code is RX, data RW/NX, and stacks have guards.
TSS.RSP0 and CR3 follow task selection. Every ISR saves a normalized register
frame; scheduling selects the frame returned by IRETQ. FPU/SIMD is deliberately
trapped because this ABI does not save that state. The target is integer code.

Syscalls check the actual running process/root and return context. User pointers
are validated over their entire range and copied through stable physical aliases.
Each process owns descriptors and sockets. Exit/fault/quota/cancel recursively
cancels descendants; off-current reaping releases their roots, leaves, stacks,
descriptors, sockets and PCB/TCB records. Fixed pools and optional CPU/syscall
budgets bound resources. See USER_MODE.md, SCHEDULING.md and SECURITY.md in docs.

The VFS stores RAM inodes/data, owner/mode/search/sticky permissions, independent
file offsets and open-unlink lifetimes. Exclusive temporary creation and atomic
regular-file replacement support safe editor saves. Files, settings and runtime
records are volatile. ATA is a tested block driver, not a mounted persistent
filesystem. PS/2 and e1000 use bounded polling; the network stack implements a
small static IPv4/socket subset documented in NETWORK.md.

The desktop's window model and built-in apps run in one private user process.
Its explicitly granted display capability permits copied drawing/input commands;
it never receives a framebuffer mapping. A supervisor back buffer is zeroed,
presented and freed at session exit. Built-in views share their process; child
programs launched by Terminal receive private roots and lose display permission.

The core roadmap is followed by owner-authorized agent milestones: typed intent
and tools, an independent action broker, a bounded user-space worker, a selected
inference backend and reviewable shell/desktop workflows. Model text must remain
data and cannot grant privileges or directly become kernel execution. The OS
remains useful offline. Current agent status is recorded in ROADMAP.md.

Build/release tooling lives on the host. It is separate from the guest kernel:
source versions, normalized paths, compact runtime images and fixed ISO metadata
produce repeatable images; QEMU and independent host tests collect evidence.
Physical hardware, SMP, production security, persistent storage, TLS and broad
hardware compatibility are outside the verified core release.
