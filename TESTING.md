# Testing and debugging

From the repository root, prefix commands with `./scripts/env.sh` when using
the rootless toolchain.

```sh
./scripts/env.sh make -j2 iso
./scripts/env.sh make test
./scripts/env.sh make test-debug
./scripts/env.sh make test-foundation
./scripts/env.sh make test-memory
./scripts/env.sh make test-interrupts
./scripts/env.sh make test-scheduler
```

## Boot regressions and Phase 2 checks

1. `check-elf`: parse the linked ELF64 file, check the x86_64/static/higher-half
   entry, four page-aligned LOAD segments, no dynamic loader, no W+X segment and
   a non-executable stack declaration.
2. `test-host`: run the actual framebuffer renderer with ASan/UBSan. Check pixel
   output, pitch padding and guard preservation, alternate RGB layout and ten
   invalid-input cases. Host test allocations never enter the kernel.
3. `tests/boot_test.py --firmware bios`: execute the ISO in QEMU/SeaBIOS.
4. `tests/boot_test.py --firmware uefi`: execute it with separate OVMF CODE and
   private VARS pflash drives (Secure Boot off).
5. The host foundation binary exercises integer extrema, formatting bounds,
   log filtering/control sanitization/truncation, assertion single evaluation,
   mocked fatal stops and synthetic CPUID leaf/family/model cases under ASan/UBSan.
6. `tests/config_test.py` checks invalid options, change-sensitive header
   generation and restoration without rewriting an unchanged configuration.

Both boot tests require kernel-entry/welcome/success markers exactly once, no
kernel error or panic, structured/ordered CPU diagnostics, interrupts disabled,
a CPU halted inside the high kernel address range, and a screen
containing the expected background/text colors with visible text pixels.
They do not merely check for a PID or open port. They use a 30-second deadline,
require clean QMP-driven shutdown and kill only their own QEMU process on
failure. QMP uses a private temporary Unix socket. Existing evidence is removed
before each run to prevent stale success. Screenshots are visually inspected
when accepting a new milestone; color checks alone do not prove the wording.

Evidence lives in `build/tests/{bios,uefi}/`:

- `serial.log`: firmware and kernel serial bytes.
- `qemu.log`: emulator errors/output.
- `command.json`: exact QEMU command and expected scenario/level for that run.
- `registers.txt`: CPU state after success.
- `screen.ppm`, `screen.png`: actual framebuffer capture, same image in two formats.

These are generated files; [the Week 5 report](docs/reports/phase-5.md)
records the accepted run. Tests return nonzero on failure. An intentional
empty boot image was also checked to confirm timeout is a failing result.

## Diagnostic and fatal-path matrix

`make test-foundation` builds separate kernels/ISOs under `build/scenarios/` and
executes four configurations on each firmware type (eight guest boots):

| BOOT_TEST | KERNEL_LOG_LEVEL | Expected result |
| --- | --- | --- |
| `logs` | `DEBUG` | All nonfatal log levels present, successful completion |
| `logs` | `WARNING` | INFO/DEBUG absent, WARNING/ERROR present, successful completion |
| `panic` | `PANIC` | Source/reason reported despite maximum filtering, no completion |
| `assert` | `DEBUG` | Failed expression/source reported, no completion |

The `logs` image emits one explicitly intentional ERROR record. The panic and
assertion images must halt inside the kernel with IF clear and never emit
`AGENTICOS: PHASE2 PASS`. These are expected fatal outcomes verified by the host;
they are not reported as successful kernel initialization. Evidence directories
include scenario and level, e.g. `build/tests/bios-assert-debug/`.
The default ISO stays a normal `BOOT_TEST=none`, INFO build.
The matrix additionally changes one shared output directory from INFO/normal
to DEBUG/logs and back, checks that the restored ELF matches its original hash,
and boots that restored normal image under BIOS.
Its evidence is isolated in `build/tests/bios-restored/`. Use `--evidence-label`
when manually checking another ISO to preserve default-ISO evidence.

