#!/usr/bin/env python3
"""Prove CPL3/CR3, checked syscalls, contained faults, UART input and reclamation."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import socket
import subprocess
import sys
import tempfile
import time

from boot_test import QMP, validate_diagnostics, validate_screen
from interrupt_boot_test import register, validate_tables
from qemu import REPO, command


def validate(text, registers, gdt, user_registers, scenario):
    for marker in ("kernel entered", "PHASE1 PASS", "PHASE2 PASS", "PHASE3 PASS", "PHASE4 PASS", "PHASE5 PASS"):
        assert text.count("AGENTICOS: " + marker) == 1
    assert "[ERROR]" not in text and "recursive exception" not in text
    assert "HLT=1" in registers and "CPL=0" in registers and register(registers, "RIP") >= 0xFFFFFFFF80000000
    assert not register(registers, "RFL") & 0x200
    root = re.search(r"activating kernel CR3=0x([0-9a-f]+)", text)
    assert root and register(registers, "CR3") == int(root[1], 16)
    assert register(registers, "CR0") & (1 << 16) and register(registers, "EFER") & (1 << 11)
    validate_tables(text, registers, gdt)
    entries = re.findall(r"^\[(\d+)\] \[(?:DEBUG|INFO|WARNING|ERROR|PANIC)\]", text, re.MULTILINE)
    assert list(map(int, entries)) == list(range(1, len(entries) + 1))
    if scenario == "reject":
        assert "AGENTICOS: PHASE6 PASS" not in text and text.count("[PANIC]") == 1
        assert re.search(r"\[PANIC\] \[panic\] tests/guest/user_probe.c:\d+ user_boot_failure_probe: assertion failed: user_process_start", text)
        return
    validate_diagnostics(text, "normal", "INFO")
    assert text.count("AGENTICOS: PHASE6 PASS") == 1
    assert register(registers, "CR0") & 0xc == 0xc
    assert user_registers and "CPL=3" in user_registers
    assert re.search(r"\bCS\s*=002b\s+", user_registers) and re.search(r"\bSS\s*=0033\s+", user_registers)
    assert 0x400000 <= register(user_registers, "RIP") < 0x800000
    assert 0x70000000 <= register(user_registers, "RSP") < 0x70004000
    assert register(user_registers, "CR3") != int(root[1], 16) and not register(user_registers, "RFL") & 0x3000
    entry_records = re.findall(r"\[user-enter\] case=(\w+) pid=(\d+) cr3=0x([0-9a-f]+) cs=0x2b ss=0x33 rsp0=0x([0-9a-f]+) frame=0x([0-9a-f]+)", text)
    entry_records = [item for item in entry_records if item[0] != "filesystem"]
    assert text.count("AGENTICOS: FS USER PASS") == 1
    rounds = 4 if scenario == "stress" else 1
    assert len(entry_records) == 17 + 2 * rounds + (1 if scenario == "io" else 0)
    assert len({item[1] for item in entry_records}) == len(entry_records)
    assert all(0 <= int(item[3], 16) - int(item[4], 16) <= 16384 for item in entry_records)
    assert register(user_registers, "CR3") in {int(item[2], 16) for item in entry_records}
    pairs = re.findall(r"\[user-test\] round=(\d+) roots=0x([0-9a-f]+),0x([0-9a-f]+) data_frames=0x([0-9a-f]+),0x([0-9a-f]+); CPL3 preemption, private data and five checked syscalls passed", text)
    assert [int(item[0]) for item in pairs] == list(range(1, rounds + 1))
    assert all(item[1] != item[2] and item[3] != item[4] for item in pairs)
    faults = {"kernel": (14, 5), "peer": (14, 4), "readonly": (14, 7), "nx": (14, 0x15),
              "cli": (13, 0), "gate": (13, 0x40a), "simd": (6, 0)}
    for case, (vector, error) in faults.items():
        fault = re.search(r"\[user-fault\] case=" + case + r" pid=\d+ vector=(\d+) error=0x([0-9a-f]+) rip=0x([0-9a-f]+) cr2=0x([0-9a-f]+) cs=0x2b; process terminated", text)
        assert fault and int(fault[1]) == vector and int(fault[2], 16) == error, case
        assert f"case={case} status={-256 - vector}; offender reaped, guardian completed, kernel survived" in text
        if case == "kernel":
            assert int(fault[4], 16) == 0xFFFFFFFF80000000
        if case == "peer":
            assert int(fault[4], 16) == 0x60000000
        if case == "nx":
            assert fault[3] == fault[4]
    assert re.search(r"\[user-fault\] case=context pid=\d+ reason=context rip=0x[0-9a-f]+ rsp=0x800000000000; process terminated", text)
    assert "case=context status=-300; offender reaped, guardian completed, kernel survived" in text
    assert text.count("AGENTICOS: USER SYSCALL PASS") == rounds
    assert text.count("AGENTICOS: USER ISOLATION PASS") == 1 and text.count("AGENTICOS: USER RECLAIM PASS") == 1
    assert re.search(r"reclaimed all processes: frames=\d+ tables=\d+ mappings=\d+; kernel CR3 restored", text)
    assert "Hello from native AgenticOS user mode" in text and "fd=2" in text and "bytes=128" in text
    if scenario == "stress":
        assert re.search(r"partial loader OOM rolled back; all \d+ frames restored", text)
    if scenario == "io":
        assert text.count("AGENTICOS: USER READ PASS") == 1 and 'text="Z" bytes=1' in text
    else:
        assert "AGENTICOS: USER READ PASS" not in text


def drain(channel, serial):
    while True:
        try:
            data = channel.recv(65536)
        except BlockingIOError:
            return
        if not data:
            return
        with serial.open("ab") as stream:
            stream.write(data)


def run(args):
    name = f"user-{args.firmware}-{args.scenario}-{args.memory}m" + ("-" + args.label if args.label else "")
    output = REPO / "build/tests" / name
    output.mkdir(parents=True, exist_ok=True)
    for name in ("serial.log", "qemu.log", "registers.txt", "user-registers.txt", "gdt-memory.txt", "screen.ppm", "screen.png", "command.json"):
        (output / name).unlink(missing_ok=True)
    iso, elf = args.iso.resolve(strict=True), args.elf.resolve(strict=True)
    serial = output / "serial.log"
    with tempfile.TemporaryDirectory(prefix="agenticos-user-") as temporary:
        qmp_path, uart_path = Path(temporary) / "qmp.sock", Path(temporary) / "uart.sock"
        cmd = command(args.firmware, temporary, headless=True, iso=iso)
        cmd[cmd.index("-m") + 1] = f"{args.memory}M"
        cmd += ["-qmp", f"unix:{qmp_path},server=on,wait=off"]
        cmd += ["-serial", f"unix:{uart_path},server=on,wait=off" if args.scenario == "io" else f"file:{serial}"]
        metadata = {"command": cmd, "scenario": args.scenario, "memory_mib": args.memory,
                    "image_sha256": hashlib.sha256(iso.read_bytes()).hexdigest(),
                    "elf_sha256": hashlib.sha256(elf.read_bytes()).hexdigest(), "serial_input": []}
        (output / "command.json").write_text(json.dumps(metadata, indent=2) + "\n")
        qmp, channel, user_registers, injected = None, None, "", False
        with (output / "qemu.log").open("wb") as log:
            process = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
            try:
                expected = "[PANIC] [panic]" if args.scenario == "reject" else "AGENTICOS: PHASE7 PASS\n"
                deadline = time.monotonic() + args.timeout
                while time.monotonic() < deadline:
                    if args.scenario == "io" and channel is None and uart_path.exists():
                        channel = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                        channel.connect(str(uart_path)); channel.setblocking(False)
                    if channel is not None:
                        drain(channel, serial)
                    text = serial.read_text(errors="replace") if serial.exists() else ""
                    decoded = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", text)
                    if process.poll() is not None:
                        raise RuntimeError(f"QEMU exited early ({process.returncode})")
                    if "PANIC:" in decoded or "[ERROR]" in text or (args.scenario != "reject" and "[PANIC]" in text):
                        raise RuntimeError(decoded)
                    if args.scenario != "reject" and "[user-enter]" in text and not user_registers:
                        if qmp is None:
                            qmp = QMP(qmp_path)
                        candidate = qmp.execute("human-monitor-command", {"command-line": "info registers"})
                        if "CPL=3" in candidate and 0x70000000 <= register(candidate, "RSP") < 0x70004000:
                            user_registers = candidate
                            (output / "user-registers.txt").write_text(candidate)
                    if channel is not None and "AGENTICOS: USER IO WAIT\n" in text and not injected:
                        assert channel.send(b"Z") == 1
                        injected = True; metadata["serial_input"].append("Z after USER IO WAIT")
                        (output / "command.json").write_text(json.dumps(metadata, indent=2) + "\n")
                    if expected in text:
                        break
                    time.sleep(0.01)
                else:
                    raise TimeoutError(f"Missing user/{args.scenario} result after {args.timeout}s")
                time.sleep(0.15)
                if channel is not None:
                    drain(channel, serial)
                if qmp is None:
                    qmp = QMP(qmp_path)
                registers = qmp.execute("human-monitor-command", {"command-line": "info registers"})
                (output / "registers.txt").write_text(registers)
                gdt = re.search(r"\bGDT=\s*([0-9a-f]+)", registers); assert gdt
                memory = qmp.execute("human-monitor-command", {"command-line": f"x /7gx 0x{gdt[1]}"})
                (output / "gdt-memory.txt").write_text(memory)
                validate(serial.read_text(), registers, memory, user_registers, args.scenario)
                assert args.scenario != "io" or injected
                qmp.execute("screendump", {"filename": str(output / "screen.ppm")})
                screen = validate_screen(output / "screen.ppm", output / "screen.png")
                qmp.execute("quit"); assert process.wait(timeout=5) == 0
                print(f"PASS: {args.firmware.upper()} user/{args.scenario} {args.memory}MiB; validated privilege/isolation outcome, {screen}")
                print(f"Evidence: {output.relative_to(REPO)}")
            finally:
                if channel is not None:
                    channel.close()
                if qmp is not None:
                    qmp.close()
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill(); process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", required=True, choices=("bios", "uefi"))
    parser.add_argument("--scenario", choices=("normal", "stress", "io", "reject"), default="normal")
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
        print(f"FAIL: user/{args.scenario} {args.firmware}: {error}; see build/tests", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
