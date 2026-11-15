# AgenticOS architecture

## Boot and kernel

The initial design is a small modular monolithic kernel: drivers and memory
management will share kernel address space, while applications and agents will
eventually execute in isolated user processes. This keeps early boot learnable
and avoids the IPC complexity of a microkernel before processes exist.

```text
BIOS / UEFI firmware
        |
Limine v8.7.0: ELF loading, initial page tables, framebuffer discovery
        |
NASM _start: kernel-owned stack, System V AMD64 calling convention
        |
C17 kernel_main: serial/logging -> boot validation -> framebuffer -> CPUID
                -> GDT/IDT/TSS + masked PIC -> PMM -> own CR3 -> heap
                -> memory self-checks -> IRETQ check -> PIT IRQs -> masked HLT
```

This path is implemented through Phase 4. Limine provides the initial 64-bit
environment and mappings. AgenticOS loads its own GDT/IDT/TSS before memory
bootstrap, then builds fresh page tables and switches CR3. Bootloader metadata
stays mapped/reserved conservatively. All descriptor and exception-stack storage
lives in the kernel image and survives the address-space switch.
Only the timer test enables maskable interrupts; it masks all PIC lines and
clears IF before reporting final success and halting.

The ELF links at `0xffffffff80000000`. Separate 4 KiB-aligned LOAD segments
contain boot requests (RW), instructions (RX), constants (R), and data/stack
(RW). No segment is both writable and executable. This is a layout property,
not evidence of user-process isolation. Phase 3 also enforces these permissions
in its own mappings; direct-map aliases of code/constants are read-only and NX.
`.bss` contains a 64 KiB kernel stack.
The linker preserves Limine request markers so the bootloader can find them.

C targets `x86_64-unknown-none-elf`: freestanding, no libc, no dynamic loader,
no stack protector runtime, no red zone, and no SIMD register use. Interrupt
stubs save all 15 general-purpose registers and restore with IRETQ; SIMD context
management remains future work. C++ is unnecessary here.

## Agentic design direction — planned, not implemented

AI functionality belongs above the kernel. An inference crash, malformed model
response, or unavailable remote service should not crash the OS or acquire
kernel privileges. A model generates a proposed action; an ordinary OS service
must authorize and validate the action before execution.

```text
Native shell / desktop / user intent
                  |
User-space agent planner and inference provider
                  |
Policy-checked tool broker + explicit grants + action/audit history
                  |
Validated system calls: processes, files, devices, networking
                  |
Custom AgenticOS kernel
```

Future agent milestones require user mode, validated syscalls, filesystem
permissions, process/resource isolation and, for hosted inference, networking.
Prefer deterministic tools with bounded arguments. Credentials should remain
in a trusted service, separate from model context. Destructive or externally
visible operations need explicit policy and user intent; model text is never
executable authority. The exact local model/runtime and cloud providers remain
design decisions for a later approved milestone, not boot dependencies.

Do not describe the current kernel as an AI runtime. Scheduling, recovery,
offline operation, cancellation, audit persistence and capability enforcement
are design requirements, not completed features.

## Boundaries

- `arch/x86_64`: register/port operations, assembly entry, architecture layout.
- `kernel/drivers`: device-specific implementation. Current display consumes
  borrowed Limine metadata without allocating or freeing it.
- `kernel/kernel.c`: initialization order and boot validation.
- `kernel/debug`: bounded formatting, sequence-numbered log records and fatal
  diagnostics; no allocator or libc dependency.
- `kernel/cpu`: CPUID decoding behind an injectable reader; actual register
  access and privileged panic-stop instructions stay in `arch/x86_64`.
- `kernel/memory`: portable frame/heap algorithms and ordered memory bootstrap.
  Four-level table encoding and CR3/WP/NX/TLB operations stay in `arch/x86_64`.
- `kernel/runtime`: compiler-required memory functions, with no host libc link.
- `kernel/interrupts`: fatal exception diagnostics, PIC IRQ dispatch and bounded
  timer acceptance. Descriptors, ISR assembly and PIC/PIT port policy remain in
  `arch/x86_64`; controller/descriptor algorithms are host-testable.
- Future portable subsystems get their own directories; they call architecture
  helpers instead of spreading inline assembly throughout the kernel.
- `userland`: future native programs that use AgenticOS syscalls, not host APIs.
- `scripts`, `tools`, host tests: development utilities, not linked into the kernel.
  `tests/guest` is linked only into intentional memory/exception test images.

There are no physical-device or production-security claims in Phase 4.
Logging currently assumes one CPU with interrupts disabled; it is not an
SMP/reentrant logging system. Timer IRQ handlers never log or allocate; their
counters are reported with IF clear afterward. Fatal exceptions use bounded
serial diagnostics, a lock-free reentry guard and dedicated IST stacks for
double fault, NMI and machine check. This does not make all logger operations
NMI-safe or provide arbitrary exception recovery.
See [foundation internals](docs/KERNEL_FOUNDATION.md).
Allocators and page-table edits also assume serialized bootstrap-CPU execution.
See [memory internals and ownership](docs/MEMORY.md) for layout, tests and limits.
See [interrupt internals](docs/INTERRUPTS.md) for frames, EOI and fatal behavior.
