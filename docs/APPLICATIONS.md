# Native applications

Launch applications from the desktop's Applications menu. Built-in windows run
inside the native compositor ELF at CPL3. They share its process and use checked
OS syscalls; this is not separate process isolation for each built-in window.
External programs launched by Terminal retain their own private address spaces.

Editor supports ASCII text up to 4095 bytes, insertion, Backspace/Delete, arrow
navigation and a viewport following the cursor. Open selects a path; a missing
path starts a new file. Save or Ctrl+S saves. New/Ctrl+N and Open/Ctrl+O preserve
an unsaved draft until Save or explicit Discard. Closing the editor or exiting
the desktop while a draft is dirty leaves it open with a message. Discard resets
the draft deliberately. There is no undo, clipboard, selection, Unicode or
concurrent-edit merge support. RAM files and drafts do not survive reboot.

Saving writes an exclusive temporary file in the destination directory and
commits it through atomic VFS replacement. Allocation or write failure closes
and removes the temporary file and preserves the previous target. Replacement
checks both parents and sticky rules before mutation; existing open descriptors
continue reading the old inode. Replacement follows directory rename permission
semantics, rather than treating file mode bits as an immutable attribute.

Files supports directory navigation, text preview, New/Delete and opening a
selected text file in Editor through Edit. Terminal shares the command library
with the serial shell, including file commands, chmod, identity/process/system
inspection and child execution. Completed editor saves refresh the file list.

Calculator evaluates signed 64-bit integer expressions with parentheses, unary
signs and `+ - * /` precedence. Division truncates toward zero. Overflow, division
by zero, invalid input and nesting deeper than eight are rejected. Expressions
are limited to 63 bytes; its keypad provides basic operands/operators, while
keyboard input also supports parentheses.

Monitor shows a real PMM memory gauge and task snapshots, including PIDs, coarse
user runtime ticks and names. It refreshes every second while open or on click.
Settings shows the session identity/version, explains RAM persistence, and saves
Ocean/Slate appearance under `/home/.settings`. A save error preserves the
previous preference. These settings reset when the RAM filesystem is rebooted.

Interactive sessions now start after the bounded boot acceptance checks. Exiting
a session reclaims its process and display buffer, restores the welcome screen
and halts; post-session tests do not modify user files.

Run `./scripts/env.sh make test-applications`. BIOS/UEFI acceptance types and
edits a file, saves with Ctrl+S, checks the unsaved-close guard, reopens and reads
actual private guest memory to verify exact file contents. It also checks the
expression result, real monitor values, saved preference data and clean exit.
Evidence and unmodified screenshots are under `build/tests/applications-*`.
The default native app batch additionally fills the RAM quota, proves a failed
save preserves the original file and reclaims every test block.
