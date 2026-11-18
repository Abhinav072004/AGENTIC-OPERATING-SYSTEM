# Change log

## 0.16.0 — 2026-11-18 (Agent milestone 2)

- Enforce immutable plan review, explicit approval and extra deletion consent.
- Check owner PID/UID and current capabilities before every atomic action.
- Execute native workspace tools with owner file permissions; preserve existing
  files on full-quota save failure and report partial cancellation precisely.
- Keep 64 owner-filtered RAM audit events without file contents; reject stale IDs.
- Verify real VFS behavior in sanitizer tests and BIOS/UEFI broker acceptance.

## 0.15.0 — 2026-11-18 (Agent milestone 1)

- Define bounded, canonical typed plans for seven native tools under /home.
- Parse explicit quoted multi-step requests atomically as data, with no execution.
- Validate reserved fields, zero tails, workspace scope and required authority.
- Test 20,000 host inputs and a budgeted, zero-capability native planner on BIOS
  and UEFI; confirm direct file access is denied and all resources are reclaimed.

## 0.14.0 — 2026-11-18 (Week 14)

- Centralize VERSION, validate its ABI bounds and introduce final READY semantics.
- Normalize compiler metadata, make NASM output repeatable, fix ISO timestamps
  and GPT identity, and ship compact kernel/native images with separate symbols.
- Build twice from scratch, compare ELF/ISO bytes, reproduce the ISO from a
  relocated source checkout, and package checksums/provenance/tool metadata.
- Add exact packaged BIOS/UEFI boot/desktop/exit validation and a full regression
  target; synchronize UI tests with guest input/frame acknowledgments.
- Document user operation, release procedure, supported limits and separately
  authorized physical testing. No physical installation is performed.

## 0.13.0 — 2026-11-18 (Week 13)

- Add native Editor, Monitor and Settings and extend Files/Terminal integration.
- Implement exclusive temporary files and atomic regular-file replacement;
  preserve the original after full-quota save failure and retain open readers.
- Support editor insertion/arrows/Delete, Ctrl shortcuts, unsaved-draft guards,
  live memory/task views and saved appearance preferences.
- Add checked signed calculator expressions with precedence/parentheses/depth
  limits and 20,000 sanitized inputs; consume queued input before new batches.
- Start interactive sessions after acceptance; verify apps through real PS/2,
  framebuffer captures and independent private guest-memory readback.

## 0.12.0 — 2026-11-18 (Week 12)

- Enforce process capabilities and RAM-file owner/other/search/sticky modes;
  scope filesystem identity, validate program execution and preserve root seeds.
- Add owner-only chmod, root-only chown, own identity snapshots and shell id/chmod.
  Extend file metadata and bump the experimental native ABI to version 2.
- Contain runaway CPU/syscall budgets, recursively cancel/reap descendants and
  close their descriptors/sockets; keep process IDs in signed syscall range.
- Exercise native denial, UID, orphan, budget and malformed-argument scenarios
  with exact resource recovery; retain user-mode, desktop/network regressions.
- Document the trusted boundary and explicit security limitations.

## 0.11.0 — 2026-11-18 (Week 11)

- Add a native user-space desktop, checked rectangle/text/input services and
  a supervisor back buffer with explicit presentation and complete reclamation.
- Implement mouse cursor, focus/z-order, dragging, taskbar, launcher and window
  close/reopen, plus terminal, file browsing/preview and integer calculator.
- Share shell commands between serial and graphical terminals; capture bounded
  child output without retaining user pointers. Ship stripped desktop ELF data.
- Test clipping, signed extremes, pitch guards, model limits and actual PS/2
  workflows on BIOS/UEFI; stabilize framebuffer captures after redraws.

## 0.10.0 — 2026-11-18 (Week 10)

- Add checked Ethernet/ARP/IPv4/ICMP/UDP and bounded TCP client/server sockets.
- Implement copied socket syscalls, ownership, receive windows, one-segment
  retransmission, FIN/EOF, reset/refusal and bounded timeout handling.
- Release process sockets, limit ARP retries and preserve offline operation.
- Add 30,000 malformed-packet sanitizer inputs and independent real e1000
  BIOS/UEFI exchanges with dropped SYN/data, out-of-order data and failures.

## 0.9.0 — 2026-11-18 (Week 9)

- Add a native CPL3 shell with quotes/escapes, path normalization, bounded lines,
  descriptive errors and filesystem/system/process commands.
- Add checked system/task snapshots, child spawn/wait/kill and sleep syscalls;
  allow serialized creation and off-current reaping during active scheduling.
- Own process names, grant terminal output to interactive shell children and
  add `make shell` plus BIOS/UEFI bidirectional serial acceptance.
- Verify native file workflows, child execution/status/frame recovery, 10,000
  parser inputs under sanitizers and the 12-check user-mode regression matrix.

## 0.8.0 — 2026-11-18 (Week 8)

- Discover PCI functions and initialize the tested e1000 with bounded DMA rings
  and uncached MMIO; add actual Ethernet transmit/receive.
- Add polled PS/2 keyboard/mouse, bounded input queues and signed mouse decoding.
- Add ATA LBA28 identify/read/write/flush with bounded errors and capacity checks.
- Test real device events, synthetic-disk integrity and network ring wrap on
  BIOS and UEFI; retain default no-network/no-data-disk acceptance.

## 0.7.0 — 2026-11-18 (Week 7)

- Add hierarchical RAM VFS, bounded sparse files, atomic storage quotas, file
  descriptors, open-unlink lifetime, directory enumeration and safe rename.
- Expose checked file syscalls and close all descriptors when reaping processes.
- Seed native programs and configuration; load the native FS acceptance ELF
  through the VFS and verify normal and faulted-process resource cleanup.