Manual configuration example (use a separate output directory):

```sh
./scripts/env.sh make BUILD=build/my-debug KERNEL_LOG_LEVEL=DEBUG BOOT_TEST=logs iso
./scripts/env.sh python3 tests/boot_test.py --firmware bios --scenario logs \
    --minimum-level DEBUG --iso "$PWD/build/my-debug/agenticos.iso"
```

Configuration headers are regenerated before object dependency checks. Changing
options in one output directory rebuilds affected C objects; reverting restores
the previous normal mode. Unchanged options preserve header timestamps and
permit incremental builds. Test images must not replace a normal release image.

## Memory management (Week 3)

`make test-host` also executes PMM, heap and software page-table checks under
ASan/UBSan: reserved/unaligned/overlapping ranges, physical frame zero, unique
exhaustion/recovery, invalid and double frees, buffer guards, fragmentation,
coalescing, 2048 deterministic heap stress steps, corruption detection, canonical
address/flag validation, partial-allocation rollback, shared-table cleanup and
data-frame ownership. Renamed compiler memory primitives are compared with libc
over copy/fill and forward/backward overlapping moves without replacing the
sanitizer runtime's own symbols.

`make test-memory` builds five isolated images under `build/memory-scenarios`:

| Scenario / BOOT_TEST | BIOS RAM | UEFI RAM | Result required |
| --- | --- | --- | --- |
| normal / none | 32, 128, 512 MiB | 128, 256, 512 MiB | PHASE3 through PHASE5 PASS |
| readonly / memory-ro | 128 MiB | 128 MiB | Real #PF error 0x3; write did not commit |
| nx / memory-nx | 128 MiB | 128 MiB | Real #PF error 0x11; RIP is the NX address |
| guard / memory-guard | 128 MiB | 128 MiB | Real #PF error 0x0 at the unmapped guard |
| oom / memory-oom | 128 MiB | 128 MiB | Exhaust/recover every free frame, then self-check and complete |

These are 14 guest checks. Since Week 4 the test-only observer receives actual
page faults through the production GDT/IDT/common ISR path. It reports and halts;
only the observer is excluded from the default kernel. Those six guest faults
are expected test outcomes, not successful normal boots.

Every memory run requires real data self-checks, CPU CR3 equal to the newly
allocated root, WP/NXE set, LA57/IF clear, a kernel HLT, and the actual welcome
capture after switching page tables. It records exact argv, ISO SHA-256, serial,
registers and screenshots in `build/tests/memory-{firmware}-{scenario}-{RAM}m/`.
The default image remains separate from deliberate failures. Normal/oom checks
now wait for PHASE5 and the completed task tests; earlier phase markers alone
are insufficient for current normal readiness.

The runner additionally rejects a readonly-fault image presented as a normal
image, even though that guest emits PHASE2 and PAGEFAULT success markers. This
expected host failure is checked for exit 1 and saved in
`build/tests/memory-runner-negative.log`. ANSI-interleaved bootloader panics are
recognized explicitly. Known low-RAM experiments: UEFI 32 MiB timed out before
kernel entry, and 64 MiB reported a Limine high-memory allocator panic. They
are not supported acceptance targets; use at least the default 128 MiB for UEFI.

Manual memory check:

```sh
./scripts/env.sh python3 tests/memory_boot_test.py --firmware bios --memory 32
./scripts/env.sh python3 tests/memory_boot_test.py --firmware uefi --memory 128
```

## GDB

`make test-debug` is an additional automated debugger-readiness check. It starts
QEMU paused, connects GDB and verifies a hardware breakpoint at `kernel_main`.
The log is `build/tests/debug/gdb.log`. Port 1234 must be free.

For an interactive session, use two terminals at the repository root:

