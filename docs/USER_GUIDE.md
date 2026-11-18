# Using AgenticOS

AgenticOS is a custom x86_64 kernel and native user environment. The tested target
is QEMU q35 with one CPU, RGB32 800x600 and 128 MiB RAM. BIOS is separately tested
at 32 MiB; the tested UEFI/bootloader path requires at least 128 MiB. It is not a
Linux distribution. Files and settings are stored in RAM and reset on reboot.

## Build and boot

On the prepared Debian environment:

```sh
./scripts/env.sh make -j2 iso
./scripts/env.sh make shell
```

`make iso` creates the self-test image at `build/agenticos.iso`; it validates its
subsystems, prints READY and halts. `make shell` runs an interactive serial shell
after the checks. Type `help` or `exit`. Ctrl+C stops QEMU from the host terminal.

For the graphical desktop, use `make desktop` on a local host with the optional
QEMU GTK backend installed. Headless/VNC alternative:

```sh
./scripts/env.sh make -j2 BUILD=build/desktop BOOT_TEST=desktop iso
./scripts/env.sh python3 tools/qemu.py --iso build/desktop/agenticos.iso --display vnc=127.0.0.1:1
```

Connect a local VNC viewer. To boot UEFI add `--firmware uefi`; `tools/qemu.py`
copies firmware variables to a private temporary directory. The desktop starts
after acceptance checks. Escape exits, subject to Editor's unsaved-draft guard.

## Apps and files

Open Applications in the taskbar. Focus a window by clicking it or its taskbar
button; drag its title bar and close with X. The launcher opens Terminal, Files,
Calculator, Editor, Monitor and Settings. See [APPLICATIONS.md](APPLICATIONS.md)
for exact controls and limits. Terminal uses the same commands as the serial
shell; see [SHELL.md](SHELL.md).

Examples:

```text
mkdir notes
write notes/today "Hello from AgenticOS"
cat notes/today
chmod 600 notes/today
run /bin/hello
ps
mem
id
```

`/home` belongs to the session UID 1000. Core binaries/configuration are protected
by root-owned directories. `/tmp` is shared with sticky ownership checks. Root
login, multiple accounts, groups and persistent disks are not provided.

## Networking

Networking is optional. Add `--network user` to the QEMU runner to attach the
tested e1000 with QEMU's user-mode backend. The guest uses static `10.0.2.15/24`,
gateway `10.0.2.2`, with no inbound forwarding enabled. See [NETWORK.md](NETWORK.md)
for socket APIs and protocol limits. DNS, TLS, DHCP and general Internet-browser
support are absent; this is a bounded educational network stack.

## Troubleshooting

- An INFO boot ends with READY and kernel HLT. Interactive images then open a
  session; a waiting shell/desktop is not a boot failure.
- Use 128 MiB or more for UEFI. A failure before `kernel entered` is a firmware/
  bootloader issue, rather than a kernel panic.
- `Save failed` leaves the previous file intact. Free RAM-file data/inodes or
  choose an owned directory. Reboot clears the RAM filesystem.
- `Permission denied` indicates a capability, file mode or directory ownership
  check. Use `id` and `chmod` on owned files; no setuid/grant syscall exists.
- Missing GTK is a host display-backend issue; use VNC or headless acceptance.
- Run through `scripts/env.sh` to activate rootless tools. Never mix a Debian
  extracted toolchain into another distribution; see [SETUP.md](SETUP.md).

See [RELEASE.md](RELEASE.md) for verified artifacts and [HARDWARE_TESTING.md](HARDWARE_TESTING.md)
for the explicit boundaries of physical testing. Do not install this image over
an existing OS or attach a valuable data disk.
