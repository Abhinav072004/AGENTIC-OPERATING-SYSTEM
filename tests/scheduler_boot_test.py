#!/usr/bin/env python3
"""Verify real task contexts, preemption, synchronization and negative readiness."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

from boot_test import QMP, validate_diagnostics, validate_screen
from interrupt_boot_test import register, validate_tables
from qemu import REPO, command


def validate(text, registers, gdt_memory, scenario):
    assert "[ERROR]" not in text and "recursive exception" not in text
    for marker in ("kernel entered", "PHASE1 PASS", "PHASE2 PASS", "PHASE3 PASS", "PHASE4 PASS", "IRET PASS", "TIMER PASS"):
        assert text.count("AGENTICOS: " + marker) == 1, marker
    sequence = re.findall(r"^\[(\d+)\] \[(?:DEBUG|INFO|WARNING|ERROR|PANIC)\]", text, re.MULTILINE)
    assert list(map(int, sequence)) == list(range(1, len(sequence) + 1))
    assert "HLT=1" in registers and "CPL=0" in registers
    assert register(registers, "RIP") >= 0xFFFFFFFF80000000 and not register(registers, "RFL") & 0x200
    assert register(registers, "CR0") & (1 << 16) and register(registers, "EFER") & (1 << 11)
    root = re.search(r"activating kernel CR3=0x([0-9a-f]+)", text)
    assert root and register(registers, "CR3") == int(root[1], 16)
    validate_tables(text, registers, gdt_memory)
    if scenario in ("normal", "stress"):
        validate_diagnostics(text, "normal", "INFO")
        assert text.count("AGENTICOS: PHASE5 PASS") == 1 and "[PANIC]" not in text
        assert text.count("AGENTICOS: PHASE6 PASS") == 1
        rounds = 4 if scenario == "stress" else 1
        for marker in ("COOPERATIVE PASS", "PREEMPT PASS", "SYNC PASS", "RECLAIM PASS"):
            assert text.count("AGENTICOS: " + marker) == rounds, marker
        assert text.count("capacity=15; invalid/capacity failures, cancellation and stack guards passed") == 1
        for round_id in range(1, rounds + 1):
            assert f"cooperative round={round_id} order=ABABAB; 15 GPRs/RSP/CF/DF" in text
            assert f"sync round={round_id} mutex_updates=8 tokens=2; sleep/idle, blocking join, cancellation and stale handles passed; join cycles passed" in text
        preemptions = re.findall(r"preemptive round=(\d+) workers=3 timer_switches=(\d+) quantum=(\d+); non-yielding progress and 15 GPRs/RSP/DF passed", text)
        assert len(preemptions) == rounds
        assert [int(item[0]) for item in preemptions] == list(range(1, rounds + 1))
        assert all(int(item[1]) >= 3 and int(item[2]) == (1 if scenario == "stress" else 2) for item in preemptions)
        reclaims = re.findall(r"reclaim round=(\d+) free_frames=(\d+) table_pages=(\d+) mappings=(\d+); no task resources retained", text)
        assert len(reclaims) == rounds and len({item[1:] for item in reclaims}) == 1
        assert [int(item[0]) for item in reclaims] == list(range(1, rounds + 1))
        if scenario == "stress":
            oom = re.search(r"real stack OOM rejected spawn; all (\d+) frames recovered", text)
            assert oom and int(oom[1]) > 0
        else:
            assert "real stack OOM" not in text
        return
    assert "AGENTICOS: PHASE5 PASS" not in text
    prefix = re.split(r"^\[\d+\] \[PANIC\]", text, maxsplit=1, flags=re.MULTILINE)[0]
    validate_diagnostics(prefix, "normal", "INFO")
    if scenario == "guard":
        assert text.count("AGENTICOS: EXCEPTION HALT") == 1 and "[PANIC] [panic]" not in text
        assert "[exception] vector=14 name=page fault error=0x0" in text
        assert register(registers, "CR2") == 0xFFFFD20000006000
        saved = re.search(r"\[exception\] cs=0x8 ss=0x10 rflags=0x([0-9a-f]+) rsp=0x([0-9a-f]+) stack=0", text)
        assert saved and int(saved[1], 16) & 0x200
        assert 0xFFFFD20000007000 <= int(saved[2], 16) < 0xFFFFD2000000B000
        assert "[page-fault] present=0 write=0 user=0 reserved=0 fetch=0" in text
    else:
        assert text.count("[PANIC]") == 1 and "EXCEPTION HALT" not in text
        assert re.search(r"\[PANIC\] \[panic\] kernel/scheduler/task.c:\d+ scheduler_interrupt: scheduler watchdog expired", text)


def run(args):
    name = f"scheduler-{args.firmware}-{args.scenario}-{args.memory}m"
    if args.label:
        name += "-" + args.label
    output = REPO / "build/tests" / name
    output.mkdir(parents=True, exist_ok=True)
    for name in ("serial.log", "qemu.log", "registers.txt", "gdt-memory.txt", "screen.ppm", "screen.png", "command.json"):
        (output / name).unlink(missing_ok=True)
    iso, elf = args.iso.resolve(strict=True), args.elf.resolve(strict=True)
    serial = output / "serial.log"
    with tempfile.TemporaryDirectory(prefix="agenticos-task-") as temporary:
        qmp_path = Path(temporary) / "qmp.sock"
        cmd = command(args.firmware, temporary, headless=True, iso=iso)
        cmd[cmd.index("-m") + 1] = f"{args.memory}M"
        cmd += ["-serial", f"file:{serial}", "-qmp", f"unix:{qmp_path},server=on,wait=off"]
        (output / "command.json").write_text(json.dumps({"command": cmd, "scenario": args.scenario,
            "memory_mib": args.memory, "image_sha256": hashlib.sha256(iso.read_bytes()).hexdigest(),
            "elf_sha256": hashlib.sha256(elf.read_bytes()).hexdigest()}, indent=2) + "\n")
        qmp = None
        with (output / "qemu.log").open("wb") as log:
            process = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
            try:
                expected = ("[PANIC] [panic]" if args.scenario == "watchdog" else
                            "AGENTICOS: EXCEPTION HALT\n" if args.scenario == "guard" else "AGENTICOS: PHASE12 PASS\n")
                deadline = time.monotonic() + args.timeout
                while time.monotonic() < deadline:
                    text = serial.read_text(errors="replace") if serial.exists() else ""
                    decoded = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", text)
                    if process.poll() is not None:
                        raise RuntimeError(f"QEMU exited early ({process.returncode})")
                    if "PANIC:" in decoded or "[ERROR]" in text or (args.scenario in ("normal", "stress") and "[PANIC]" in text):
                        raise RuntimeError(decoded)
                    if expected in text:
                        break
                    time.sleep(0.025)
                else:
                    raise TimeoutError(f"Missing scheduler/{args.scenario} result after {args.timeout}s")
                time.sleep(0.15)
                qmp = QMP(qmp_path)
                registers = qmp.execute("human-monitor-command", {"command-line": "info registers"})
                (output / "registers.txt").write_text(registers)
                gdt = re.search(r"\bGDT=\s*([0-9a-f]+)", registers)
                assert gdt
                gdt_memory = qmp.execute("human-monitor-command", {"command-line": f"x /7gx 0x{gdt[1]}"})
                (output / "gdt-memory.txt").write_text(gdt_memory)
                validate(serial.read_text(), registers, gdt_memory, args.scenario)
                qmp.execute("screendump", {"filename": str(output / "screen.ppm")})
                screen = validate_screen(output / "screen.ppm", output / "screen.png")
                qmp.execute("quit")
                assert process.wait(timeout=5) == 0
                print(f"PASS: {args.firmware.upper()} scheduler/{args.scenario} {args.memory}MiB; validated task outcome, {screen}")
                print(f"Evidence: {output.relative_to(REPO)}")
            finally:
                if qmp is not None:
                    qmp.close()
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", required=True, choices=("bios", "uefi"))
    parser.add_argument("--scenario", choices=("normal", "stress", "guard", "watchdog"), default="normal")
    parser.add_argument("--memory", type=int, choices=(32, 128, 256, 512), default=128)
    parser.add_argument("--iso", type=Path, default=REPO / "build/agenticos.iso")
    parser.add_argument("--elf", type=Path, default=REPO / "build/kernel.elf")
    parser.add_argument("--timeout", type=float, default=45)
    parser.add_argument("--label", default="")
    args = parser.parse_args()
    if args.label and not re.fullmatch(r"[a-z0-9-]+", args.label):
        parser.error("--label must contain lowercase letters, digits or hyphens")
    try:
        run(args)
    except (AssertionError, OSError, RuntimeError, TimeoutError, subprocess.SubprocessError) as error:
        print(f"FAIL: scheduler/{args.scenario} {args.firmware}: {error}; see build/tests", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