```sh
# Terminal 1: paused VM, debug stub bound to loopback only
./scripts/env.sh make debug

# Terminal 2: symbols + hardware breakpoint + continue
./scripts/env.sh gdb -x scripts/debug.gdb
```

Hardware breakpoints work before the kernel's higher-half pages are mapped;
ordinary software breakpoints may fail while firmware is executing. At the C
entry point use `step`, `next`, `info registers`, `x/i $rip` and `bt`. The optimized
build may fold variables/lines; no claim is made that all C statements can be
single-stepped individually. Exit QEMU with Ctrl+C in its terminal.

## Interrupts and exceptions (Week 4)

`test-host` additionally checks actual descriptor/controller algorithms under
ASan/UBSan: GDT access/type/base/limit encoding, TSS RSP/IST/I/O bitmap layout,
all 256 gate addresses/selectors/attributes, bad inputs without partial writes,
PIC initialization/masking/cascade rules, explicit/slave/spurious EOI order,
and PIT frequency bounds, divisor and low/high port writes.
Hardware I/O is replaced by callbacks in this host binary.

`make test-interrupts` builds nine isolated images and executes **18 guest checks**
at 128 MiB, one BIOS and one UEFI run per row:

| BOOT_TEST | Required CPU result |
| --- | --- |
| none | Owned GDT/IDT/TSS, IRETQ register/flag preservation, at least eight timer IRQs/EOIs, then task completion and PHASE5 PASS |
| exception-divide | Real integer divide error, vector 0, error 0, exact fault RIP |
| exception-ud | UD2, vector 6, error 0, exact fault RIP |
| exception-gp | Invalid GDT selector, vector 13, error 0x28, exact fault RIP |
| exception-pf | Unmapped read, vector 14, error 0, exact CR2/RIP and decoded bits |
| exception-df | Real nested page-fault delivery with bad RSP, vector 8, error 0, handler on IST1 |
| exception-nmi | Host QMP inject-nmi with IF clear, vector 2, handler on IST2 |
| exception-ro | Read-only supervisor write, vector 14, error 0x3 and decoded write/present bits |
| exception-nx | NX heap instruction fetch, vector 14, error 0x11, RIP=CR2 and decoded fetch bit |

Every run verifies actual GDTR/IDTR bases and limits against the kernel report,
TR selector/limit, CS, CR3/WP/NXE, CPL 0 and a final high-kernel HLT with IF clear.
The TSS busy bit is read independently from actual guest GDT memory through
QMP; the cached TR label differs in the pinned QEMU version. Fatal runs verify
all 15 saved GPR fields and saved CS/SS/RFLAGS/RSP. Symbol addresses from the
corresponding ELF check fault RIPs; the double-fault abort RIP is not used as a
recovery instruction. The NMI run records the actual injection action.

The normal software INT check verifies distinct high-bit values in every GPR,
original RSP, saved CF/DF and live DF/IF. Controlled software IRQ7/15 vectors
exercise spurious paths; actual timer delivery separately proves hardware IRQ0
and return/EOI behavior. Only IRQ0 is enabled, and IRQ handlers do not log or
allocate. Main code reports counters after masking the PIC and clearing IF.

Fatal probes halt after EXCEPTION HALT, never PHASE4 PASS; they are expected
test outcomes rather than successful normal boots. A divide-fault image fed
to the normal boot checker must exit 1 even though PHASE3 and IRET completed.
Its output is `build/tests/interrupt-runner-negative.log`.

Images live under `build/interrupt-scenarios/{normal,divide,...}/`. Evidence is
in `build/tests/interrupt-{firmware}-{scenario}/`: exact argv, ISO and ELF hashes,
QMP actions, serial/emulator logs, register dump, `gdt-memory.txt` and actual
framebuffer captures. Runners clear known stale evidence, enforce a deadline,
request QMP shutdown, and clean up only their own QEMU process on failure.

Manual check of the default normal ISO:

