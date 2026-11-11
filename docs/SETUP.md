# Development environment

## Inspected cloud host

Debian GNU/Linux 13.6 (trixie), x86_64, five available CPUs, approximately 33 GiB
RAM and 60 GiB available disk at inspection. Initially present: GCC 14.2.0,
Make 4.4.1, Python 3.12.14, Git 2.52.0, curl, tar and SHA-256 utilities.
Clang/LLD, NASM, QEMU, GDB and xorriso were initially absent. There is no sudo
or scoped system-package helper on this host. Ordinary `apt-get update` failed
because system package directories are read-only.

`scripts/setup-debian.sh` solves this with a user-owned APT state/cache and
package extraction at `/workspace/.astra-tools/root`. It does not change
system repositories, disable verification, or run package maintainer scripts.
APT verifies official signed indexes and downloaded package hashes. The
base host supplies already installed dependencies; this is not a standalone
toolchain for arbitrary Linux distributions.

## Validated version pins

| Component | Debian package version / source pin |
| --- | --- |
| Clang and LLD 19.1.7 | `1:19.1.7-3+b1` |
| NASM 2.16.03 | `2.16.03-1` |
| QEMU x86/system data/common 10.0.13 | `1:10.0.13+ds-0+deb13u1` |
| SeaBIOS | `1.16.3-2` |
| OVMF | `2025.02-8+deb13u1` |
| GDB 16.3 | `16.3-1` |
| xorriso 1.5.6 | `1.5.6-1.2+b1` |
| Limine v8.7.0 binary distribution + header | `aad3edd370955449717a334f0289dee10e2c5f01` |

Limine's archive SHA-256 is pinned in `boot/limine.lock`. The fetcher checks it
before extraction and builds the supplied host `limine` utility with `cc`.
Do not switch headers independently of the bootloader. Transitive Debian
dependencies are resolved from signed trixie repositories. These commands
reproduce the build procedure; bit-for-bit ISO reproducibility is not yet
claimed because timestamps and transitive package availability may vary.

## Prepared cloud environment

```sh
cd /workspace/AGENTIC-OPERATING-SYSTEM
./scripts/setup-debian.sh
./scripts/env.sh make -j2
./scripts/env.sh make test test-debug
```

Setup was rerun successfully using the existing package cache. Run commands
through `scripts/env.sh` to activate the rootless tools only for that process.
The directory can be changed by setting `ASTRA_TOOLS` to an absolute path
before both setup and activation. Do not mix a rootless toolchain for another
distribution with this Debian host. If pinned packages disappear from the
repository, deliberately update and revalidate pins; do not bypass APT checks.

Necessary network destinations: `deb.debian.org`, `codeload.github.com`; Git
collaboration uses `github.com` through the platform's injected Git proxy.
No API credentials or model downloads are needed. The environment's existing
package-manager network preset allowed these operations.

## A local Debian 13 host

With administrator privileges, the same packages can be installed normally.
Install base prerequisites (`build-essential`, `python3`, `git`, `curl`,
`ca-certificates`, `tar`) and the exact packages/versions listed in
`scripts/setup-debian.sh` using APT. Then `make -j2` and `make test` work directly.
`scripts/env.sh` also works without a local toolchain directory and simply
uses installed tools. Optional `qemu-system-gui` adds GTK for an on-screen VM.

On Ubuntu/WSL2, install the corresponding tools from that distribution's
supported packages; Debian version strings are not Ubuntu package versions.
That path has not been validated in this milestone. The cross target and
freestanding flags remain mandatory. Do not use an ordinary host GCC command
without a cross-kernel configuration.

## Windows

Use WSL2 with Ubuntu for source/build tools instead of a normal Windows C
compiler. Store source in the WSL filesystem. QEMU can run in WSL with TCG;
desktop display requires WSLg or an appropriate display backend. This cloud
run validates Debian only, not Windows/WSL or physical hardware.

## Services and retained state

Tools, `.cache/limine` and generated `build/` files can be retained. Live QEMU
processes and debugger connections must be restarted for each task. Use the
existing checkout: cloud tasks are already isolated; do not create Git worktrees
unless explicitly requested. `make clean` removes build outputs only;
`make distclean` also removes fetched Limine assets. Neither removes tools.

No package/model secrets, physical disks, installation scripts, Docker services
or other background services are required. UEFI variables are copied to a
temporary directory per run, so tests do not mutate installed firmware files.
