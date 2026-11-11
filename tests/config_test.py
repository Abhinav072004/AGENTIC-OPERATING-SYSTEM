#!/usr/bin/env python3
"""Check validation and change-sensitive generation of the build configuration."""
from pathlib import Path
import subprocess
import sys
import tempfile

script = Path(__file__).resolve().parents[1] / "tools/configure.py"
with tempfile.TemporaryDirectory(prefix="agenticos-config-") as directory:
    header = Path(directory) / "config.h"
    def generate(level="INFO", mode="none"):
        return subprocess.run([sys.executable, str(script), "--output", str(header),
                               "--log-level", level, "--selftest", mode],
                              capture_output=True, text=True)
    assert generate().returncode == 0
    original, timestamp = header.read_text(), header.stat().st_mtime_ns
    assert generate().returncode == 0 and header.stat().st_mtime_ns == timestamp
    assert generate("DEBUG", "assert").returncode == 0
    assert "CONFIG_LOG_LEVEL LOG_DEBUG" in header.read_text()
    assert "CONFIG_SELFTEST 3" in header.read_text()
    changed = header.read_text()
    assert generate("TRACE").returncode != 0 and header.read_text() == changed
    assert generate(mode="unknown").returncode != 0 and header.read_text() == changed
    assert generate().returncode == 0 and header.read_text() == original
print("PASS: configuration validation, unchanged mtime, mode changes and restoration")
