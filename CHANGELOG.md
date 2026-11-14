# Change log

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
