# Contributing to AgenticOS

Use C17 for kernel code, NASM for x86_64 glue and the pinned Clang cross target.
Keep architecture operations under `kernel/arch/x86_64` and host tools outside
the kernel. Keep warnings as errors. Explain pointer ownership, boot invariants,
hardware assumptions and non-obvious assembly. Do not import a host runtime.

Work on a feature branch for a major subsystem. Complete one milestone,
run the relevant checks and QEMU regressions, update the roadmap/change log and
write a report with real evidence and remaining limits. Make a meaningful local
commit after verification. Pushing or publishing is a separate action.

Do not begin the next phase until the project owner approves it. A new phase
should explain the OS concepts and its acceptance criteria before expansion.
Do not count scaffolding, trivial zero-test passes or unexecuted checks as
completed features. Preserve unrelated files and never attach a host disk or
perform a physical installation without explicit permission.

This code is educational systems software, not a production-secure OS.
AI-generated plans must never become privileged execution by themselves.
