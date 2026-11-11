#!/usr/bin/env python3
"""Validate the actual linked image without a host ELF library dependency."""
from pathlib import Path
import struct
import sys

data = Path(sys.argv[1]).read_bytes()
assert data[:7] == b"\x7fELF\x02\x01\x01", "Expected little-endian ELF64"
header = struct.unpack_from("<HHIQQQIHHHHHH", data, 16)
kind, machine, _, entry, offset, _, _, _, phsize, count, *_ = header
assert kind == 2 and machine == 62, "Expected static x86_64 executable"
assert entry >= 0xFFFFFFFF80000000, "Expected a higher-half entry point"
assert phsize == 56 and count > 0
executable_entry = False
loads = 0
for index in range(count):
    ptype, flags, _, virtual, _, filesz, memsz, align = struct.unpack_from(
        "<IIQQQQQQ", data, offset + index * phsize)
    assert ptype not in (2, 3), "Kernel must not depend on a dynamic linker"
    if ptype == 1:
        loads += 1
        assert flags & 3 != 3, "Writable executable LOAD segment"
        assert filesz <= memsz
        assert align == 4096
        executable_entry |= bool(flags & 1 and virtual <= entry < virtual + memsz)
    if ptype == 0x6474E551:
        assert not flags & 1, "Executable stack requested"
assert loads == 4 and executable_entry, "Missing kernel segments/entry"
print("PASS: ELF64 x86_64, static, higher-half entry, four W^X LOAD segments")
