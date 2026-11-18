# Week 7 — RAM filesystem

Verified on 2026-11-18 with the pinned Clang/NASM/QEMU environment.

The VFS adds absolute path resolution, directories, descriptors, sparse bounded
files, quota preflight, create/read/write/seek/close/stat/list/delete and atomic
rename. A file remains readable through an open descriptor after unlink; its
last close reclaims blocks. Process reaping closes descriptors even after faults.
The native acceptance ELF is seeded into RAM, read through the VFS, then loaded
into ring 3. Its normal and deliberately faulted runs restore all transient
filesystem blocks and physical frames.

Executed:

- `./scripts/env.sh make -j2 iso`: warning-clean freestanding build.
- `./scripts/env.sh make test`: ELF validation; all existing sanitized host
  suites; new ASan/UBSan filesystem hierarchy, cross-block and sparse data,
  descriptor exhaustion, full quota rollback, rename cycles and cleanup tests;
  BIOS and UEFI normal boots reach PHASE7 and FS USER PASS.
- `./scripts/env.sh make test-user`: all 12 BIOS/UEFI checks pass, including
  BIOS 32/128/512 MiB, UEFI 128/256/512 MiB, four-round stress/partial OOM,
  actual CPL3 capture, UART input and rejected required ELF. Normal readiness
  still rejects the deliberately broken image after PHASE5.
- `git diff --check`: clean.

Default BIOS evidence is `build/tests/bios/serial.log`; firmware counterpart
and images are under `build/tests/uefi`. The new records show the native file
checks, vector-6 fault containment, nine retained seed blocks and PHASE7 PASS.
Regression evidence is under `build/tests/user-*`.

Artifact SHA-256:

```text
5f160413ed21b83df538ec37237b5fff2c33920f23c6fb65bb214d5a8eea2cc9  build/agenticos.iso
c0385e033d6765d0ff509f747ce4d83ea96a2c0bd75e2cedca4215ce054224c7  build/kernel.elf
```

Limits: one serialized CPU; 64 inodes, 512 KiB data quota, 64 KiB/file,
16 descriptors/process, 128-byte syscall transfers. RAM data resets on reboot.
Persistent filesystems and multiple mount backends are not implemented. Access
policy remains a later roadmap milestone; no physical disk was attached.
