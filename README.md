# AGENTICOS

A custom, bootable x86_64 operating system, developed one verified milestone at
a time. The long-term goal is an **agentic OS**: permission-controlled AI agents
working through native OS services. AgenticOS has its own C17 kernel and uses
Limine to boot it; it does not use a Linux or Windows kernel.

**Current milestone: Week 5 / Phase 5 — tasks and scheduling, verified in QEMU.**
AgenticOS runs cooperative and timer-preempted round-robin kernel tasks, with
guarded stacks, task/process control blocks, sleep, join, cancellation, mutexes
and semaphores. It retains its memory managers and owned GDT/IDT/TSS. Normal
boot verifies task contexts, synchronization and resource recovery, masks IRQs,
reports `AGENTICOS: PHASE5 PASS`, then halts with IF clear.
Kernel tasks share one address space. User mode, isolated processes, syscalls,
AI runtime, shell, filesystem, networking and desktop remain future work.
Phase 6 (user mode and syscalls) awaits owner approval.

## Build and test

See [environment setup](docs/SETUP.md) for prerequisites and pinned versions.
On the prepared Debian cloud environment:

```sh
cd /workspace/AGENTIC-OPERATING-SYSTEM
./scripts/setup-debian.sh                 # first setup, or refresh local tools
./scripts/env.sh make -j2                # build/agenticos.iso and build/kernel.elf
./scripts/env.sh make test               # ELF, sanitized host, BIOS and UEFI tests
./scripts/env.sh make test-debug         # verify GDB can break at kernel_main
./scripts/env.sh make test-foundation    # eight BIOS/UEFI log and fatal-path boots
./scripts/env.sh make test-memory        # RAM sizes, protection faults, OOM recovery
./scripts/env.sh make test-interrupts    # owned tables, timer, real faults and NMI
./scripts/env.sh make test-scheduler     # contexts, synchronization, stress and failures
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

Use the default **128 MiB or more for UEFI**. The pinned UEFI/Limine boot path
fails before kernel entry at 32/64 MiB; BIOS is separately verified at 32 MiB.
See the [Week 3 report](docs/reports/phase-3.md) for the observed limitation.

## Project map

```text
boot/                    Limine config and dependency lock
kernel/
  kernel.c               Ordered initialization, boot checks and CPU diagnostics
  arch/x86_64/           Entry/ISRs, descriptors, PIC/PIT, UART, paging and CPU
  cpu/                   Portable CPU information decoding
  debug/                 Bounded formatting, log levels, panic and assertions
  drivers/display/       Bounded framebuffer welcome renderer
  memory/                Physical allocator, heap, bootstrap and guest self-checks
  runtime/               Freestanding compiler memory primitives
  interrupts/            Exception dispatch, returning IRQs and timer self-check
  scheduler/             Task states, guarded stacks, switching and synchronization
  process/               Bounded kernel PCB groups; shared kernel address space
  syscall/               Reserved for Phase 6
  filesystem/            Reserved for Phase 7
userland/
  shell/, applications/  Future native processes
  agents/                Future isolated agent services
libraries/               Reserved for shared kernel/user libraries
scripts/                 Setup, dependency fetching, tool activation, GDB commands
tools/                   QEMU launcher and validated configuration generator
tests/                   Sanitized host, ELF, guest, protection and debugger checks
  guest/                 Deliberate fault probes; excluded from normal ISO
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
- [Kernel foundation: logging, panic and CPU discovery](docs/KERNEL_FOUNDATION.md)
- [Memory management internals](docs/MEMORY.md)
- [Interrupt and exception internals](docs/INTERRUPTS.md)
- [Tasks and scheduling internals](docs/SCHEDULING.md)
- [Week 5 results and limitations](docs/reports/phase-5.md)
- [Historical Week 4 report](docs/reports/phase-4.md)
- [Historical Week 3 report](docs/reports/phase-3.md)
- [Historical Week 2 report](docs/reports/phase-2.md)
- [Historical Week 1 report](docs/reports/phase-1.md) (original AstraOS branding)
- [Test commands and debugging](TESTING.md)
- [Contributing](CONTRIBUTING.md) · [Change log](CHANGELOG.md)

Original repository license: [Apache-2.0](LICENSE). Limine assets and its protocol
header retain their upstream licenses; see [third-party notices](docs/THIRD_PARTY.md).
