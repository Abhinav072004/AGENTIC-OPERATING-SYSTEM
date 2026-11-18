# Isolated agent runtime

A controller with CAP_AGENT_CONTROL starts a request using SYS_AGENT_START. The
kernel copies a zero-padded `agent_request` and reserves one of two job slots.
It loads the read-only /bin/agent ELF into a private address space, sets its
capabilities to zero, and binds its PID to this job. A normal spawn does not
acquire this binding. The worker cannot access files, devices, sockets, the
console input or process-spawn APIs, and cannot approve its own proposal.

The worker fetches its context, produces a typed plan and proposes it. The
kernel copies and validates the whole plan before changing PLANNING to REVIEW.
The controller obtains that immutable copy with REVIEW, examines STATUS and
explicitly APPROVEs it. The worker WAITs without busy-spinning, then requests
one atomic STEP at a time. The broker uses the controller PCB's current UID and
capabilities. Cancellation takes effect between atomic actions, preserving the
completed count and previous successful effects. There is no workflow rollback.

The worker has 64 user timer entries and 2048 syscalls. A job deadline is 6000
PIT ticks (about 60 seconds), including review. At most two jobs fit alongside a
controller in the current three-user-process system. Failed or completed jobs
remain reviewable until RELEASE, which reclaims a slot; IDs are never reused.
STATUS returns 1 on a copied record, 0 for an empty owner slot (id=0, slot in the
third argument), or a negative error. AUDIT returns the next owner-filtered event
after a sequence cursor, including released jobs. Only metadata is retained.

All public buffers are checked against the calling process's mappings. Worker
CONTEXT/PROPOSE/WAIT/STEP calls require the exact job/PID binding; controller
REVIEW/APPROVE/CANCEL/STATUS/RELEASE require owner identity and control authority.
Planning alone has no filesystem effects. Polling at syscall boundaries detects
deadlines, cancels/reaps finished workers, and process-exit hooks reclaim orphan
jobs after descendants. A terminated user process cannot retain an agent root,
file handle or runnable child. This single-CPU implementation serializes broker
mutations with interrupts disabled and does not retain user pointers.

No inference provider is part of this milestone. The worker currently uses the
explicit grammar in [AGENT_INTENTS.md](AGENT_INTENTS.md). Native shell/desktop
review controls follow in the integration milestone.
