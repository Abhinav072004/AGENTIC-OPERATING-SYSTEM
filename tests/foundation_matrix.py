#!/usr/bin/env python3
"""Build isolated diagnostic ISOs and verify normal/fatal behavior on both firmware types."""
from pathlib import Path
import hashlib
import subprocess
import sys

repo = Path(__file__).resolve().parents[1]
variants = (("logs", "DEBUG"), ("logs", "WARNING"), ("panic", "PANIC"), ("assert", "DEBUG"))
for mode, level in variants:
    build = f"build/scenarios/{mode}-{level.lower()}"
    subprocess.run(["make", "-j2", f"BUILD={build}", f"BOOT_TEST={mode}",
                    f"KERNEL_LOG_LEVEL={level}", "iso", "check-elf"], cwd=repo, check=True)
    for firmware in ("bios", "uefi"):
        subprocess.run([sys.executable, "tests/boot_test.py", "--firmware", firmware,
                        "--scenario", mode, "--minimum-level", level,
                        "--iso", str(repo / build / "agenticos.iso")], cwd=repo, check=True)
print("PASS: 8 diagnostic boots: all levels, filtering, panic and assertion across BIOS/UEFI")

# Prove that reusing one output directory does not retain a stale self-test or
# log-level setting. The restored kernel must match its original normal ELF.
build = "build/scenarios/reconfigure"
hashes = []
for mode, level in (("none", "INFO"), ("logs", "DEBUG"), ("none", "INFO")):
    subprocess.run(["make", "-j2", f"BUILD={build}", f"BOOT_TEST={mode}",
                    f"KERNEL_LOG_LEVEL={level}", "iso"], cwd=repo, check=True)
    hashes.append(hashlib.sha256((repo / build / "kernel.elf").read_bytes()).digest())
assert hashes[0] != hashes[1] and hashes[0] == hashes[2], "Configuration rebuild/restoration failed"
subprocess.run([sys.executable, "tests/boot_test.py", "--firmware", "bios",
                "--evidence-label", "restored",
                "--iso", str(repo / build / "agenticos.iso")], cwd=repo, check=True)
print("PASS: same-directory INFO -> DEBUG/logs -> INFO restores the normal kernel and boots")
