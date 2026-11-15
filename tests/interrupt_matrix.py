#!/usr/bin/env python3
"""Production descriptor/timer acceptance and isolated real exception probes."""
from pathlib import Path
import subprocess
import sys

repo = Path(__file__).resolve().parents[1]
cases = (("normal", "none"),) + tuple((name, "exception-" + name) for name in
        ("divide", "ud", "gp", "pf", "df", "nmi", "ro", "nx"))
executed = 0
for scenario, mode in cases:
    build = f"build/interrupt-scenarios/{scenario}"
    subprocess.run(["make", "-j2", f"BUILD={build}", f"BOOT_TEST={mode}",
                    "KERNEL_LOG_LEVEL=INFO", "iso", "check-elf"], cwd=repo, check=True)
    for firmware in ("bios", "uefi"):
        subprocess.run([sys.executable, "tests/interrupt_boot_test.py", "--firmware", firmware,
                        "--scenario", scenario, "--iso", str(repo / build / "agenticos.iso"),
                        "--elf", str(repo / build / "kernel.elf")], cwd=repo, check=True)
        executed += 1
print(f"PASS: {executed} interrupt guest checks: descriptor state, IRETQ, PIT/EOI, CPU exceptions, double-fault IST and injected NMI")

# Phase 3 completion is not sufficient for Week 4 readiness. A real fatal image
# must be rejected by the normal runner even after PHASE3 and IRET pass.
result = subprocess.run([sys.executable, "tests/boot_test.py", "--firmware", "bios",
                        "--iso", str(repo / "build/interrupt-scenarios/divide/agenticos.iso"),
                        "--evidence-label", "rejected-week4-fault", "--timeout", "8"],
                        cwd=repo, capture_output=True, text=True)
(repo / "build/tests/interrupt-runner-negative.log").write_text(result.stdout + result.stderr)
assert result.returncode == 1 and "[PANIC] [exception] vector=0" in result.stderr
text = (repo / "build/tests/bios-rejected-week4-fault/serial.log").read_text()
assert "AGENTICOS: PHASE3 PASS" in text and "AGENTICOS: PHASE4 PASS" not in text
print("PASS: normal readiness rejects a fatal exception after PHASE3/IRET completion")
