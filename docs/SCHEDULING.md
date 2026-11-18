# Week 5: tasks, contexts and synchronization

AgenticOS now runs multiple kernel tasks on the bootstrap CPU. They have
independent guarded stacks and saved CPU contexts, and can cooperate or be
preempted by the PIT. This chapter describes the Week 5 kernel-thread contract.
Week 6 reuses these contexts for [ring-3 native programs](USER_MODE.md), adding
private user mappings and checked syscalls. There is no AI runtime yet.

## PCB and TCB ownership

A process control block (PCB) identifies a group and its retained thread count.
Four PCB slots include the permanent kernel group, PID 1. Kernel groups borrow
the same kernel CR3; user PCBs bind an owned user root. The user-process manager
destroys that root/pages before releasing the PCB; bare task_join is reserved
for kernel tasks, and user reaping goes through that manager.
Thread counts include zombies until their resources are joined and reclaimed.
A group cannot be destroyed while any retained thread remains.

A task control block (TCB) records TID/PID, state, saved-frame pointer, wait
reason, wake tick, dispatch/runtime counters, join owner and held mutex count.
There are 16 TCB slots: slot 0 is the bootstrap/idle context, leaving 15 worker
slots. PIDs and TIDs increase independently of slot numbers. Reusing a slot does
not make an old handle identify its new occupant; UINT64_MAX is reserved and
ID exhaustion fails rather than wrapping. This is trusted kernel bookkeeping,
not authenticated handle access from user mode.

`task_spawn` borrows a C entry function and argument until completion. The caller
must keep referenced argument data alive. Entry functions may call task APIs and
return normally (exit status 0), or call `task_exit(status)`. Control blocks and
stack descriptors are fixed BSS tables, not heap allocations.

```text
UNUSED --spawn--> READY --dispatch--> RUNNING
                    ^                   |
                    |----yield/quantum--|
                    |                   |
                    |----wake----- BLOCKED (sleep, join, mutex, semaphore)
                                        |
RUNNING/READY/BLOCKED --exit/cancel--> ZOMBIE --off-stack join--> UNUSED
```

## One frame format for yield and preemption

The Week 4 common ISR saves all 15 GPRs plus vector/error and the CPU's
RIP/CS/RFLAGS/RSP/SS frame. Week 5 changes the dispatcher to return the frame
pointer to restore. The stub finishes its C call on the old stack, changes RSP
to that pointer, restores the selected registers and executes IRETQ.

Vector 129 is the DPL-0 software yield gate. Blocking/exit code changes its TCB
with IRQs disabled and enters that gate to switch away. PIT IRQ0 can select a
different frame after acknowledging the PIC. The same 176-byte layout handles
both paths; no separate partial-register coroutine ABI is used.

A new task starts from a constructed frame: kernel code/data selectors, IF set,
a C trampoline RIP and an ABI-aligned stack pointer. The trampoline invokes the
borrowed entry and exits if it returns. No privileged host context switch is
executed by host tests; actual entry/IRET behavior is exercised in QEMU.

The GPR-only compiler configuration, no red zone and cleared live DF remain
required. Assembly probes set unique values with bit 40 in all GPRs and verify
registers, RSP and flags after switching. The preemption probe never executes
yield: it spins with those values live until another task sets a signal. That
task can run only after a real timer-driven switch. DF is restored for the
interrupted assembly and cleared before returning to C.

Week 6 selects a user PCB's CR3 and sets TSS.RSP0 to its kernel stack before
returning to CPL 3. Shared supervisor upper mappings keep both the old and new
kernel stacks accessible while the dispatcher finishes switching. FS/GS bases
are zero; FPU/SIMD instructions trap until their contexts are implemented.

## Round robin, sleeps and idle

Cooperative mode switches on explicit yield/block/exit. Timer ticks can wake
sleepers, but do not forcibly rotate a running cooperative worker. Preemptive
mode rotates ready workers after a tick quantum, normally two ticks near
100 Hz (approximately 20 ms). The stress image also exercises a one-tick quantum.
The portable policy scans the bounded task table in circular order, skipping
blocked/zombie/unused entries. The idle context is selected only when no worker
is ready, so it does not consume a normal worker quantum.

The bootstrap `scheduler_run` loop saves its own context on the first switch.
When every live worker blocks, the loop sleeps with adjacent `sti; hlt; cli`.
A later tick wakes sleepers and can switch directly from idle to a ready task.
When no live worker remains, the loop masks the PIC and returns with IF clear.
Zombies can still retain resources until join; completion and reclamation are
separate operations.

`task_sleep` requires a positive delay that does not overflow the tick counter.
It records a deadline while IF is clear, then blocks atomically with switching.
A sleeper becomes ready at or after that tick; queueing can delay its actual
execution. These are logical PIT ticks, not calendar time or calibrated delays.
Counter overflow is rejected rather than silently changing deadline ordering.

