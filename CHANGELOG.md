# Change log

## 0.1.0 — 2026-11-11

- Establish the custom x86_64 C17/NASM kernel and higher-half ELF layout.
- Pin Limine v8.7.0; build a BIOS/UEFI ISO with checksum-verified assets.
- Render `Welcome to AstraOS`; emit bounded serial diagnostics and halt.
- Add signed-APT rootless Debian setup, QEMU launch and GDB scripts.
- Verify host rendering under ASan/UBSan, ELF layout, BIOS/UEFI boots and GDB.
- Document the OS roadmap, agent/user-space boundary and current limitations.

This is Phase 1 only. No agent runtime or general-purpose OS services exist yet.
