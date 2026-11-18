# Repeatable release procedure

The release targets x86_64 QEMU q35, one CPU, 128 MiB RAM and RGB32 800x600.
The ISO supports BIOS and UEFI through pinned Limine assets. The source version
is `VERSION`; experimental user ABI is 2. All files/settings remain in RAM.

```sh
./scripts/env.sh make test-regression
./scripts/env.sh make release
./scripts/env.sh make test-release
cd build/release
sha256sum -c SHA256SUMS
```

`make release` removes only its owned `build/release-repro-{a,b}` outputs, then
builds twice from scratch in independent output directories. It compares both
kernel ELFs and ISOs byte-for-byte, then also builds from a relocated source
checkout and requires the compact runtime ISO to match. Failure stops publication of new
artifacts. `build/release` contains the versioned ISO, kernel ELF/debug symbols,
SHA256SUMS, manifest and build logs. The manifest records tool versions, exact
source-tree digest, Git commit, dirty state, source epoch and artifact sizes/hashes.
A release intended for distribution should have `dirty: false`.

Reproducibility uses normalized compiler/debug paths, NASM reproducible output,
stripped kernel/native runtime images, stable GPT identity and fixed ISO metadata dates.
`SOURCE_DATE_EPOCH` defaults to 2026-11-18 00:00:00 UTC and can be set deliberately
for a different release. It must be within 1980–2107. This claim is tested with
the pinned Debian toolchain; it does not promise identical output from arbitrary
compiler/binutils/bootloader versions or unsupported hosts. Debug ELFs remain available separately from compact runtime image data. NASM
and system-header debug paths retain the recorded source/toolchain roots, so the
full debug ELF comparison uses one source root; the boot ISO is separately
verified across relocated source roots.

`make test-regression` runs the sanitized host suites, normal BIOS/UEFI boots,
GDB entry, diagnostics/filter/fatal paths, memory sizes/protection/OOM,
interrupts/NMI, scheduler stress/guards/watchdog, native user isolation/UART,
real devices, serial shell, network loss/failures, desktop/apps and permissions.
Every runner uses fresh logs and a private QEMU/QMP instance and tears it down.
`make test-release` then boots the exact packaged interactive ISO on BIOS and
UEFI, requires all fourteen core markers plus READY, captures its live desktop,
exits and verifies the kernel halt. Evidence is `build/tests/release-*`.

Before publishing a code milestone, update its report and roadmap, run relevant
checks, commit as the project owner and push the fast-forward commit to main.
The owner authorized sequential remaining milestones on 2026-11-18. Do not
rewrite existing history. Git transport uses the connected workspace credential
helper; credentials are never placed in source, artifacts or command output.

A release is educational QEMU software, with explicit limits in USER_GUIDE.md,
DRIVERS.md, NETWORK.md, APPLICATIONS.md and SECURITY.md. Physical testing remains
separately authorized under HARDWARE_TESTING.md. There is no installer and no
physical storage device is attached by the standard runner or acceptance tests.
The driver test uses a generated private scratch disk only.
