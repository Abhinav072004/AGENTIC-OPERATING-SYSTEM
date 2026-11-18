# Permissions, containment and resource policy

AgenticOS targets one serialized QEMU CPU. User code executes at CPL3 with
private lower page trees, supervisor kernel mappings, W^X, WP/NX and guarded
stacks. The syscall gate checks return contexts and copies complete user buffers
before mutation. Ordinary user faults terminate the offender; kernel faults
still panic. This is educational systems software, not a production security
boundary for hostile software or a claim of a security audit by a third party.

Experimental user ABI 2 extends file metadata with owner UID. Rebuild native
programs; stable binary compatibility is not claimed. `id` shows the session
identity/capabilities and `chmod OCTAL_MODE PATH` changes owned file permissions.

Each process has a kernel-owned UID and capability mask. Boot launches the local
session as UID 1000; tests also use UID 1001. There is no login/account database,
setuid syscall or user-controlled capability grant. Children inherit the
creator's UID and capabilities with display permission removed. Only the boot
manager grants a compositor display access. Filesystem read/write, spawn,
network, inspection, display and console input are independently checked before
syscall dispatch. Wait/kill additionally require direct parent ownership.
Structured stdout stays available for diagnostics and is sanitized; raw terminal
output is a separate manager-granted stream.

RAM files have owner UID and mode bits. Owner and other read/write/execute/search
bits are enforced; groups are not implemented. The root UID is used only by
trusted boot/manager code. `/bin` and `/etc` are root-owned 0755 directories;
programs are 0755, configuration 0644. `/home` belongs to UID 1000 with mode 0700.
`/tmp` is 01777, enforcing sticky-owner rules for deletion and rename. New files
are 0600, directories 0700. Chmod requires ownership; chown requires root.
A descriptor retains access granted when opened, while per-call capability
checks still apply. Unlinked files release data at final close.

The RAM VFS scopes UID to each syscall while interrupts are disabled and resets
to the manager identity on every return. These routines neither sleep nor
schedule. Program spawn reads and checks execute permission as the creator,
then resets identity before allocating/loading its private address space.
No mutable UID context may be used concurrently; SMP needs a different design.

Optional per-process CPU and syscall budgets terminate an offender with -122.
CPU accounting charges timer entries while executing CPL3; syscalls charge a
separate counter including rejected requests. Every privileged operation is
bounded, but these counters do not claim precise kernel CPU-time accounting.
Existing fixed limits bound processes, user image pages, descriptors, RAM data,
sockets, receive buffers, drawing geometry and copied transfer sizes.

Exit, fault, quota termination and cancellation recursively cancel descendants.
Off-current reaping releases their descriptors, sockets, user pages, roots,
guarded kernel stacks and PCB/TCB records before the parent. Children cannot
survive an exited parent as unowned daemons. Process IDs stop before the signed
syscall return range is exhausted.

The native security acceptance covers denied capabilities, UID/search/sticky
permissions, orphan cleanup, runaway syscall/CPU termination, malformed syscall
arguments and successful work after errors. It checks exact frame and transient
filesystem-block recovery. Existing page/privilege attack tests, packet fuzzing,
renderer guards and user-copy sanitizers remain regression checks.

Review limits: no SMP, KPTI, SMEP/SMAP policy, speculative-execution mitigation,
cryptographic networking/TLS, hardware isolation, multi-user authentication or
persistent permission database. The kernel/compositor/broker implementation is
trusted code. Built-in desktop windows share one user process. Inference or
agent output must pass an independent action broker; model text alone must never
change these permissions or execute privileged actions.
