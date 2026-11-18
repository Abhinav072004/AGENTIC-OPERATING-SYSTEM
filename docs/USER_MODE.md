# Week 6: native user programs and the syscall boundary

AgenticOS now executes freestanding C ELF programs at x86 privilege level 3.
Each user process owns lower-half page tables and RAM; supervisor kernel
mappings remain available during kernel entry. The scheduler can preempt user
code, resume it under its CR3, and terminate a faulty program while another
process finishes. Boot exercises these paths, reclaims their resources and
returns to the final kernel halt.

## Native image format and loading

`userland/runtime` supplies entry assembly, a linker script and inline syscall
wrappers. `hello.c` and `probe.c` are cross-compiled with the freestanding Clang
target and linked without a host runtime. Their native ELF files are embedded
as read-only kernel assets until a filesystem exists. The kernel loads them as
program images; it does not call their code at ring 0.

The loader accepts little-endian x86_64 static ET_EXEC ELF64 with at most eight
program headers. LOAD starts/file offsets must be 4 KiB aligned with 4 KiB
alignment, lie within `0x400000..0x800000`, and have valid file/memory lengths.
Page-rounded overlaps and W+X segments are rejected. The entry must lie in
file-backed executable bytes. Dynamic/interpreter/TLS images and executable
stack requests are rejected. This is deliberately a bounded image ABI.

Each allocated page is zeroed, file bytes copied and the rest left zero,
including BSS and page padding. A process owns at most 64 user data pages,
including stack, info and optional witness pages. Ordinary allocation/map OOM
rolls back owned pages/tables before releasing its kernel task and PCB. The
caller publishes PID/TID outputs only after complete construction.

The native entry receives its PID in RDI and a kernel-selected test argument in
RSI. It aligns the user stack, calls C `user_main`, and submits its return value
to SYS_EXIT. There is no argc/envp, dynamic linker, user heap, fork or exec ABI.

## Page ownership and address layout

| Address | Role and permission |
| --- | --- |
| `0x400000..0x800000` | ELF segments: RX instructions, RO/NX constants, RW/NX data |
| `0x50000000` | Private read-only/NX info page: ABI version, PID, delivered user-runtime ticks |
| `0x60000000` | Optional private witness page used by the isolation checks |
| `0x6ffff000` | Unmapped lower user-stack guard |
| `0x70000000..0x70004000` | Four RW/NX user-stack pages |
| `0x70004000` | Unmapped upper user-stack guard |
| Upper canonical half | Borrowed supervisor-only kernel mappings |

Two programs can use the same virtual data address while reaching different
physical frames. The kernel root is kept alive, and only upper supervisor
subtrees are borrowed into each user root. Lower user trees/root and data pages
are owned and reclaimed independently. Map/protect/unmap reject edits to
borrowed upper mappings, and child destruction never traverses/frees them.

The kernel task stack is allocated before cloning the upper root entries.
All worker slots lie inside that existing shared stack subtree, so the old
and selected kernel stacks remain mapped while C finishes a CR3 switch.
Lifecycle edits are serialized on the bootstrap CPU; the borrowed kernel
mappings must remain valid for a runnable child. This is not arbitrary table
sharing, SMP reference counting or copy-on-write.

Every user leaf and its parents must permit USER; effective write permission
also requires all levels, and NX at any level forbids execution. The public VMM
query now reports these effective permissions. Kernel direct-map aliases are
supervisor-only. Unmapping user RAM does not remove those kernel aliases.

## Privilege entry, context switching and CPU state

The GDT retains kernel code/data `0x8`/`0x10` and TSS `0x18`, then adds user
code/data `0x2b`/`0x33`. It is seven slots with inclusive limit 55. The scheduler
sets the selected CR3 and TSS.RSP0 before restoring a user frame. IRETQ loads
the user selectors, instruction, stack and IF. On a user interrupt or syscall,
the CPU switches to that task's supervisor kernel stack before the common ISR
saves registers and calls C.

Only the syscall gate at vector 130 (`0x82`) has DPL 3. Kernel gates such as
yield vector 129 retain DPL 0. User attempts to invoke them produce a general
protection fault rather than entering their kernel ABI. IOPL stays zero, and
the TSS I/O bitmap is beyond its limit, so user I/O and CLI remain restricted.

User RIP/RSP must be canonical lower-half addresses before IRET. The return
policy keeps arithmetic flags and DF, forces IF/reserved bit 1, and removes
IOPL, NT, TF, VM and AC. An invalid return context terminates that process,
preventing a user-selected noncanonical RSP from causing a kernel IRET fault.
Unmapped but canonical user pointers still fault normally in user mode.

