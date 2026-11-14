#!/usr/bin/env python3
"""An expected-fault test image must never pass normal memory readiness."""
import subprocess
import sys

from memory_boot_test import check_boot_errors
from qemu import REPO

# UEFI serial panic text may be interleaved with cursor-position controls.
try:
    check_boot_errors("\x1b[1;1HP\x1b[1;2HA\x1b[1;3HN\x1b[1;4HI\x1b[1;5HC: High memory allocator: Out of memory")
except RuntimeError as error:
    assert "PANIC: High memory allocator: Out of memory" in str(error)
else:
    raise AssertionError("Bootloader panic went undetected")

iso = REPO / "build/memory-scenarios/readonly/agenticos.iso"
result = subprocess.run([sys.executable, "tests/memory_boot_test.py", "--firmware", "bios",
                         "--iso", str(iso), "--label", "rejected-fault", "--timeout", "8"],
                        cwd=REPO, capture_output=True, text=True)
(REPO / "build/tests/memory-runner-negative.log").write_text(result.stdout + result.stderr)
assert result.returncode == 1 and "Missing expected memory result" in result.stderr
serial = (REPO / "build/tests/memory-bios-normal-128m-rejected-fault/serial.log").read_text()
assert "AGENTICOS: PAGEFAULT PASS" in serial and "AGENTICOS: PHASE2 PASS" in serial
assert "AGENTICOS: PHASE3 PASS" not in serial
print("PASS: normal readiness rejects an expected-fault image; ANSI bootloader panics are detected")
