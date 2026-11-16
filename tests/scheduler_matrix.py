#!/usr/bin/env python3
"""Task acceptance across firmware/RAM plus stress and deliberate failures."""
from pathlib import Path
import subprocess
import sys

repo = Path(__file__).resolve().parents[1]
cases = (("normal", "none"), ("stress", "scheduler-stress"),
         ("guard", "scheduler-guard"), ("watchdog", "scheduler-watchdog"))
executed = 0
for scenario, mode in cases:
    build = f"build/scheduler-scenarios/{scenario}"
    subprocess.run(["make", "-j2", f"BUILD={build}", f"BOOT_TEST={mode}",
                    "KERNEL_LOG_LEVEL=INFO", "iso", "check-elf"], cwd=repo, check=True)
    for firmware in ("bios", "uefi"):
        sizes = ((32, 128, 512) if firmware == "bios" else (128, 256, 512)) if scenario == "normal" else (128,)
        for ram in sizes:
            subprocess.run([sys.executable, "tests/scheduler_boot_test.py", "--firmware", firmware,
                            "--scenario", scenario, "--memory", str(ram), "--iso", str(repo / build / "agenticos.iso"),
                            "--elf", str(repo / build / "kernel.elf")], cwd=repo, check=True)
            executed += 1
print(f"PASS: {executed} scheduler guest checks: RAM/firmware, cooperative/preemptive contexts, synchronization, stress, guards and watchdog")
result = subprocess.run([sys.executable, "tests/boot_test.py", "--firmware", "bios",
                        "--iso", str(repo / "build/scheduler-scenarios/guard/agenticos.iso"),
                        "--evidence-label", "rejected-week5-fault", "--timeout", "8"],
                        cwd=repo, capture_output=True, text=True)
(repo / "build/tests/scheduler-runner-negative.log").write_text(result.stdout + result.stderr)
assert result.returncode == 1 and "[PANIC] [exception] vector=14" in result.stderr
text = (repo / "build/tests/bios-rejected-week5-fault/serial.log").read_text()
assert "AGENTICOS: PHASE4 PASS" in text and "AGENTICOS: PHASE5 PASS" not in text
print("PASS: normal readiness rejects a task guard fault after PHASE4 completion")
