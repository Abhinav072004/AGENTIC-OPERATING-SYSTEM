# AstraOS architecture

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
C17 kernel_main: protocol validation -> framebuffer welcome -> serial PASS -> HLT
```

Only this vertical path is implemented. Limine provides the initial 64-bit
execution environment and mappings. AstraOS does not yet own a virtual/physical
memory manager, GDT, IDT or allocator. Interrupts remain disabled.

The ELF links at `0xffffffff80000000`. Separate 4 KiB-aligned LOAD segments
contain boot requests (RW), instructions (RX), constants (R), and data/stack
(RW). No segment is both writable and executable. This is a layout property,
not evidence of user-process isolation. `.bss` contains a 64 KiB kernel stack.
The linker preserves Limine request markers so the bootloader can find them.

C targets `x86_64-unknown-none-elf`: freestanding, no libc, no dynamic loader,
no stack protector runtime, no red zone, and no SIMD register use. Interrupt
code and SIMD context management do not exist yet. C++ is unnecessary here.

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
Custom AstraOS kernel
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
- Future portable subsystems get their own directories; they call architecture
  helpers instead of spreading inline assembly throughout the kernel.
- `userland`: future native programs that use AstraOS syscalls, not host APIs.
- `scripts`, `tools`, `tests`: host programs; never linked into the kernel.

There are no physical-device or production-security claims in Phase 1.
