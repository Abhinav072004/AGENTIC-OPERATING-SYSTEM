# Testing and debugging

From the repository root, prefix commands with `./scripts/env.sh` when using
the rootless toolchain.

```sh
./scripts/env.sh make -j2 iso
./scripts/env.sh make test
./scripts/env.sh make test-debug
```

## Required Phase 1 checks

1. `check-elf`: parse the linked ELF64 file, check the x86_64/static/higher-half
   entry, four page-aligned LOAD segments, no dynamic loader, no W+X segment and
   a non-executable stack declaration.
2. `test-host`: run the actual framebuffer renderer with ASan/UBSan. Check pixel
   output, pitch padding and guard preservation, alternate RGB layout and ten
   invalid-input cases. Host test allocations never enter the kernel.
3. `tests/boot_test.py --firmware bios`: execute the ISO in QEMU/SeaBIOS.
4. `tests/boot_test.py --firmware uefi`: execute it with separate OVMF CODE and
   private VARS pflash drives (Secure Boot off).

Both boot tests require kernel-entry/welcome/success markers exactly once, no
kernel error, a CPU halted inside the high kernel address range, and a screen
containing the expected background/text colors with visible text pixels.
They do not merely check for a PID or open port. They use a 30-second deadline,
require clean QMP-driven shutdown and kill only their own QEMU process on
failure. QMP uses a private temporary Unix socket. Existing evidence is removed
before each run to prevent stale success. Screenshots are visually inspected
when accepting a new milestone; color checks alone do not prove the wording.

Evidence lives in `build/tests/{bios,uefi}/`:

- `serial.log`: firmware and kernel serial bytes.
- `qemu.log`: emulator errors/output.
- `registers.txt`: CPU state after success.
- `screen.ppm`, `screen.png`: actual framebuffer capture, same image in two formats.

These are generated files; [the committed report](docs/reports/phase-1.md)
records the accepted run. Tests return nonzero on failure. An intentional
empty boot image was also checked to confirm timeout is a failing result.

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

The test target covers only implemented Phase 1 behavior. It does not verify a
memory manager, exception recovery, user isolation, driver portability, AI,
physical boot or Secure Boot. These require their own future milestones.
