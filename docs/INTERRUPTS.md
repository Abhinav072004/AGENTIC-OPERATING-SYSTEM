# Week 4: interrupts and exceptions

AgenticOS now owns the descriptor tables used after initial boot, handles
returning timer IRQs, and prints fatal CPU exception diagnostics. It still has
one bootstrap CPU. Week 5 adds [kernel-task scheduling](SCHEDULING.md) and Week 6
adds [native user processes](USER_MODE.md). The final boot state is still a masked, IF-clear halt after
the task exercises finish and their resources are reclaimed.

## GDT, TSS and IDT have different jobs

The GDT defines segment descriptors. Long-mode code uses a flat address space,
but CS still needs a valid 64-bit code descriptor and IDT gates still select it.
AgenticOS installs a null descriptor, ring-0 code at selector `0x8`, writable
data at `0x10`, and a two-slot 64-bit TSS descriptor at `0x18`. Week 6 appends
ring-3 code/data at selectors `0x2b`/`0x33`. The table is now 56 bytes with
inclusive limit 55. A far return reloads CS; data/stack
selectors and TR are reloaded explicitly.

The 104-byte TSS supplies stack pointers. It does not schedule processes or
perform hardware task switching here. RSP0 points at the bootstrap stack for
privilege transitions and is updated to the selected task's kernel-stack top.
The CPU uses it on ring-3 interrupt/syscall entry. Three separate
16 KiB stacks serve IST1 (double fault), IST2 (NMI) and IST3 (machine check).
The I/O bitmap offset is beyond the TSS limit. LTR must set the TSS descriptor's
busy bit in GDT memory, which the kernel and QMP tests independently check.
QEMU's register dump can label its cached TR `TSS64-avl` even after the memory
descriptor becomes busy; the tests therefore inspect the actual GDT bytes too.

The IDT has 256 sixteen-byte interrupt gates, with inclusive limit 4095. Each
gate selects our code descriptor and points at its own assembly stub. All gates
have DPL 0 except the Week 6 syscall gate at vector 130, which has DPL 3.
The DPL-0 gates reject arbitrary user software INTs; CPU-generated faults still
reach their handlers regardless of gate DPL.
Interrupt gates clear live IF on entry. Only vectors 8, 2 and 18 select an IST;
other handlers use the interrupted stack.

All tables/stacks live in kernel data/BSS and remain mapped RW/NX when CR3
changes. Bootloader/ACPI memory remains reserved conservatively even though the
bootloader GDT is no longer used. General IDT ownership is present in the default
kernel; deliberate fault generators and the Week 3 observer are test-image only.

## From a CPU event to C and back

```text
CPU event -> IDT gate -> optional IST stack -> CPU frame
          -> vector stub -> saved GPRs -> aligned C dispatcher
          -> restored GPRs -> remove vector/error -> IRETQ
```

Long mode saves SS, RSP, RFLAGS, CS and RIP. CPU exceptions with architectural
error codes additionally push one error slot. For other vectors the stub pushes
zero; each stub then pushes its vector. The shared entry saves all 15 GPRs
other than RSP, clears DF for the C ABI and aligns the call stack to 16 bytes.
The resulting 176-byte frame layout is checked with C static assertions.
The C dispatcher returns a pointer to the saved frame to restore, so alignment
padding is not mistaken for saved registers. Week 5 may return another task's
frame; the stub changes RSP only after C finishes using the old stack.

The error-code list includes vectors 8, 10–14, 17, 21, 29 and 30. Kernel code
must not issue software INT against CPU error-code exception vectors: a software
INT does not manufacture the CPU exception's error slot. The bootstrap register
probe uses vector 128, gated by test state. Kernel-task yield/block/exit uses
vector 129 while the scheduler is active. User syscalls use vector 130.

IRETQ restores the interrupted instruction, stack and flags. The normal self-check
passes distinct values with bit 40 set in all 15 GPRs, verifies the C view of
those fields, and checks the values, RSP, CF and DF after return. It also checks
that live DF/IF were clear inside dispatch. This exercises actual CPU entry and
return rather than only comparing descriptor encodings.

The build uses no red zone and `-mgeneral-regs-only`: asynchronous stack frames
must not overwrite a red zone, and these stubs do not preserve SIMD/FPU state.
Ring-3 entry uses the interrupt gate and TSS stack. FS/GS bases are explicitly
zero with no TLS/SWAPGS ABI. User FPU/MMX/SIMD instructions trap until those
contexts are supported; current task switches save GPR contexts.