The current ABI saves GPRs. CR0.EM/TS make FPU/MMX/SIMD instructions trap until
their context handling exists, preventing unsaved register state from crossing
processes. User FSGSBASE/RDPMC are disabled; FS/GS/KERNELGS bases are explicitly
zero. Native code uses `-mgeneral-regs-only`, no red zone and no TLS. This is an
integer-program ABI, not complete floating-point or general compiler-runtime
support.

The read-only info page counts timer entries while that process is executing
at CPL 3. It is a logical runtime counter, not wall-clock time. Native checks
spin without yielding until six such ticks arrive, proving actual preemption.
The host additionally captures CPU registers while CPL 3 is live.

## Syscall ABI

Invoke `int 0x82` with RAX selecting the call, RDI/RSI/RDX carrying arguments,
and receive a signed result in RAX. Other saved GPRs return unchanged. The gate
clears live IF, and the common entry clears DF and aligns the C stack. User code
never supplies a kernel frame pointer or kernel-stack address.

| Number | Call | Arguments | Result |
| --- | --- | --- | --- |
| 1 | write | fd, source, count | Accepted count, or negative error |
| 2 | read | fd, destination, count | Bytes received; 0 means currently unavailable |
| 3 | exit | signed 32-bit status | Terminates the current user task; invalid status returns EINVAL |
| 4 | getpid | none | Current PCB PID |
| 5 | yield | none | Switch if another worker is ready; returns 0 when resumed |

Write accepts fd 1/2 and read fd 0. The per-call limit is 128 bytes. Unknown
numbers return ENOSYS (-38); bad fds EBADF (-9), excessive lengths E2BIG (-7),
bad buffers EFAULT (-14), and invalid exit status EINVAL (-22). A valid-fd
zero-length transfer returns 0 without dereferencing its pointer. Descriptor
validation precedes size/buffer validation. There is no general file table yet.

Write emits a structured INFO console record, converts nonprintable bytes to
`?`, and appends kernel-controlled metadata. Formatting never treats user text
as a format string. Filtering and bounded UART delivery apply to these records;
this is diagnostic text output rather than a binary terminal stream. Read polls
COM1 without blocking and consumes/discards errored UART bytes as before.

The bidirectional test exposed dropped characters under the old 100,000-spin
transmit bound. Transmit now has a separate bounded 1,000,000-spin budget;
loopback initialization retains its original bound. The complete socket I/O
test then passed on BIOS and UEFI. These are iteration limits, not calibrated
wall-clock guarantees or hardware reliability claims.

## User copies and lifetime

Before I/O or copying, every page of the entire requested user range is checked:
canonical lower-half bounds, overflow, presence, effective USER and, for output
to user, WRITE. Copying translates the virtual pages and accesses their stable
physical RAM through supervisor aliases. The kernel never dereferences the
unchecked user virtual address directly. A rejected cross-page destination is
not partly modified, and a bad read destination is rejected before UART input
is consumed. Address-space edits cannot race these copies in the current
single-threaded-process/IRQ-disabled syscall model.

Each user PCB currently has one task. Process creation/reaping is a trusted
bootstrap-manager operation, with at most three user PCBs alongside PID 1.
User pages/root are freed after the task becomes a zombie and CR3 has returned
to the kernel. The manager then reaps the kernel stack/TCB and releases the PCB.
Bare kernel task_join refuses user tasks to keep that ownership order intact.

## Fault containment and tests

Ordinary CPL3 exceptions terminate the offending task with status
`-256 - vector`; bad return context uses -300. The handler logs a bounded
WARNING and selects another context. Kernel faults, NMI, double fault and
machine check retain their fatal policy. This is process containment, not
resuming a faulting instruction or repairing damaged memory.

The default tests run native hello, two private copies of the same program,
and eight offender/guardian pairs: kernel read, peer-only page read, read-only
write, NX fetch, CLI, kernel-gate INT, SIMD and noncanonical return stack. Each
offender must be reaped with its expected status and its guardian must finish.
Every user page/root must be reclaimed, with the original kernel frame/table/
mapping counts restored. Native checks also exercise bad fds, unknown calls,
overflow/NULL/kernel/straddling/readonly buffers, zero length and maximum writes.

Host ASan/UBSan checks validate actual native ELF input and fourteen malformed
variants, effective parent permissions, copy bounds, safe return flags, BSS,
private roots and every loader allocation budget from 0 through 32. Guest
stress adds four rounds and a real partial-load OOM rollback. A UART fixture
receives `Z` through SYS_READ and echoes it through SYS_WRITE. An invalid required
boot image must fail normal readiness despite PHASE5 completion. See
[the acceptance report](reports/phase-6.md) for final evidence and hashes.

The OS still ends in a final halt after its boot exercises. There is no general
user process creation API, persistent service loop, filesystem, signal ABI,
networking, desktop or AI runtime. APIC/SMP, demand paging, dynamic loading,
FPU/SIMD contexts and production security require further milestones. Week 7's
VFS and RAM filesystem await owner approval.
