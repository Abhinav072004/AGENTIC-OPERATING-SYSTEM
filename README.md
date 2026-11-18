# AgenticOS

A custom, bootable x86_64 operating system, developed one verified milestone at
a time. The long-term goal is an **agentic OS**: permission-controlled AI agents
working through native OS services. AgenticOS has its own C17 kernel and uses
Limine to boot it; it does not use a Linux or Windows kernel.

**Current milestone: Agent milestone 4 — offline inference and optional model connector.**
The core OS has isolated native ELF processes, a RAM filesystem, tested device
and network stacks, a serial shell and a graphical desktop with six applications.
Capabilities, file permissions, bounded resources and fault containment protect
the documented single-CPU target. Files/settings reset on reboot; native ABI is 2.

Start with the [user guide](docs/USER_GUIDE.md) and [release procedure](docs/RELEASE.md).
The default test ISO prints `AGENTICOS: READY` and halts; desktop/shell builds
open an interactive session after acceptance checks. The owner has authorized
continuing through all agent milestones after the verified core release.
[Typed intent](docs/AGENT_INTENTS.md), broker execution, isolated workers and
[offline inference/model connector](docs/AGENT_INFERENCE.md) are verified.
Native review integration is the final planned agent milestone.

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
./scripts/env.sh make test-user          # real CPL3/CR3, syscalls, isolation and UART
./scripts/env.sh make test-drivers       # PS/2, PCI, private ATA disk and Ethernet rings
./scripts/env.sh make test-shell         # interactive serial shell on BIOS and UEFI
./scripts/env.sh make test-network       # real native Ethernet/IP/socket exchanges
./scripts/env.sh make test-desktop       # real framebuffer and PS/2 desktop workflows
./scripts/env.sh make test-security      # native permissions, quotas and orphan cleanup
./scripts/env.sh make test-applications  # editor, calculator, monitor and settings
./scripts/env.sh make test-agent-network # native model connector and failure cases
./scripts/env.sh make test-regression    # full regression suite
./scripts/env.sh make release test-release # two clean builds and packaged ISO boots
./scripts/env.sh make desktop            # local GTK desktop; see docs for VNC
./scripts/env.sh make shell              # use the native serial shell
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

Standard runs attach the ISO and private UEFI variables. Driver acceptance uses
a generated scratch disk. Host block devices are not attached. Physical testing
needs a separate decision; see [the procedure](docs/HARDWARE_TESTING.md).

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
  drivers/               Display, PS/2, PCI, legacy ATA and e1000 drivers
  memory/                Physical allocator, heap, bootstrap and guest self-checks
  runtime/               Freestanding compiler memory primitives
  interrupts/            Exception dispatch, returning IRQs and timer self-check
  scheduler/             Task states, guarded stacks, switching and synchronization
  process/               PCB groups and retained thread/address-space metadata
  user/                  Bounded ELF loader, private mappings, copies and containment
  syscall/               Shared native ABI and validated console/process dispatch
  fs/                    RAM VFS, descriptors, permissions and atomic replacement
  net/                   Ethernet, ARP, IPv4, ICMP, UDP and bounded TCP sockets
  gui/                   Clipped rendering, supervisor back buffer and display ABI
  security/              Capabilities and native containment/resource checks
userland/
  runtime/               Native entry, linker, syscall/command/app/window libraries
  applications/          Native shell/desktop and acceptance programs
  agents/                Future isolated agent services
libraries/               Reserved for shared kernel/user libraries
scripts/                 Setup, dependency fetching, tool activation, GDB commands
tools/                   QEMU, configuration and repeatable release tooling
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
- [Milestones and completion evidence](ROADMAP.md)
- [How the first boot works](docs/FIRST_BOOT.md)
- [Kernel foundation: logging, panic and CPU discovery](docs/KERNEL_FOUNDATION.md)
- [Memory management internals](docs/MEMORY.md)
- [Interrupt and exception internals](docs/INTERRUPTS.md)
- [Tasks and scheduling internals](docs/SCHEDULING.md)
- [User mode and syscall ABI](docs/USER_MODE.md)
- [Current release evidence](docs/reports/phase-14.md)
- [Native applications](docs/APPLICATIONS.md)
- [Security boundary and limits](docs/SECURITY.md)
- [Historical Week 6 report](docs/reports/phase-6.md)
- [Historical Week 5 report](docs/reports/phase-5.md)
- [Historical Week 4 report](docs/reports/phase-4.md)
- [Historical Week 3 report](docs/reports/phase-3.md)
- [Historical Week 2 report](docs/reports/phase-2.md)
- [Historical Week 1 report](docs/reports/phase-1.md) (original AstraOS branding)
- [Test commands and debugging](TESTING.md)
- [Contributing](CONTRIBUTING.md) · [Change log](CHANGELOG.md)

Original repository license: [Apache-2.0](LICENSE). Limine assets and its protocol
header retain their upstream licenses; see [third-party notices](docs/THIRD_PARTY.md).
