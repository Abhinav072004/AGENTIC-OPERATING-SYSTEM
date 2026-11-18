# Native user-space shell

`./scripts/env.sh make shell` builds the interactive ISO and boots its serial
console. Wait for `AgenticOS native shell` and `/home >`. Type `help`; use Ctrl+C
to stop QEMU. An interactive ISO can also be booted with UEFI using
`tools/qemu.py --firmware uefi --iso build/interactive/agenticos.iso` under env.sh.
The normal ISO runs a bounded automated native shell check and halts.

Commands:

- `help`, `echo`, `uname`, `uptime`, `mem`, `pid`, `ps`, `id`
- `pwd`, `cd PATH`, `ls [PATH]`, `mkdir PATH`, `touch PATH`
- `cat PATH`, `write PATH TEXT`, `append PATH TEXT`, `stat PATH`
- `mv SOURCE DESTINATION`, `rm PATH`, `chmod OCTAL_MODE PATH`
- `run /bin/hello`, `run /bin/probe`, `wait PID`, `kill PID`, `exit`

Single/double quotes preserve spaces; backslash escapes the following character
outside single quotes. Empty quoted arguments are preserved. Lines are bounded
to 255 bytes and 16 arguments. Unterminated quotes, incomplete escapes, excessive
arguments, oversized lines, invalid paths and filesystem errors are reported.
Paths are relative to `/home` initially; `.` and `..` are normalized. Traversal
above root is rejected. Operators, pipes, environment expansion, redirects and
POSIX compatibility are not implemented. Files reset on reboot.

Examples:

```text
mkdir notes
cd notes
write "week 9" "hello from AgenticOS"
cat "week 9"
mv "week 9" completed
ls
run /bin/hello
```

The shell is a native ELF at CPL3. It reads the console through checked syscalls,
prints through its granted terminal stream, and sleeps while awaiting input.
It does not parse or execute commands in kernel mode. System/process snapshots
copy fixed records without exposing kernel pointers. Spawn copies an ELF from
the VFS into a new private address space. Wait and kill are limited to direct
children. Waiting returns EAGAIN while the child runs; the shell sleeps and
retries. A zombie is reaped off its own stack. The shell waits for each `run`
command, so launched programs do not compete for console input by default.

The single-CPU scheduler now permits serialized child creation and off-current
zombie reaping during an active run. Existing upper-half stack subtrees are
shared, and newly allocated private lower mappings are never installed in the
parent root. Automated acceptance verifies child output/status and exact frame
recovery. Phase 12 adds capabilities, UID/mode/search/sticky checks, process budgets and
recursive orphan cleanup; see [SECURITY.md](SECURITY.md).

`./scripts/env.sh make test-shell` runs an actual bidirectional serial session
on BIOS and UEFI. Evidence is under `build/tests/shell-{bios,uefi}`.
