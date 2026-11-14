#!/usr/bin/env python3
"""Build isolated memory fixtures and verify them across RAM sizes/firmware."""
from pathlib import Path
import subprocess
import sys

repo = Path(__file__).resolve().parents[1]
cases = (("normal", "none"), ("readonly", "memory-ro"), ("nx", "memory-nx"),
         ("guard", "memory-guard"), ("oom", "memory-oom"))
executed = 0
for scenario, mode in cases:
    build = f"build/memory-scenarios/{scenario}"
    subprocess.run(["make", "-j2", f"BUILD={build}", "KERNEL_LOG_LEVEL=INFO",
                    f"BOOT_TEST={mode}", "iso", "check-elf"], cwd=repo, check=True)
    for firmware in ("bios", "uefi"):
        # The pinned UEFI/Limine path runs out of boot memory at 64MiB;
        # 32MiB does not reach the kernel. BIOS exercises the lower RAM target.
        ram_sizes = ((32, 128, 512) if firmware == "bios" else (128, 256, 512)) if scenario == "normal" else (128,)
        for ram in ram_sizes:
            subprocess.run([sys.executable, "tests/memory_boot_test.py", "--firmware", firmware,
                            "--scenario", scenario, "--memory", str(ram),
                            "--iso", str(repo / build / "agenticos.iso")], cwd=repo, check=True)
            executed += 1
print(f"PASS: {executed} memory guest checks across BIOS/UEFI, RAM sizes, protection and exhaustion")