## PIC and PIT timer path

The tested backend is the cascaded 8259-compatible PIC on QEMU q35. It remaps
master IRQ0–7 to vectors 32–39 and slave IRQ8–15 to 40–47, avoiding CPU exception
vectors. Initialization uses explicit EOI mode and leaves all lines masked.
Unmasking any slave line also opens the master's IRQ2 cascade.

PIT channel 0 uses binary rate-generator mode with low/high count writes.
The input clock is 1,193,182 Hz; rounding a 100 Hz request gives divisor 11,932,
approximately 100 interrupts per second. Only IRQ0 is unmasked for the test.

The handler increments bootstrap-CPU counters and acknowledges the PIC. It
does not log, allocate or acquire locks. Real slave IRQs acknowledge the slave
before the master. Spurious IRQ7 receives no EOI; spurious IRQ15 acknowledges
only the master cascade. The PIC's in-service register distinguishes these
cases. Host callback tests verify port order, and controlled software INTs
exercise both spurious dispatcher branches with no in-service request.

The main test uses adjacent `sti; hlt; cli`: STI's interrupt shadow makes sleep
atomic with respect to enabling maskable interrupts. After at least eight
ticks it masks all PIC lines. With IF clear, it checks matching timer EOI and
saved-IF counts, rejects unexpected real IRQs and prints the results. EOI tells
the controller a request is finished; IRETQ separately restores CPU execution.
Neither substitutes for the other.

The PIT counter can continue running after the PIC is masked; it does not wake
the final IF-clear HLT. Week 5 reuses the timer during task runs for preemption
and tick-based sleeps, with the same approximate rate. It is not a calibrated
wall clock or APIC backend. If IRQs never arrive,
the guest remains asleep and the host acceptance deadline fails the run.

## Fatal diagnostics and dedicated stacks

Unhandled kernel exceptions and unexpected non-PIC vectors report vector/name,
error code, RIP, CR2, saved CS/SS/RFLAGS/RSP, all 15 GPRs and the handler's stack
source. Page faults decode present, write, user, reserved-bit, instruction-fetch,
protection-key, shadow-stack and SGX bits. CR2 is meaningful for page faults;
on unrelated exceptions it can contain an older fault address.

Week 6 contains ordinary CPL3 faults by terminating that user process and
selecting another context. NMI, double fault and machine check retain their
fatal policy. Kernel fatal handlers do not advance RIP or return into damaged state. They emit
EXCEPTION HALT and stop with CLI/HLT. A lock-free atomic flag prevents nested
fatal paths from recursively invoking formatting; reentry uses a fixed serial
message and halts. Serial output remains bounded, best effort and nonpersistent.
A fatal NMI interrupting a normal serial record can leave that record partial;
the bootstrap logger is not generally NMI-reentrant or SMP-safe.

One test reads an unmapped address after moving RSP into an unmapped guard.
The CPU cannot deliver the first page fault on that stack; escalation delivers
a real double fault using IST1. Tests verify the saved bad RSP and that the
handler frame actually resides on IST1. Double faults are aborts; their saved
RIP is diagnostic and is not treated as a reliable recovery instruction.
Another image waits with IF clear while the host injects a genuine NMI through
QMP, then verifies vector 2 on IST2. Machine-check IST3 is configured and its
encoding is checked, but an actual machine check is not exercised.

The fixed IST stacks have no guard pages, and this is not arbitrary stack
corruption recovery. Ordinary exceptions still need a valid current stack
unless delivery can escalate to the dedicated double-fault path. Repeated NMI,
machine-check and severe memory corruption handling need further work.

## Verification and boundaries

`make test-host` runs descriptor/controller algorithms under ASan/UBSan with
fake port callbacks; no privileged host instruction is executed.
`make test-interrupts` performs 18 real QEMU checks: two normal boots and eight
fatal scenarios on both BIOS and UEFI. They cover divide, invalid opcode,
general protection, absent/read-only/NX page faults, double fault and injected
NMI. QMP captures actual descriptor/control registers, GDT memory, screenshots,
commands and artifact hashes. A fatal image that already passed PHASE3/IRET
must fail the normal readiness checker. Current normal readiness additionally
requires PHASE6 after task/user checks; PHASE4 records this subsystem's completion.

These are tested QEMU single-CPU behaviors. APIC/IOAPIC, SMP, user isolation,
recoverable exceptions, device-driver IRQ registration and physical hardware
validation remain future work. Kernel/user preemption and the five basic
syscalls now exist. The filesystem milestone awaits owner approval.
