# Agent milestone 2 — permission broker and audit

Verified 2026-11-18. The kernel broker holds an immutable copy of each typed
plan, with two job slots and monotonic non-reused IDs. No action runs before
owner approval. Deletion needs a separate consent flag. Approval and every step
check the owner's current capability mask; actual VFS operations use that UID.
All tools remain within canonical /home paths and accept no executable code.

Writes use an exclusive temporary file and atomic replacement. Cancellation
stops future steps and reports the completed count; it does not undo earlier
steps. Read/list previews are bounded to 127 bytes. The RAM audit ring holds 64
metadata events, filters by owner PID and UID, and excludes file text. Old events
are overwritten with a dropped counter; no durable audit is claimed.

`./scripts/env.sh make test` passes the strict cross build, all ASan/UBSan host
suites and real BIOS/UEFI boots. The broker suite checks immutable copies,
unapproved execution, wrong identity, capability revocation, cancellation after
one step, explicit deletion consent, storage exhaustion preserving old content,
audit wrap/privacy, slot exhaustion and stale/overflowed IDs. Guest acceptance
executes create/write/read/delete against the real RAM VFS and restores its
transient block count before AGENT2 PASS and READY.

This is a trusted kernel component. The next milestone authenticates isolated
user workers and exposes checked syscalls; this milestone does not claim that
an untrusted model or user process can call the broker directly.
