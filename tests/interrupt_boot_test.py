#!/usr/bin/env python3
"""Check actual descriptor registers, returning IRQs and production fatal ISRs."""
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
from qemu import REPO, command

FAULTS = {"divide": (0, 0), "ud": (6, 0), "gp": (13, 0x38), "pf": (14, 0),
          "df": (8, 0), "nmi": (2, 0), "ro": (14, 3), "nx": (14, 0x11)}


def register(text, name):
    match = re.search(r"\b" + name + r"=([0-9a-f]+)", text)
    assert match, f"Missing register {name}"
    return int(match[1], 16)


def validate_tables(text, registers, gdt_memory):
    match = re.search(r"\[interrupt\] GDT=0x([0-9a-f]+) limit=55 IDT=0x([0-9a-f]+) limit=4095 CS=0x8 SS=0x10 TR=0x18", text)
    assert match, "No owned descriptor-table report"
    for name, group, limit in (("GDT", 1, 55), ("IDT", 2, 4095)):
        actual = re.search(r"\b" + name + r"=\s*([0-9a-f]+)\s+([0-9a-f]+)", registers)
        assert actual and int(actual[1], 16) == int(match[group], 16) and int(actual[2], 16) == limit
        assert int(actual[1], 16) >= 0xFFFFFFFF80000000
    # QEMU reports its cached TR as available on this version. Inspect the
    # actual GDT memory independently: LTR must have set the busy descriptor bit.
    assert re.search(r"\bTR\s*=0018\s+[0-9a-f]+\s+00000067.*TSS64-(?:avl|busy)", registers)
    words = [int(word, 16) for line in gdt_memory.splitlines() if ":" in line
             for word in re.findall(r"0x[0-9a-f]+", line.split(":", 1)[1])]
    assert len(words) == 7 and ((words[3] >> 40) & 0xff) == 0x8b, "LTR did not mark the TSS busy"
    assert re.search(r"\bCS\s*=0008\s+", registers)
    assert "dedicated IST: double-fault=1 NMI=2 machine-check=3" in text


def symbols(elf):
    result = subprocess.check_output(["nm", "-n", str(elf)], text=True)
    return {parts[2]: int(parts[0], 16) for line in result.splitlines()
            if len(parts := line.split()) == 3 and re.fullmatch(r"[0-9a-fA-F]+", parts[0])}


def validate_result(text, registers, scenario, elf, gdt_memory):
    assert "[PANIC] [panic]" not in text and "[ERROR]" not in text
    assert "recursive exception" not in text
    entries = re.findall(r"^\[(\d+)\] \[(?:DEBUG|INFO|WARNING|ERROR|PANIC)\]", text, re.MULTILINE)
    assert list(map(int, entries)) == list(range(1, len(entries) + 1))
    for marker in ("kernel entered", "PHASE1 PASS", "PHASE2 PASS", "PHASE3 PASS", "IRET PASS"):
        assert text.count("AGENTICOS: " + marker) == 1
    assert "preserved 15 GPRs, RSP, CF/DF; live DF/IF clear; spurious IRQ7/15 passed" in text
    assert "HLT=1" in registers and "CPL=0" in registers
    assert register(registers, "RIP") >= 0xFFFFFFFF80000000
    assert not register(registers, "RFL") & 0x200
    assert register(registers, "CR0") & (1 << 16) and register(registers, "EFER") & (1 << 11)
    root = re.search(r"activating kernel CR3=0x([0-9a-f]+)", text)
    assert root and register(registers, "CR3") == int(root[1], 16)
    validate_tables(text, registers, gdt_memory)
    if scenario == "normal":
        validate_diagnostics(text, "normal", "INFO")
        assert text.count("AGENTICOS: PHASE4 PASS") == 1
        assert text.count("AGENTICOS: PHASE5 PASS") == 1
        assert text.count("AGENTICOS: PHASE6 PASS") == 1
        assert text.count("AGENTICOS: TIMER PASS") == 1 and "EXCEPTION HALT" not in text
        timer = re.search(r"\[timer\] PIT input_hz=(\d+) divisor=(\d+) ticks=(\d+) eoi=(\d+) saved_IF=1; PIC masked, IF=0", text)
        assert timer and int(timer[1]) == 1193182 and int(timer[2]) == 11932
        assert int(timer[3]) >= 8 and timer[3] == timer[4]
        assert re.search(r"\bSS\s*=0010\s+", registers)
        return
    assert "AGENTICOS: PHASE4 PASS" not in text and "AGENTICOS: TIMER PASS" not in text
    assert text.count("AGENTICOS: EXCEPTION HALT") == 1
    prefix = re.split(r"^\[\d+\] \[PANIC\]", text, maxsplit=1, flags=re.MULTILINE)[0]
    validate_diagnostics(prefix, "normal", "INFO")
    fault = re.search(r"\[exception\] vector=(\d+) name=([^\n]+?) error=0x([0-9a-f]+) rip=0x([0-9a-f]+) cr2=0x([0-9a-f]+)", text)
    assert fault and (int(fault[1]), int(fault[3], 16)) == FAULTS[scenario]
    rip, cr2 = int(fault[4], 16), int(fault[5], 16)
    saved = re.search(r"\[exception\] cs=0x8 ss=0x10 rflags=0x([0-9a-f]+) rsp=0x([0-9a-f]+) stack=(\d+)", text)
    assert saved and not int(saved[1], 16) & 0x200
    assert int(saved[3]) == (1 if scenario == "df" else 2 if scenario == "nmi" else 0)
    for name in ("rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"):
        assert re.search(r"\b" + name + r"=0x[0-9a-f]+\b", text), f"Missing saved {name}"
    names = symbols(elf)
    if scenario in ("divide", "ud", "gp", "pf", "ro"):
        assert rip == names[f"probe_{scenario}_instruction"], f"Wrong fault instruction: {rip:x}"
    if scenario in ("pf", "ro", "nx"):
        expected = "present=0 write=0 user=0 reserved=0 fetch=0" if scenario == "pf" else (
            "present=1 write=1 user=0 reserved=0 fetch=0" if scenario == "ro" else
            "present=1 write=0 user=0 reserved=0 fetch=1")
        assert "[page-fault] " + expected + " pkey=0 shadow_stack=0 sgx=0" in text
        assert register(registers, "CR2") == cr2
    if scenario == "pf":
        assert cr2 == 0xFFFFD00000000000
    elif scenario == "ro":
        assert cr2 == names["__rodata_start"]
    elif scenario == "nx":
        assert rip == cr2 and 0xFFFFD00000001000 <= cr2 < 0xFFFFD00000041000
    elif scenario == "df":
        assert int(saved[2], 16) == 0xFFFFD00000000800
        assert 0xFFFFD00000000000 <= cr2 < 0xFFFFD00000001000
    if scenario != "nx":
        assert rip >= 0xFFFFFFFF80000000


