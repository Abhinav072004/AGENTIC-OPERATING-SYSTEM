# Testing and debugging

From the repository root, prefix commands with `./scripts/env.sh` when using
the rootless toolchain.

```sh
./scripts/env.sh make -j2 iso
./scripts/env.sh make test
./scripts/env.sh make test-debug
./scripts/env.sh make test-foundation
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

These are generated files; [the Week 2 report](docs/reports/phase-2.md)
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

## Limits

The targets cover implemented Phase 1/2 behavior. They do not verify a
memory manager, exception recovery, user isolation, driver portability, AI,
physical boot or Secure Boot. These require their own future milestones.
