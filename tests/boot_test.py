#!/usr/bin/env python3
"""Boot a real ISO; require kernel success, a halted CPU, and rendered pixels."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import socket
import struct
import subprocess
import sys
import tempfile
import time
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from qemu import REPO, command  # noqa: E402


class QMP:
    def __init__(self, path):
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.socket.settimeout(5)
        self.socket.connect(str(path))
        self.stream = self.socket.makefile("rwb")
        assert "QMP" in self.receive(), "Missing QMP greeting"
        self.execute("qmp_capabilities")

    def receive(self):
        line = self.stream.readline()
        if not line:
            raise RuntimeError("QMP connection closed")
        return json.loads(line)

    def execute(self, name, arguments=None):
        self.stream.write(json.dumps({"execute": name, "arguments": arguments or {}}).encode() + b"\n")
        self.stream.flush()
        while True:
            response = self.receive()
            if "error" in response:
                raise RuntimeError(response["error"])
            if "return" in response:
                return response["return"]

    def close(self):
        self.stream.close()
        self.socket.close()


def validate_screen(ppm, png):
    with ppm.open("rb") as image:
        assert image.readline() == b"P6\n"
        dimensions = image.readline()
        while dimensions.startswith(b"#"):
            dimensions = image.readline()
        width, height = map(int, dimensions.split())
        assert image.readline() == b"255\n"
        pixels = image.read()
    assert width >= 392 and height >= 100
    assert len(pixels) == width * height * 3
    palette = Counter(pixels[i:i + 3] for i in range(0, len(pixels), 3))
    background, foreground = bytes((12, 18, 32)), bytes((225, 235, 250))
    assert set(palette) == {background, foreground}, f"Unexpected display colors: {palette}"
    assert palette[background] > width * height * 0.9
    assert palette[foreground] > 1000, "No visible welcome text"
    # Re-encode the unmodified RGB capture as PNG using the standard library.
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    rows = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
    png.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
                    + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))
    return f"{width}x{height}, {palette[foreground]} text pixels"


def run(args):
    suffix = args.firmware if args.scenario == "normal" else f"{args.firmware}-{args.scenario}-{args.minimum_level.lower()}"
    if args.evidence_label:
        suffix += f"-{args.evidence_label}"
    output = REPO / "build/tests" / suffix
    output.mkdir(parents=True, exist_ok=True)
    serial = output / "serial.log"
    # Never accept a previous run's output as success.
    for filename in ("serial.log", "qemu.log", "screen.ppm", "screen.png", "registers.txt"):
        (output / filename).unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix="agenticos-test-") as workdir:
        qmp_path = Path(workdir) / "qmp.sock"
        cmd = command(args.firmware, workdir, headless=True, iso=args.iso)
        cmd += ["-serial", f"file:{serial}", "-qmp", f"unix:{qmp_path},server=on,wait=off"]
        (output / "command.json").write_text(json.dumps({
            "command": cmd, "scenario": args.scenario,
            "minimum_level": args.minimum_level}, indent=2) + "\n")
        qmp = None
        with (output / "qemu.log").open("wb") as log:
            process = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + args.timeout
                while time.monotonic() < deadline:
                    text = serial.read_text(errors="replace") if serial.exists() else ""
                    if process.poll() is not None:
                        raise RuntimeError(f"QEMU exited early ({process.returncode})")
                    if args.scenario == "normal" and ("[ERROR]" in text or "[PANIC]" in text):
                        raise RuntimeError(text)
                    expected = "[PANIC] [panic]" if args.scenario in ("panic", "assert") else "AGENTICOS: READY\n"
                    if expected in text:
                        break
                    time.sleep(0.05)
                else:
                    raise TimeoutError(f"No expected kernel result within {args.timeout}s")
                # Confirm the machine remains in our final HLT, with no reboot loop.
                time.sleep(0.25)
                qmp = QMP(qmp_path)
                registers = qmp.execute("human-monitor-command", {"command-line": "info registers"})
                (output / "registers.txt").write_text(registers)
                assert re.search(r"RIP=ffffffff800[0-9a-f]+", registers), "CPU left the kernel"
                assert "HLT=1" in registers, "CPU did not reach the final halt"
                flags = re.search(r"RFL=([0-9a-f]+)", registers)
                assert flags and not int(flags[1], 16) & 0x200, "Maskable interrupts unexpectedly enabled"
                text = serial.read_text()
                assert text.count("AGENTICOS: kernel entered") == 1, "Unexpected kernel restart"
                assert text.count("AGENTICOS: PHASE1 PASS") == 1
                validate_diagnostics(text, args.scenario, args.minimum_level)
                if args.scenario not in ("panic", "assert"):
                    assert text.count("AGENTICOS: PHASE3 PASS") == 1
                    assert text.count("AGENTICOS: PHASE4 PASS") == 1
                    assert text.count("AGENTICOS: PHASE5 PASS") == 1
                    assert text.count("AGENTICOS: PHASE6 PASS") == 1
                    assert text.count("AGENTICOS: PHASE7 PASS") == 1
                    assert text.count("AGENTICOS: FS USER PASS") == 1
                    assert text.count("AGENTICOS: PHASE8 PASS") == 1
                    assert text.count("AGENTICOS: PHASE9 PASS") == 1
                    assert text.count("AGENTICOS: SHELL USER PASS") == 1
                    assert text.count("AGENTICOS: PHASE10 PASS") == 1
                    assert text.count("AGENTICOS: PHASE11 PASS") == 1
                    assert text.count("AGENTICOS: DESKTOP USER PASS") == 1
                    assert text.count("AGENTICOS: PHASE12 PASS") == 1
                    assert text.count("AGENTICOS: SECURITY POLICY PASS") == 1
                    assert text.count("AGENTICOS: PHASE13 PASS") == 1
                    assert text.count("AGENTICOS: APPLICATION USER PASS") == 1
                    assert text.count("AGENTICOS: PHASE14 PASS") == 1
                    assert text.count("AGENTICOS: READY") == 1
                    assert text.count("AGENTICOS: AGENT1 PASS") == 1
                    if args.minimum_level in ("DEBUG", "INFO"):
                        assert "native application checks passed" in text
                    if args.minimum_level in ("DEBUG", "INFO"):
                        assert re.findall(r"\[security-test\] case=(\d) status=(-?\d+)", text) == [(str(i), "-122" if i in (3, 4) else "0") for i in range(6)]
                    assert text.count("AGENTICOS: IRET PASS") == 1
                    assert text.count("AGENTICOS: TIMER PASS") == 1
                qmp.execute("screendump", {"filename": str(output / "screen.ppm")})
                screen = validate_screen(output / "screen.ppm", output / "screen.png")
                qmp.execute("quit")
                assert process.wait(timeout=5) == 0, "QEMU did not exit cleanly"
                print(f"PASS: {args.firmware.upper()} {args.scenario}/{args.minimum_level}, validated diagnostics, IRQs off, kernel HLT, screen {screen}")
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
    parser.add_argument("--firmware", choices=("bios", "uefi"), required=True)
    parser.add_argument("--iso", type=Path)
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--scenario", choices=("normal", "logs", "panic", "assert"), default="normal")
    parser.add_argument("--minimum-level", choices=("DEBUG", "INFO", "WARNING", "ERROR", "PANIC"), default="INFO")
    parser.add_argument("--evidence-label", default="", help="Optional unique suffix for this run's evidence")
    args = parser.parse_args()
    if args.evidence_label and not re.fullmatch(r"[a-z0-9-]+", args.evidence_label):
        parser.error("--evidence-label must contain only lowercase letters, digits or hyphens")
    try:
        run(args)
    except (AssertionError, OSError, RuntimeError, TimeoutError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {args.firmware.upper()} {args.scenario}: {error}; see build/tests", file=sys.stderr)
        return 1
    return 0


def validate_diagnostics(text, scenario, minimum):
    levels = ("DEBUG", "INFO", "WARNING", "ERROR", "PANIC")
    entries = re.findall(r"^\[(\d+)\] \[(DEBUG|INFO|WARNING|ERROR|PANIC)\] \[([^\]]+)\] (.*)$", text, re.MULTILINE)
    assert entries or minimum == "PANIC", "No structured kernel diagnostics"
    assert [int(entry[0]) for entry in entries] == list(range(1, len(entries) + 1)), "Discontinuous/restarted log sequence"
    assert all(levels.index(entry[1]) >= levels.index(minimum) for entry in entries), "Log filter failed"
    if levels.index(minimum) <= levels.index("INFO"):
        assert text.count("Welcome to AgenticOS") == 1
        assert re.search(r"\[INFO\] \[cpu\] vendor=\S+ family=\d+ model=\d+ stepping=\d+", text)
        assert "[INFO] [cpu] brand=" in text
    if minimum == "DEBUG":
        assert "[DEBUG] [serial] COM1 115200 8N1; transmit/receive loopback verified" in text
        assert "[DEBUG] [cpu] long_mode=1" in text
    if scenario in ("panic", "assert"):
        assert text.count("[PANIC]") == 1 and "AGENTICOS: PHASE2 PASS" not in text
        reason = "intentional panic probe" if scenario == "panic" else "assertion failed: 2 + 2 == 5"
        assert re.search(r"\[PANIC\] \[panic\] kernel/kernel.c:\d+ kernel_main: " + re.escape(reason), text)
        assert "[ERROR]" not in text
    else:
        assert text.count("AGENTICOS: PHASE2 PASS") == 1 and "[PANIC]" not in text
        if scenario == "logs":
            for level in levels[:-1]:
                expected = f"[{level}] [selftest] diagnostic {level} record"
                assert (expected in text) == (levels.index(level) >= levels.index(minimum))
            assert all(entry[2] == "selftest" for entry in entries if entry[1] == "ERROR")
        else:
            assert "[ERROR]" not in text


if __name__ == "__main__":
    raise SystemExit(main())