def run(args):
    suffix = f"interrupt-{args.firmware}-{args.scenario}"
    if args.label:
        suffix += "-" + args.label
    output = REPO / "build/tests" / suffix
    output.mkdir(parents=True, exist_ok=True)
    for name in ("serial.log", "qemu.log", "registers.txt", "gdt-memory.txt", "screen.ppm", "screen.png", "command.json"):
        (output / name).unlink(missing_ok=True)
    iso = args.iso.resolve(strict=True)
    elf = args.elf.resolve(strict=True)
    serial = output / "serial.log"
    with tempfile.TemporaryDirectory(prefix="agenticos-irq-") as temporary:
        qmp_path = Path(temporary) / "qmp.sock"
        cmd = command(args.firmware, temporary, headless=True, iso=iso)
        cmd += ["-serial", f"file:{serial}", "-qmp", f"unix:{qmp_path},server=on,wait=off"]
        metadata = {"command": cmd, "scenario": args.scenario,
                    "image_sha256": hashlib.sha256(iso.read_bytes()).hexdigest(),
                    "elf_sha256": hashlib.sha256(elf.read_bytes()).hexdigest(), "qmp_actions": []}
        (output / "command.json").write_text(json.dumps(metadata, indent=2) + "\n")
        qmp = None
        with (output / "qemu.log").open("wb") as log:
            process = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
            try:
                expected = "AGENTICOS: PHASE7 PASS\n" if args.scenario == "normal" else "AGENTICOS: EXCEPTION HALT\n"
                deadline = time.monotonic() + args.timeout
                injected = False
                while time.monotonic() < deadline:
                    text = serial.read_text(errors="replace") if serial.exists() else ""
                    decoded = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", text)
                    if process.poll() is not None:
                        raise RuntimeError(f"QEMU exited early ({process.returncode})")
                    if "PANIC:" in decoded or "[PANIC] [panic]" in text or "[ERROR]" in text:
                        raise RuntimeError(decoded)
                    if args.scenario == "normal" and "[PANIC]" in text:
                        raise RuntimeError(text)
                    if args.scenario == "nmi" and "AGENTICOS: NMI WAIT\n" in text and not injected:
                        qmp = QMP(qmp_path)
                        qmp.execute("inject-nmi")
                        injected = True
                        metadata["qmp_actions"].append("inject-nmi")
                        (output / "command.json").write_text(json.dumps(metadata, indent=2) + "\n")
                    if expected in text:
                        break
                    time.sleep(0.025)
                else:
                    raise TimeoutError(f"Missing interrupt/{args.scenario} result after {args.timeout}s")
                time.sleep(0.15)
                if qmp is None:
                    qmp = QMP(qmp_path)
                registers = qmp.execute("human-monitor-command", {"command-line": "info registers"})
                (output / "registers.txt").write_text(registers)
                gdt = re.search(r"\bGDT=\s*([0-9a-f]+)", registers)
                assert gdt, "Missing actual GDTR"
                gdt_memory = qmp.execute("human-monitor-command", {"command-line": f"x /7gx 0x{gdt[1]}"})
                (output / "gdt-memory.txt").write_text(gdt_memory)
                validate_result(serial.read_text(), registers, args.scenario, elf, gdt_memory)
                assert args.scenario != "nmi" or injected
                qmp.execute("screendump", {"filename": str(output / "screen.ppm")})
                screen = validate_screen(output / "screen.ppm", output / "screen.png")
                qmp.execute("quit")
                assert process.wait(timeout=5) == 0
                print(f"PASS: {args.firmware.upper()} interrupt/{args.scenario}; owned tables, real CPU result, {screen}")
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
    parser.add_argument("--scenario", choices=("normal", *FAULTS), default="normal")
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
        print(f"FAIL: interrupt/{args.scenario} {args.firmware}: {error}; see build/tests", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
