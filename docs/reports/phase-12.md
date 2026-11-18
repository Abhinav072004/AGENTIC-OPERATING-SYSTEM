# Week 12 — permissions and containment

Verified 2026-11-18. Kernel-owned capabilities gate filesystem access, spawn,
network, inspection, display and console input. Files enforce owner/other/search
permissions and sticky-directory deletion/rename. The session is UID 1000;
UID 1001 tests sandbox denial. Children inherit identity/capabilities with display
removed. Root identity is scoped to trusted manager work; no setuid or capability
grant syscall exists. Experimental native ABI 2 adds ownership metadata.

Exit, fault, quota and cancellation cancel descendants. Off-current reaping
releases descendants before parent pages/roots/stacks/records. Optional CPU and
syscall budgets terminate offenders with -122. Process IDs stay in the positive
signed syscall range. Invalid calls return errors and do not poison later work.

Executed successfully:

- `./scripts/env.sh make test`: strict builds, ELF validation, all host sanitizer
  suites, new UID/mode/search/sticky tests and default BIOS/UEFI acceptance.
- `./scripts/env.sh make test-security`: final ABI-2 BIOS/UEFI boots check six
  native cases: denied capabilities, UID/sticky restrictions, orphan cleanup,
  syscall-budget exhaustion, actual CPU-budget exhaustion and malformed syscall
  arguments followed by a successful valid open. Each restores exact physical
  frames and transient filesystem blocks; only cases 3/4 exit with -122.
- `./scripts/env.sh make test-user`: 12 BIOS/UEFI checks pass after core policy
  changes: real CPL3/CR3, low-RAM BIOS, stress/OOM, eight contained attacks,
  UART and required-image/readiness rejection.
- `./scripts/env.sh make test-desktop` and `make test-network`: both firmware
  workflows pass with the new capabilities, UID checks and lifecycle policy.
- Final `./scripts/env.sh make test-shell`: BIOS/UEFI real serial sessions
  additionally chmod a file to 000, confirm read denial, restore 600, read the
  contents and verify `id` reports UID 1000; existing file/child/error tests pass.
- `git diff --check`: clean.

Evidence: `build/tests/{bios-security,uefi-security}/serial.log` records all six
statuses and frame counts; `shell-*`, `user-*`, `desktop-*` and `network-*` hold
independent regression evidence. The final normal image includes ABI-2 checks.

Default artifact SHA-256:

```text
ea5fa4dcb45917a99efc82fbd2cfce79e9a999d301fd9a9fa0c3829fb46ec15f  build/agenticos.iso
ecf771cf6af260193ea63534eaba83bcb65a0715a50104527a7393543701aa76  build/kernel.elf
```

The boundary review is [SECURITY.md](../SECURITY.md). Limits include one CPU,
coarse user CPU accounting, shared-process desktop built-ins, no account/login
system or groups, no persistent permissions, TLS or speculative mitigations.
This milestone is tested containment on the documented QEMU target, not a claim
of production security or an independent third-party audit.
