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
    assert width >= 356 and height >= 100
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
    output = REPO / "build/tests" / args.firmware
    output.mkdir(parents=True, exist_ok=True)
    serial = output / "serial.log"
    # Never accept a previous run's output as success.
    for filename in ("serial.log", "qemu.log", "screen.ppm", "screen.png", "registers.txt"):
        (output / filename).unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix="astra-test-") as workdir:
        qmp_path = Path(workdir) / "qmp.sock"
        cmd = command(args.firmware, workdir, headless=True, iso=args.iso)
        cmd += ["-serial", f"file:{serial}", "-qmp", f"unix:{qmp_path},server=on,wait=off"]
        qmp = None
        with (output / "qemu.log").open("wb") as log:
            process = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + args.timeout
                while time.monotonic() < deadline:
                    text = serial.read_text(errors="replace") if serial.exists() else ""
                    if process.poll() is not None:
                        raise RuntimeError(f"QEMU exited early ({process.returncode})")
                    if "ERROR" in text:
                        raise RuntimeError(text)
                    if "ASTRA: PHASE1 PASS\n" in text:
                        break
                    time.sleep(0.05)
                else:
                    raise TimeoutError(f"No first-boot success marker within {args.timeout}s")
                # Confirm the machine remains in our final HLT, with no reboot loop.
                time.sleep(0.25)
                qmp = QMP(qmp_path)
                registers = qmp.execute("human-monitor-command", {"command-line": "info registers"})
                (output / "registers.txt").write_text(registers)
                assert re.search(r"RIP=ffffffff800[0-9a-f]+", registers), "CPU left the kernel"
                assert "HLT=1" in registers, "CPU did not reach the final halt"
                text = serial.read_text()
                assert text.count("ASTRA: kernel entered") == 1, "Unexpected kernel restart"
                assert text.count("Welcome to AstraOS") == 1
                assert text.count("ASTRA: PHASE1 PASS") == 1 and "ERROR" not in text
                qmp.execute("screendump", {"filename": str(output / "screen.ppm")})
                screen = validate_screen(output / "screen.ppm", output / "screen.png")
                qmp.execute("quit")
                assert process.wait(timeout=5) == 0, "QEMU did not exit cleanly"
                print(f"PASS: {args.firmware.upper()} boot, welcome, stable kernel HLT, screen {screen}")
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
    args = parser.parse_args()
    try:
        run(args)
    except (AssertionError, OSError, RuntimeError, TimeoutError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {args.firmware.upper()}: {error}; see build/tests/{args.firmware}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