- Add ASan/UBSan quota/path/lifetime tests and BIOS/UEFI acceptance.
- Record the owner’s authorization to complete and push remaining milestones.

## 0.6.0 — 2026-11-18 (Week 6)

- Load bounded native ELF64 programs into private user page trees, with shared
  supervisor kernel mappings, zeroed BSS/pages, W^X and guarded user stacks.
- Add ring-3 GDT descriptors and a DPL-3 `int 0x82` syscall gate. Switch CR3 and
  TSS.RSP0 with tasks; validate/sanitize user return contexts before IRETQ.
- Implement checked console write/read and exit/getpid/yield, effective user
  page permissions, whole-buffer validation and physical-alias copies.
- Contain user page/privilege/SIMD/context faults, reap the offender and let a
  guardian finish. Trap unsaved FPU/SIMD use and clear FS/GS bases.
- Build freestanding native C hello/probe ELFs and an explicit syscall runtime.
- Add host ELF/copy/return/rollback tests and 12 BIOS/UEFI user checks with actual
  CPL3 CPU capture, private data, eight attack cases, OOM/stress and UART input.
- Extend bounded UART transmit polling after bidirectional testing exposed
  dropped characters. Normal readiness now requires PHASE6 completion.

## 0.5.0 — 2026-11-17 (Week 5)

- Add bounded PCB/TCB tables, monotonic task/process IDs, cooperative scheduling
  and PIT-driven round robin with configurable tick quantum on one CPU.
- Select full saved interrupt frames on yield/IRQ return, with actual assembly
  checks of all 15 high-bit GPR values, RSP and DF across task switches.
- Allocate four-page RW/NX task stacks with two guards, rollback partial spawn
  failures and reclaim frames/tables after off-stack join.
- Add exit status, blocking join/cycle rejection, cancellation, timed sleep and
  an idle HLT path. Reject cancellation/exit of mutex owners.
- Add FIFO mutex handoff, bounded semaphores and IRQ-preserving lifecycle/heap
  edits. Kernel tasks and PCB groups continue sharing the kernel address space.
- Add sanitized scheduling/ownership/failure tests and 12 BIOS/UEFI task checks,
  including one-tick stress, real stack OOM, guard faults and runaway watchdog.
  Normal readiness now requires PHASE5 and task resource recovery.

## 0.4.0 — 2026-11-15 (Week 4)

- Load kernel-owned GDT, 256-entry IDT and 64-bit TSS; verify their actual CPU
  registers and the hardware-written TSS busy bit. Reserve separate 16 KiB IST
  stacks for double fault, NMI and machine check.
- Normalize CPU error/no-error frames, save all 15 GPRs, clear live DF, align the
  C stack and return through IRETQ. Check high-bit register values, RSP and CF/DF.
- Report fatal exception vector/error/RIP, saved registers, stack source and CR2;
  decode page-fault flags. Guard fatal reentry with a lock-free atomic operation.
- Remap/mask the 8259 PIC, handle spurious IRQ7/15 correctly, program PIT channel
  0 near 100 Hz, and verify eight real IRQs with matching EOI and saved IF.
- Route Week 3 protection probes through the owned ISR path. Add sanitized
  descriptor/controller checks and 18 BIOS/UEFI guest checks, including real
  double-fault escalation, injected NMI, read-only/NX diagnostics and readiness
  rejection after PHASE3. Normal readiness now requires PHASE4 completion.

## 0.3.0 — 2026-11-14 (Week 3)

- Allocate/free 4 KiB physical frames using validated usable-memory regions and
  separate eligibility/allocation bitmaps; reserve allocator metadata.
- Build kernel-owned four-level page tables, switch CR3, enable WP/NX, flush
  inherited global translations, and support map/query/protect/unmap/cleanup.
- Enforce W^X, protect kernel code/constants and their direct-map aliases.
- Add a 16-byte-aligned 256 KiB first-fit heap with splitting, coalescing,
  invalid-free checks, metadata validation and two unmapped boundary guards.
- Supply freestanding compiler memory primitives and sanitizer tests.
- Verify guest data aliasing, TLB remapping, cross-page heap data and OOM recovery.
- Add 14 BIOS/UEFI memory checks, test-only real page-fault probes and readiness
  rejection checks. Record pre-kernel UEFI low-RAM failures at 32/64 MiB.

## 0.2.0 — 2026-11-11 (Week 2)

- Adopt the owner's AgenticOS branding in current banners, artifacts and docs;
  preserve original Week 1 evidence.
- Add bounded, allocation-free formatting and sequence-numbered DEBUG, INFO,
  WARNING, ERROR and PANIC logging with configurable severity filtering.
- Add always-enabled assertions and panic diagnostics with file, line, function
  and reason, followed by interrupt disable and a permanent halt.
- Probe COM1 using internal transmit/receive loopback; expose nonblocking receive.
- Report CPUID vendor, brand, family/model/stepping and feature availability.
- Generate validated build configuration; isolate intentional-failure test ISOs.
- Add sanitized host tests and eight BIOS/UEFI diagnostic/fatal boot scenarios.
- Preserve the newer GitHub README commit and the correct Abhinav Singh identity.

## 0.1.0 — 2026-11-11

- Establish the custom x86_64 C17/NASM kernel and higher-half ELF layout.
- Pin Limine v8.7.0; build a BIOS/UEFI ISO with checksum-verified assets.
- Render `Welcome to AgenticOS`; emit bounded serial diagnostics and halt.
- Add signed-APT rootless Debian setup, QEMU launch and GDB scripts.
- Verify host rendering under ASan/UBSan, ELF layout, BIOS/UEFI boots and GDB.
- Document the OS roadmap, agent/user-space boundary and current limitations.

This is Phase 1 only. No agent runtime or general-purpose OS services exist yet.