```sh
./scripts/env.sh python3 tests/interrupt_boot_test.py --firmware bios
./scripts/env.sh python3 tests/interrupt_boot_test.py --firmware uefi
```

## Tasks and scheduling (Week 5)

The sanitized host binary executes PCB/state/stack policy, without privileged
host context switching. It checks bounded tables and stale IDs, PCB thread
ownership, circular fairness, one RUNNING task, cooperative versus two-tick
preemption, sleep deadlines, wait-object isolation/FIFO wake, cancellation of a
woken joiner, off-stack reap and counter/ID overflow. Stack callback tests fail
at every one of four allocation and four mapping stages, prove exact cleanup,
reject occupied guards, and allow physical frame zero.

`make test-scheduler` builds four isolated images and performs **12 guest checks**:

| BOOT_TEST | BIOS RAM | UEFI RAM | Required result |
| --- | --- | --- | --- |
| none | 32, 128, 512 MiB | 128, 256, 512 MiB | Full normal task checks and PHASE5 PASS |
| scheduler-stress | 128 MiB | 128 MiB | Four task rounds, one-tick quantum, real stack OOM/recovery, PHASE5 PASS |
| scheduler-guard | 128 MiB | 128 MiB | Real #PF at the task's unmapped lower guard, no PHASE5 |
| scheduler-watchdog | 128 MiB | 128 MiB | Runaway worker hits the scheduler watchdog, no PHASE5 |

Each normal round checks actual cooperative order ABABAB, live 64-bit GPR/RSP/
CF/DF preservation across yield, and a non-yielding assembly probe that can
finish only when another task runs through timer preemption. Three CPU-bound
workers must progress with positive runtime ticks and at least three timer
switches. The synchronization exercise forces mutex contention across a yield,
verifies eight protected updates and two semaphore tokens, sleeps through idle,
joins a spawned child, rejects a real two-task join cycle, cancels a blocked
sleeper and rejects its stale handle. After joins, free-frame, page-table and
mapping counts must exactly match the pre-task baseline.

The initial capacity exercise fills all 15 worker slots, rejects another spawn
without allocating more frames, verifies RW/NX stack mappings and both guards,
then cancels/joins every task. The stress-only OOM exercise consumes every free
physical frame, requires spawn to fail without publishing a TID, recovers all
frames and then executes the full task rounds successfully.

The guard fixture faults from a valid worker stack while reading
`0xffffd20000006000`: vector 14/error 0 and the real CR2/saved RSP are checked.
The watchdog fixture never yields and intentionally fails after its delivered-
tick bound. These four BIOS/UEFI fatal checks are expected host test outcomes,
not normal boot success. A guard-fault image presented as normal must exit 1
even though it completed PHASE4; its output is
`build/tests/scheduler-runner-negative.log`.

The runner verifies actual control/descriptor registers and GDT busy memory,
high-kernel HLT with IF clear, ordered diagnostics, exact phase/result counts,
and an actual welcome capture. Its metadata includes exact QEMU argv/RAM and
ISO/ELF hashes. Evidence is in
`build/tests/scheduler-{firmware}-{scenario}-{RAM}m/`, with images isolated in
`build/scheduler-scenarios/`. Known stale outputs are removed before each run;
deadlines, clean QMP exit and owned-process cleanup remain required.

```sh
./scripts/env.sh python3 tests/scheduler_boot_test.py --firmware bios --memory 32
./scripts/env.sh python3 tests/scheduler_boot_test.py --firmware uefi --memory 128
```

## Limits

The targets cover Phase 1–5 behavior. They do not verify general exception
recovery, user isolation, SMP/concurrent allocation, driver portability, AI,
physical boot or Secure Boot. Actual machine-check delivery, APIC/IOAPIC,
arbitrary NMI nesting, user-mode scheduling and FPU/SIMD contexts are not exercised or claimed.
These require future milestones.
