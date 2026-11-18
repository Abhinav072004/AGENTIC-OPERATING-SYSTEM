# Native desktop

`./scripts/env.sh make desktop` builds and starts the desktop ISO on a local
host with QEMU GTK installed. On a host without GTK, build with
`make BUILD=build/desktop BOOT_TEST=desktop iso` under env.sh and use
`tools/qemu.py --iso build/desktop/agenticos.iso --display vnc=127.0.0.1:1`,
or run `make desktop QEMU_DISPLAY=vnc=127.0.0.1:1`. Connect a local VNC viewer.
The VNC example binds loopback. The cloud acceptance tests use headless QEMU,
real PS/2 event injection and captured framebuffer output.

The desktop is a native ring-3 ELF with a window model, z-order, taskbar,
launcher, focus, title-bar dragging, close/reopen and a software mouse cursor.
Open Applications to focus or launch Terminal, Files or Calculator. Drag a title
bar; click X to close. The taskbar focuses open windows. Escape exits to the
kernel's verified halt. A maximum of eight model windows is supported; the
current launcher keeps one live window per application.

Terminal shares the native shell command parser and file/process services.
It shows command results and captures bounded child output into the terminal;
`run /bin/hello` creates a separate user process. Files browses directories,
previews text files, navigates Home/Up and provides New/Delete buttons. Click an
entry to open it. Calculator accepts two nonnegative integer operands and one
of `+ - * /`; Enter or `=` evaluates, C clears, Backspace edits. Overflow and
division by zero report an error. More applications and richer editing are
Phase 13 work. Files remain volatile across reboot.

Rendering uses copied rectangle/text commands through privileged display
syscalls. The user process never maps the framebuffer. The kernel clips with
64-bit arithmetic, respects framebuffer pitch and bounds text to 64 bytes per
command. Input returns copied PS/2 events. Only the boot manager's granted
display process can use these services. Built-in windows share one user process;
this phase does not claim separate process isolation for each window.

The tested mode is 800x600 RGB32 (geometry supports 800x600 through 1920x1080).
The original 5x7 font renders lowercase using capitals. Rendering is software,
into a supervisor software back buffer, presented as a complete frame after
redraws on input; there is no GPU
acceleration, scalable font, resize support or optimized damage compositor.

`./scripts/env.sh make test-desktop` exercises calculator input, window dragging,
file preview, terminal file/child workflows, launcher and close/reopen on BIOS
and UEFI. Evidence is `build/tests/desktop-{bios,uefi}`. The screenshots are
unmodified captures of the guest framebuffer. Default acceptance boots execute
a bounded native desktop batch, restore the welcome screen, report PHASE11 and
halt, preserving earlier boot/fault test expectations.
