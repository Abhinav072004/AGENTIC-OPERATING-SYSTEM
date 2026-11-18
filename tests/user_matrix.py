#!/usr/bin/env python3
"""Native user acceptance, independent CPU evidence, I/O and failure readiness."""
from pathlib import Path
import subprocess
import sys
repo = Path(__file__).resolve().parents[1]
cases = (("normal", "none"), ("stress", "user-stress"), ("io", "user-io"), ("reject", "user-reject"))
executed = 0
for scenario, mode in cases:
    build = f"build/user-scenarios/{scenario}"
    subprocess.run(["make", "-j2", f"BUILD={build}", f"BOOT_TEST={mode}", "KERNEL_LOG_LEVEL=INFO", "iso", "check-elf"], cwd=repo, check=True)
    for firmware in ("bios", "uefi"):
        sizes = ((32, 128, 512) if firmware == "bios" else (128, 256, 512)) if scenario == "normal" else (128,)
        for ram in sizes:
            subprocess.run([sys.executable, "tests/user_boot_test.py", "--firmware", firmware, "--scenario", scenario,
                            "--memory", str(ram), "--iso", str(repo / build / "agenticos.iso"),
                            "--elf", str(repo / build / "kernel.elf")], cwd=repo, check=True)
            executed += 1
print(f"PASS: {executed} user guest checks: CPL3 CPU capture, private roots, syscalls, eight contained attacks, OOM/stress, UART and invalid boot image")
result = subprocess.run([sys.executable, "tests/boot_test.py", "--firmware", "bios", "--iso",
                        str(repo / "build/user-scenarios/reject/agenticos.iso"), "--evidence-label", "rejected-week6-image", "--timeout", "8"],
                        cwd=repo, capture_output=True, text=True)
(repo / "build/tests/user-runner-negative.log").write_text(result.stdout + result.stderr)
assert result.returncode == 1 and "[PANIC] [panic]" in result.stderr
text = (repo / "build/tests/bios-rejected-week6-image/serial.log").read_text()
assert "AGENTICOS: PHASE5 PASS" in text and "AGENTICOS: PHASE6 PASS" not in text
print("PASS: normal readiness rejects a required user image failure after PHASE5")