Runs have a watchdog measured in delivered timer ticks. Boot exercises allow
500 ticks; an isolated runaway fixture allows eight and panics when exceeded.
This detects failure/deadlock and halts, not recovery or forced cleanup. It
cannot detect a task that permanently disables IRQs; the host boot deadline
is the remaining check for that case. Arbitrary ring-0 tasks are trusted.

## Guarded stacks and termination

Each worker slot owns four 4 KiB physical frames mapped contiguously RW/NX at
`0xffffd20000000000 + slot * 0x6000 + 0x1000`. One unmapped page lies below and
above the 16 KiB body. The allocator rejects an occupied body or either occupied
guard before allocating. Failure at any allocation/map stage rolls back owned
pages; unmap also releases empty page tables through the existing VMM.
Backend release/unmap must succeed for owned resources.

A lower-body canary and saved-frame/RSP bounds are checked at task switches.
Guards do not catch every object overflow or use-after-free; direct-map aliases
of physical frames still exist. This is kernel error detection, not isolation.

Exit makes a zombie and wakes its joiner without freeing the current stack.
Another context joins it, consumes the exit status, unmaps the stack, frees the
frames/tables, decrements the PCB count and releases the TCB slot. Freeing a
currently executing stack would invalidate the dispatcher itself, so it is
explicitly forbidden. Normal boot compares frame, page-table and mapping counts
before creation and after all joins; every round must restore all three.

Join permits one claimant, rejects self/idle joins and follows blocked join
dependencies to reject cycles. A worker can block until its target exits; the
bootstrap caller can reap completed targets after a run. Cancellation can stop
a noncurrent ready/blocked task, producing TASK_CANCELLED. It detaches any join
claims, including those of a waiter cancelled after wakeup. Stale handles,
double joins, current-task cancellation and cancellation of mutex owners fail.
There is no arbitrary asynchronous termination of lock-owning kernel code.

## Synchronization on one CPU

Task lifecycle, queue edits, stack allocation and synchronization use IRQ save/
restore. Saving IF, disabling local interrupts and restoring the prior IF makes
updates atomic with respect to this CPU's timer handler. Nested callers that
already had IF clear leave it clear. This is not an SMP spinlock or an NMI lock.
The heap's kmalloc/kfree wrappers now use the same rule; raw PMM/VMM/heap clients
must still serialize their operations. The normal IRQ path does not allocate or log.
Ordinary task logging is not made reentrant by these changes.

Mutexes track one owner and wake the oldest blocked waiter by wait ticket.
Unlock transfers ownership before making that waiter runnable; a new caller
cannot steal the handoff. Recursive lock and nonowner unlock fail. A held-lock
count prevents cancellation of an owner, including one that has received a
handoff but has not resumed yet. Exiting with held mutexes is a fatal invariant
violation. General lock ordering/deadlock recovery and priority inheritance are
not implemented.

Semaphores have a bounded count. Wait checks/decrements under IRQ protection
or blocks while zero. Post rejects overflow and wakes all waiters to recheck
the count. Waking all prevents cancellation of one awakened task from stranding
a posted token; it does not guarantee strict FIFO semaphore acquisition. These
objects are borrowed: initialize only fresh objects and keep them alive until
all owners/waiters have finished. Reinitializing an in-use object is invalid.

The synchronization exercise holds a mutex across a yield, forcing a contender
to block, verifies eight protected updates, consumes two posted tokens, sleeps
through idle, joins a newly spawned child, cancels a blocked sleeper and checks
that its old TID cannot affect a replacement task. Two more workers attempt a
join cycle: rejection lets the second exit and the first consume its result.

## Tests and remaining scope

Sanitized host checks run the actual PCB/state/stack policy: capacity and ID
exhaustion, round-robin fairness, cooperative versus quantum behavior, sleep,
wait-object isolation, join wake/cancellation, off-stack reap and all eight
stack allocation/map rollback points, including physical frame zero.

The scheduler matrix contains 12 QEMU checks: six normal firmware/RAM runs,
two stress runs (four rounds, one-tick preemption, real physical-frame OOM and
recovery), two real task-guard faults, and two runaway-watchdog failures. Fatal
fixtures must never emit PHASE5. A guard-fault image that passed PHASE4 must fail
normal readiness. Existing memory, interrupt, diagnostic and debugger suites
remain regression checks. See [the acceptance report](reports/phase-5.md).

Scheduling ends in the documented final halt after kernel and user self-checks.
There is no persistent service loop, APIC/SMP balancing, priority scheduler or
AI runtime. User mode, private lower mappings and the basic syscall ABI now
exist. Physical hardware remains unverified; the filesystem milestone awaits
owner approval.
