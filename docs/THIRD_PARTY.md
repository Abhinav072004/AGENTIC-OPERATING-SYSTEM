# Third-party components

AstraOS source retains the repository's Apache-2.0 license. The bitmap welcome
glyphs were authored for this project. Host development tools are separate
programs, not linked into the kernel.

Limine v8.7.0 binary distribution is downloaded from upstream at commit
`aad3edd370955449717a334f0289dee10e2c5f01`. The archive checksum is in
`boot/limine.lock`. Preserve its included `LICENSE` in the dependency cache;
its `limine.h` carries its own BSD Zero Clause notice. Do not relabel upstream
files as AstraOS-authored code. For redistributing a binary ISO, distribute
the matching upstream license alongside the image. The build copies it both
inside the ISO (`/LIMINE-LICENSE.txt`) and beside it in `build/`.

QEMU, SeaBIOS and OVMF run outside AstraOS for development/testing and retain
their respective package copyright and license files under the local tools
directory (`usr/share/doc`). They are not custom AstraOS drivers.
