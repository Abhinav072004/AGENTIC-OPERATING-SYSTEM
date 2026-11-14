# Week 2: understanding the kernel foundation

This is the Week 2 foundation walkthrough. Week 3 extends it with
[kernel-owned memory management](MEMORY.md); historical acceptance results
remain in the Week 2 report.

Phase 1 proved the CPU could reach our C code. Phase 2 adds the diagnostics
needed to develop memory management and interrupts without guessing why a
boot stopped. These services run before allocation, threads or a filesystem.

## Initialization order

1. NASM establishes the kernel stack with maskable interrupts disabled.
2. Initialize COM1 at 115200 baud/8N1, test internal transmit/receive loopback,
   then restore normal UART mode.
3. Bind the logger to the serial sink and reset its sequence counter.
4. Check the Limine protocol and framebuffer; report a fatal panic if invalid.
5. Draw `Welcome to AgenticOS` using the existing framebuffer renderer.
6. Read CPUID, assert long-mode capability, log CPU details and current limits.
7. Run explicitly selected test probes, or complete normal initialization.
8. Emit `AGENTICOS: PHASE2 PASS` only on successful completion, then halt.

No interrupt is enabled and no memory manager is introduced in this phase.

## Structured logging without a C library

The kernel has no host `printf`, stdout, libc or heap. `kernel/debug/format.c`
implements a small bounded formatter; `log.c` wraps it into records such as:

```text
[3] [INFO] [cpu] vendor=AuthenticAMD family=15 model=107 stepping=1
```

The first field is an emitted-record sequence, **not a timestamp**. There is no
clock/timer driver yet. Filtered records do not consume sequence numbers.
`log_init` resets state, allowing deterministic repeated boots and host tests.

Severity is ordered DEBUG < INFO < WARNING < ERROR < PANIC. Build option
`KERNEL_LOG_LEVEL` selects the minimum. PANIC remains visible even at the
highest minimum. Logging a PANIC-severity record alone does not stop execution;
the fatal `PANIC()` function reports the record and performs the stop.

Each message has a 256-byte buffer, each component a 32-byte buffer and each
complete record a 352-byte buffer. Overflow truncates the message with `...`.
Every record is one line; embedded control bytes become `?`. No allocation is
performed. A sink borrows the stack-backed record only during its call and
must not retain it or recursively invoke the logger.

The formatter supports `%s`, `%c`, `%d`, `%u`, `%x`, `%p`, `%%`, `l`/`ll` integer
lengths and unsigned `z` lengths. It returns the would-have-written length,
NUL-terminates a nonzero-capacity output and rejects unsupported formatting
with -1. Null string arguments render as `(null)`. Flags, field widths,
precision, floats and `%n` are intentionally unsupported. Logger errors yield
`<invalid log format>` rather than consuming unknown argument types.
Use trusted literal formats and type-correct arguments, as with `printf`.

This is a bootstrap logger for one CPU with interrupts disabled. It has no
locks, persistent ring buffer, NMI support or concurrency guarantees. Those
must be revisited before interrupts/SMP are enabled. Bounded buffers do not
make arbitrary invalid string pointers safe to read.

## Assertions and fatal panic

`KASSERT(expression)` tests an invariant exactly once. It is always enabled,
including builds defining `NDEBUG`; it does not depend on the host assert macro.
Failure expands to `PANIC()` with file, line, function and the expression text.
Explicit panic can report another unrecoverable condition.

The handler disables interrupts before logging. It uses only stack storage and
the bounded serial path, then enters a permanent `cli; hlt` loop. It cannot
return into damaged state. Host tests replace the architecture stop functions
with a controlled `longjmp`; guest tests exercise the real privileged halt.
There is no exception recovery, stack trace unwinder, crash dump or reboot
policy yet. The framebuffer stays on its welcome screen; panic details are on
COM1, so retain the serial console when diagnosing a stopped VM.

## CPU identification

CPUID is available on x86_64. `arch_cpuid` executes the instruction; a portable
decoder queries leaf 0 for the vendor/maximum basic leaf, leaf 1 for model and
feature data, and supported extended leaves for NX, long mode and the brand.
Unsupported leaves are never assumed to exist. Model/family extension bits are
applied only under the architectural rules for base families 6 and 15.

The guest reports **QEMU's emulated CPU**, not the cloud host's CPU. Feature
availability is not feature initialization: reporting APIC/SSE2/NX does not
mean AgenticOS has an APIC driver, uses SIMD or manages protected mappings.
The compiler still avoids SIMD and the kernel still uses Limine's mappings.

## Serial console boundary

COM1 supports polling output and a nonblocking receive primitive. The startup
loopback test verifies transmit/receive without keyboard or timer support.
Errored input is consumed and rejected; absent input returns immediately.
Transmission and loopback polling have finite iteration limits, so missing
hardware does not cause an infinite wait. Iteration limits are not calibrated
wall-clock timeouts. There is no input loop, command interpreter or shell yet.
If COM1 is absent, the bootstrap assertion stops; there is no alternate panic
output sink, and UART transmission can drop bytes on timeout.

## Build configuration and AI prerequisites

`tools/configure.py` validates `KERNEL_LOG_LEVEL` and `BOOT_TEST`, generates a
header under the chosen build directory and only rewrites changed content.
Make includes it in the cross build and tracks it as an object dependency.
The normal build uses INFO and no self-tests. The test matrix keeps deliberate
panic/error images isolated from `build/agenticos.iso`.

These diagnostics are a prerequisite for future agent services: failures must
be observable before adding model runtimes or autonomous tools. This phase
contains no inference, agent planner, permissions broker or AI execution.
The next approved phase is physical/virtual memory management.
