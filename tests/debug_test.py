#!/usr/bin/env python3
"""Verify the QEMU GDB stub can reach the C entry point with debug symbols."""
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from qemu import REPO, command  # noqa: E402

output = REPO / "build/tests/debug"
output.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="agenticos-debug-") as workdir:
    # Refuse to attach to somebody else's debugger endpoint.
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 1234))
    cmd = command("bios", workdir, headless=True, debug=True)
    cmd += ["-serial", f"file:{output / 'serial.log'}"]
    with (output / "qemu.log").open("wb") as log:
        process = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 5
            while True:
                try:
                    with socket.create_connection(("127.0.0.1", 1234), timeout=0.1):
                        break
                except OSError:
                    if process.poll() is not None or time.monotonic() >= deadline:
                        raise RuntimeError("GDB stub failed to start")
                    time.sleep(0.05)
            result = subprocess.run(
                ["gdb", "--batch", "-x", str(REPO / "scripts/debug.gdb"), "-ex", "detach"],
                cwd=REPO, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30,
                text=True, check=False)
            (output / "gdb.log").write_text(result.stdout)
            assert result.returncode == 0, result.stdout
            assert "Breakpoint 1, kernel_main ()" in result.stdout, result.stdout
            print("PASS: GDB hardware breakpoint reached kernel_main with source symbols")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
