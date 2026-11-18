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
Open Applications to focus or launch Terminal, Files, Calculator, Editor, Monitor,
Settings or Agent. Ask Agent is also available in the top bar. Drag a title
bar; click X to close. The taskbar focuses open windows. Escape exits to the
kernel's verified halt. A maximum of eight model windows is supported; the
current launcher keeps one live window per application.

Terminal shares the native shell command parser and file/process services.
It shows command results and captures bounded child output into the terminal;
`run /bin/hello` creates a separate user process. Files browses directories,
previews text files, navigates Home/Up and provides New/Delete buttons. Click an
entry to open it. Calculator supports signed integer expressions and parentheses; Enter or `=`
evaluates, C clears and Backspace edits. Editor, Monitor and Settings complete
the native app set; see [APPLICATIONS.md](APPLICATIONS.md). Files remain volatile across reboot.

Rendering uses copied rectangle/text commands through privileged display
syscalls. The user process never maps the framebuffer. The kernel clips with
64-bit arithmetic, respects framebuffer pitch and bounds text to 64 bytes per
command. Input returns copied PS/2 events. Only the boot manager's granted
display process can use these services. Built-in windows share one user process;
this phase does not claim separate process isolation for each window.

The tested mode is 800x600 RGB32 (geometry supports 800x600 through 1920x1080).
The original 5x7 font now renders all 95 printable ASCII characters with distinct
case and punctuation, so agent paths and literal data remain reviewable. Rendering is software,
into a supervisor software back buffer, presented as a complete frame after
redraws on input; there is no GPU
acceleration, scalable font, resize support or optimized damage compositor.

`./scripts/env.sh make test-desktop` exercises calculator input, window dragging,
file preview, terminal file/child workflows, launcher and close/reopen on BIOS
and UEFI. Evidence is `build/tests/desktop-{bios,uefi}`. The screenshots are
unmodified captures of the guest framebuffer. Default acceptance boots execute bounded native desktop/app batches, restore
the welcome screen and halt. Interactive sessions start after the checks.

`make test-agent-ui` additionally exercises native Agent planning/review, full
fields, approval/deletion guards, cancellation, result readback, audit and model
errors through actual PS/2 input on both firmware paths. See
[AGENT_WORKFLOWS.md](AGENT_WORKFLOWS.md).
