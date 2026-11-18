# Tested device drivers

The target is one x86_64 QEMU q35 CPU. UART, framebuffer and PIC/PIT foundations
remain in use. PCI configuration mechanism 1 enumerates buses, slots and functions
and records up to 64 devices. Configuration access is serialized on the CPU.

PS/2 uses the translated set-1 keyboard and standard three-byte relative mouse.
The controller's device IRQs are disabled; polling drains at most 64 bytes/call.
The decoder handles shift, printable keys, backspace/enter and extended arrows;
mouse packets validate synchronization and overflow and sign-extend movement.
A 63-event queue counts drops. This is not USB, hotplug or a full international
keyboard stack.

ATA supports a primary-master legacy PIO device, LBA28 and 512-byte sectors.
IDENTIFY supplies capacity. Every status wait is bounded; out-of-capacity requests
fail before I/O. Writes flush the device cache. The test runner constructs a
private 1 MiB raw disk, attaches an ISA IDE controller and verifies that only the
expected sector changed. Default runs attach no data disk; the boot CD is ATAPI
and is not treated as an ATA data device. No persistent filesystem is mounted.

Intel 82540EM (`8086:100e`, QEMU e1000) uses a 32-bit MMIO BAR, mapped uncached and
supervisor RW/NX. The driver owns page-aligned DMA descriptor rings and separate
buffers. Each ring contains 16 descriptors. Ownership changes use memory fences;
RX rejects errors, fragments and oversized frames. TX is bounded and does not
reuse a descriptor still owned by hardware. Device interrupts are disabled.
The Phase 8 test sends and receives 24 real Ethernet frames through QEMU's UDP
socket backend on loopback, exercising wraparound. Ethernet protocols are Phase 10.

Run `./scripts/env.sh make test-drivers`. Evidence, exact command, ISO digest,
registers and packet/disk results are under `build/tests/drivers-{bios,uefi}`.
The test never attaches a host block device or sends traffic outside loopback.
Physical hardware, USB, NVMe/AHCI, other NICs, SMP and interrupt-driven I/O are
outside the tested driver set.
