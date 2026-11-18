# Week 9 — native shell

Verified 2026-11-18. The command parser and shell execute in a native ELF at
CPL3. Kernel services expose copied system/task records, checked file operations,
child ELF spawn, parent-restricted wait/kill and timed sleep. The scheduler can
create children and reap inactive zombies while a parent remains runnable.
Names are owned by process records; syscall-local paths are never retained.

Executed successfully:

- `./scripts/env.sh make test`: strict build/ELF validation, all sanitized host
  suites, 10,000 bounded parser inputs, quotes/escapes/empty arguments, integer
  overflow and root traversal; default BIOS/UEFI native command batches reach
  PHASE9 with child output, zero exit statuses and exact physical-frame recovery.
- `./scripts/env.sh make test-shell`: actual bidirectional serial shell sessions
  on BIOS and UEFI; quoted filenames, create/write/read/rename/stat/delete,
  process inspection, `/bin/hello` spawn/wait, malformed quotes, oversized input,
  helpful errors and clean exit. Evidence: `build/tests/shell-{bios,uefi}`.
- `./scripts/env.sh make test-user`: 12 BIOS/UEFI cases pass, retaining real CPL3
  capture, low-RAM BIOS, stress/partial OOM, eight contained attacks, UART input,
  required-image failure and negative normal-readiness check.

The default image uses a bounded automated shell batch; `make shell` selects an
interactive ISO that sleeps while awaiting serial input and runs until `exit`.
Default boot evidence: `build/tests/{bios,uefi}`. Both tested native children
complete, their private roots/stacks are freed, and the kernel reaches final HLT.

Default artifact SHA-256:

```text
36953cc1d42532918dfe142c0dbf39a494be73e34ca28fe09a1200a824675e42  build/agenticos.iso
2618296fceed4a8226958fceacacac965aac989040db0ed86dabdac0aab3c4d2  build/kernel.elf
```

Limits: 255-byte lines, 16 arguments, volatile filesystem, three simultaneous
user processes, serial console and synchronous `run`. No POSIX pipelines,
redirection, expansion or general script language. Broader permission/orphan
hardening is reserved for Phase 12. See [SHELL.md](../SHELL.md).
