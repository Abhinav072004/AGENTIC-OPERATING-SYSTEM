# AGENTICOS

A custom, bootable x86_64 operating system, developed one verified milestone at
a time. The long-term goal is an **agentic OS**: permission-controlled AI agents
working through native OS services. AgenticOS has its own C17 kernel and uses
Limine to boot it; it does not use a Linux or Windows kernel.

**Current milestone: Phase 1 — first boot, verified in QEMU BIOS and UEFI.**
The kernel displays `Welcome to AgenticOS`, emits serial test evidence, and halts.
There is no AI runtime, shell, scheduler, filesystem, networking, or desktop yet.
Phase 2 requires the project owner's approval before implementation.

## Build and test

See [environment setup](docs/SETUP.md) for prerequisites and pinned versions.
On the prepared Debian cloud environment:

```sh
cd /workspace/AGENTIC-OPERATING-SYSTEM
./scripts/setup-debian.sh                 # first setup, or refresh local tools
./scripts/env.sh make -j2                # build/astraos.iso and build/kernel.elf
./scripts/env.sh make test               # ELF, sanitized host, BIOS and UEFI tests
./scripts/env.sh make test-debug         # verify GDB can break at kernel_main
./scripts/env.sh make run                # BIOS, serial on terminal; Ctrl+C to exit
./scripts/env.sh make run-uefi           # UEFI equivalent
```

The default runner is headless, suitable for a cloud host. The kernel still
draws into a real emulated framebuffer; boot tests capture it to
`build/tests/{bios,uefi}/screen.png`. On a local Linux desktop with QEMU's GTK
backend installed:

```sh
./scripts/env.sh python3 tools/qemu.py --display gtk
```

No disk image or host block device is attached. Only the ISO and, for UEFI,
a temporary copy of firmware variables are used. Physical installation is out
of scope for this milestone.

## Project map

```text
boot/                    Limine config and dependency lock
kernel/
  kernel.c               Boot-protocol checks and first-boot sequence
  arch/x86_64/           Assembly entry, port I/O, UART, linker memory layout
  drivers/display/       Bounded framebuffer welcome renderer
  memory/                Reserved for Phase 3
  interrupts/            Reserved for Phase 4
  scheduler/, process/   Reserved for Phase 5
  syscall/               Reserved for Phase 6
  filesystem/            Reserved for Phase 7
userland/
  shell/, applications/  Future native processes
  agents/                Future isolated agent services
libraries/               Reserved for shared kernel/user libraries
scripts/                 Setup, dependency fetching, tool activation, GDB commands
tools/                   QEMU launcher
tests/                   Host, ELF, boot and debugger checks
docs/                    Setup, internal explanations and milestone reports
build/                   Generated ELF, ISO and evidence (ignored by Git)
.cache/                  Verified third-party Limine assets (ignored by Git)
```

Drivers live under `kernel/drivers` rather than a second competing top-level
driver tree. Architecture-specific UART/CPU operations remain in `arch/x86_64`.
Reserved directories contain no claimed implementations.

## Read next

- [Architecture and AI boundary](ARCHITECTURE.md)
- [Milestones and approval gates](ROADMAP.md)
- [How the first boot works](docs/FIRST_BOOT.md)
- [Actual Phase 1 results and limitations](docs/reports/phase-1.md)
- [Test commands and debugging](TESTING.md)
- [Contributing](CONTRIBUTING.md) · [Change log](CHANGELOG.md)

Original repository license: [Apache-2.0](LICENSE). Limine assets and its protocol
header retain their upstream licenses; see [third-party notices](docs/THIRD_PARTY.md).
