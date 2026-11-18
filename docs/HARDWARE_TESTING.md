# Physical hardware test procedure

No physical hardware boot, disk installation or existing-OS replacement has been
performed. The release is verified in QEMU only. This document is a procedure
for a separately authorized future test, not an installer or a claim of hardware
compatibility. The owner's current authorization covers repository work and
QEMU; physical installation still requires a separate explicit decision.

The kernel expects x86_64 long mode, NX, a Limine-supported RGB32 framebuffer,
legacy COM1/PIC/PIT and the tested PS/2/ATA/e1000 device models. Modern laptops
may lack those devices, use USB-only input or use NVMe/AHCI/Wi-Fi hardware without
a matching driver. APIC/SMP, Secure Boot, ACPI power management and broad hardware
support are not implemented. Keep firmware Secure Boot requirements in mind;
the image has no trusted signing chain.

Before any approved attempt:

1. Record the machine/firmware, CPU and available devices. Compare them with the
   supported set in DRIVERS.md and the limitations in SECURITY.md.
2. Back up valuable data independently. Prefer a spare machine and physically
   disconnect storage that must not be modified. Obtain the owner's approval
   for the exact removable device and exact operation before writing any media.
3. Build/verify a clean release and SHA256SUMS. Use a dedicated removable device
   with no valuable data. There is intentionally no disk-writing command here;
   it would need the exact approved device to be safe and reviewable.
4. Arrange serial capture, camera/screen evidence and a recovery firmware boot
   path. Attempt a removable-media boot only. Do not install to an internal disk.
5. Record whether firmware hands control to the kernel, framebuffer mode,
   serial diagnostics, memory map, exceptions, timer, input and optional devices.
   A missing driver is an unsupported device, not permission to probe/write disks.
6. Stop on unexpected storage access, repeated faults or missing recovery path.
   Power off and restore the original firmware boot selection; keep all evidence.

Any actual attempt needs a new report identifying the exact machine, firmware,
media and observed results. QEMU results cannot establish physical compatibility.
