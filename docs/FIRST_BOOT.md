# Understanding the first boot

## 1. Firmware finds a bootloader

QEMU emulates a PC. SeaBIOS or OVMF initializes it and reads the ISO's El Torito
boot entries. The ISO contains both BIOS and x86_64 UEFI Limine assets. For UEFI,
`EFI/BOOT/BOOTX64.EFI` is also included. `limine.conf` selects the kernel ELF,
requests an 800x600 framebuffer and boots immediately.

Limine is a bootloader, not our kernel. It loads the ELF, sets up long mode and
initial virtual memory, discovers a framebuffer, and jumps to our ELF entry.
Using it lets us focus on kernel development instead of writing a BIOS/UEFI
loader at the same time.

## 2. The linker defines where code lives

A compiler turns C into object files; NASM does the same for assembly. The linker
combines them into an ELF executable. `linker.ld` selects `_start`, places the
kernel in the upper part of the 64-bit virtual address space, and assigns
segment permissions. Debug symbols remain in the ELF for GDB.

Physical RAM is the actual storage. A virtual address is what an instruction
uses; page tables translate it to physical storage. Our high virtual address
does not mean the PC has that much RAM. Limine maps the loaded kernel there.
AgenticOS will create and manage its own mappings in Phase 3.

## 3. Assembly establishes our C calling environment

`entry.asm` keeps interrupts disabled because no IDT exists, clears the direction
flag, selects our zero-initialized 64 KiB stack and calls `kernel_main`.
The stack pointer is aligned to 16 bytes before `call`, as required by the System
V AMD64 ABI. `call` pushes the return address. A stack provides temporary storage
and return addresses, not a general heap. The kernel never returns to firmware.

Limine already supplies long mode, segment registers, initial mappings and a
temporary stack. The small assembly entry does not reimplement those features.

## 4. C validates the boot contract

Special request structures in the ELF ask Limine for protocol base revision 3
and a framebuffer. Limine writes response pointers into them. `volatile` makes
these external changes visible to C; `used` and `KEEP` prevent removal of the
otherwise unreferenced protocol structures.

The kernel initializes COM1, reports entry, checks the protocol response, then
draws the welcome. An unsupported framebuffer or protocol logs a specific error
and halts; the success marker is only printed after drawing succeeds.

## 5. Pixels and serial bytes are different output paths

A framebuffer is mapped device memory holding pixel values. Its `pitch` is the
byte distance between rows, which may include padding. The renderer checks
geometry, alignment, overflow and non-overlapping RGB masks. It supports only
32-bit packed RGB with 8 bits per channel. Other modes are explicitly rejected.

The welcome uses original 5x7 bitmap glyphs scaled by three. It is deliberately
a tiny first-boot renderer, not a terminal, font engine, window system or GUI.

COM1 is an emulated 16550-compatible UART at port `0x3f8`. We configure 115200,
8N1 and poll its transmitter with a finite bound. QEMU routes its bytes to a
host file or terminal. Week 2 adds internal loopback verification and structured
logging over this path; see [kernel foundation](KERNEL_FOUNDATION.md). A missing
UART fails the bootstrap assertion; output can be unavailable on that hardware.

## 6. Halt is success for this milestone

The original first-boot milestone printed its Phase 1 success marker and then
executed `cli; hlt`. Week 2 completes its foundation diagnostics and prints
`AGENTICOS: PHASE2 PASS` before entering that same halt loop.
There is no scheduler or input handling yet. The boot test checks the serial
marker, captures actual display pixels, and asks QEMU for CPU registers to
verify the CPU is halted inside our higher-half kernel rather than rebooting.
The host terminates QEMU through QMP after recording evidence.

## Current limits

- Initial page tables and GDT are still Limine's; no AgenticOS memory manager.
- No exception handlers, IDT, recovery, SMP, users, permissions or processes.
- No input, files, persistent storage, networking, model runtime or desktop.
- Only QEMU `q35`/`qemu64`/standard VGA/COM1 were tested; no physical hardware.
- Interrupts are disabled, but this is not protection against NMIs or all faults.
- UEFI tests use OVMF without Secure Boot; the image is not signed.

Protocol references (matched to the dependency pin):
[Limine protocol](https://github.com/limine-bootloader/limine/blob/v8.7.0/PROTOCOL.md),
[configuration](https://github.com/limine-bootloader/limine/blob/v8.7.0/CONFIG.md).
