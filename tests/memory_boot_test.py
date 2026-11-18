#!/usr/bin/env python3
"""Verify guest allocation/mapping, real CR3/WP/NX and expected CPU faults."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

from boot_test import QMP, validate_screen, validate_diagnostics
from qemu import REPO, command


def check_boot_errors(text):
    # Limine's UEFI console interleaves cursor-control escapes with panic text.
    decoded = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", text)
    if "PANIC:" in decoded or "[ERROR]" in decoded or "[PANIC]" in decoded:
        raise RuntimeError(decoded)


def check_memory_logs(text, scenario):
    validate_diagnostics(text, "normal", "INFO")
    assert text.count("AGENTICOS: kernel entered") == 1
    assert text.count("AGENTICOS: PHASE1 PASS") == 1
    assert re.search(r"AgenticOS 0\.\d+\.0", text)
    assert text.count("PMM allocate/free and VMM alias/protect/remap/unmap passed") == 1
    assert text.count("heap alignment, cross-page data, invalid-free and coalescing passed") == 1
    assert "guard pages=2; supervisor WP=1 NX=1" in text
    if scenario in ("readonly", "nx", "guard"):
        assert "AGENTICOS: PHASE3 PASS" not in text
        assert "AGENTICOS: PHASE4 PASS" not in text
        assert text.count("AGENTICOS: PAGEFAULT PASS") == 1
        fault = re.search(r"\[memory-probe\] (\w+) fault: address=0x([0-9a-f]+) error=0x([0-9a-f]+) ip=0x([0-9a-f]+)", text)
        assert fault and fault[1] == scenario
        address = 0xFFFFD00000000000 if scenario == "guard" else 0xFFFFD10000000000
        assert int(fault[2], 16) == address
        assert int(fault[3], 16) == {"readonly": 3, "nx": 0x11, "guard": 0}[scenario]
        if scenario == "nx":
            assert int(fault[4], 16) == address
        else:
            assert int(fault[4], 16) >= 0xFFFFFFFF80000000
    else:
        assert text.count("AGENTICOS: PHASE3 PASS") == 1 and "AGENTICOS: PAGEFAULT PASS" not in text
        assert text.count("AGENTICOS: PHASE4 PASS") == 1
        assert text.count("AGENTICOS: PHASE5 PASS") == 1
        assert text.count("AGENTICOS: PHASE6 PASS") == 1
        assert text.count("AGENTICOS: TIMER PASS") == 1
    if scenario == "oom":
        exhaustion = re.search(r"\[memory-test\] exhausted and recovered (\d+) physical frames", text)
        assert exhaustion and int(exhaustion[1]) > 0 and text.count("AGENTICOS: OOM PASS") == 1
    else:
        assert "AGENTICOS: OOM PASS" not in text


def run(args):
    name = f"memory-{args.firmware}-{args.scenario}-{args.memory}m"
    if args.label:
        name += f"-{args.label}"
    output = REPO / "build/tests" / name
    output.mkdir(parents=True, exist_ok=True)
    for filename in ("serial.log", "qemu.log", "registers.txt", "screen.ppm", "screen.png", "command.json"):
        (output / filename).unlink(missing_ok=True)
    iso = (args.iso or REPO / "build/agenticos.iso").resolve(strict=True)
    serial = output / "serial.log"
    with tempfile.TemporaryDirectory(prefix="agenticos-mm-") as temporary:
        qmp_path = Path(temporary) / "qmp.sock"
        cmd = command(args.firmware, temporary, headless=True, iso=iso)
        assert cmd.count("-m") == 1
        cmd[cmd.index("-m") + 1] = f"{args.memory}M"
        cmd += ["-serial", f"file:{serial}", "-qmp", f"unix:{qmp_path},server=on,wait=off"]
        (output / "command.json").write_text(json.dumps({
            "command": cmd, "scenario": args.scenario, "memory_mib": args.memory,
            "image_sha256": hashlib.sha256(iso.read_bytes()).hexdigest(),
        }, indent=2) + "\n")
        qmp = None
        with (output / "qemu.log").open("wb") as log:
            process = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
            try:
                expected = ("AGENTICOS: PAGEFAULT PASS\n" if args.scenario in ("readonly", "nx", "guard")
                            else "AGENTICOS: PHASE13 PASS\n")
                deadline = time.monotonic() + args.timeout
                while time.monotonic() < deadline:
                    text = serial.read_text(errors="replace") if serial.exists() else ""
                    if process.poll() is not None:
                        raise RuntimeError(f"QEMU exited early ({process.returncode})")
                    check_boot_errors(text)
                    if expected in text:
                        break
                    time.sleep(0.05)
                else:
                    raise TimeoutError(f"Missing expected memory result after {args.timeout}s")
                time.sleep(0.25)
                qmp = QMP(qmp_path)
                registers = qmp.execute("human-monitor-command", {"command-line": "info registers"})
                (output / "registers.txt").write_text(registers)
                assert "HLT=1" in registers and "CPL=0" in registers
                assert re.search(r"RIP=ffffffff800[0-9a-f]+", registers)

                def register(name):
                    match = re.search(r"\b" + name + r"=([0-9a-f]+)", registers)
                    assert match, f"Missing CPU register {name}"
                    return int(match[1], 16)

                assert register("CR0") & (1 << 16), "CR0.WP is clear"
                assert register("EFER") & (1 << 11), "EFER.NXE is clear"
                assert not register("CR4") & (1 << 12), "Unexpected five-level paging"
                assert not register("RFL") & 0x200, "Interrupts unexpectedly enabled"
                text = serial.read_text()
                root = re.search(r"activating kernel CR3=0x([0-9a-f]+)", text)
                assert root and (register("CR3") & 0x000FFFFFFFFFF000) == int(root[1], 16)
                check_memory_logs(text, args.scenario)
                qmp.execute("screendump", {"filename": str(output / "screen.ppm")})
                screen = validate_screen(output / "screen.ppm", output / "screen.png")
                qmp.execute("quit")
                assert process.wait(timeout=5) == 0
                print(f"PASS: {args.firmware.upper()} memory/{args.scenario} {args.memory}MiB; own CR3, WP/NX, {screen}")
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
    parser.add_argument("--scenario", default="normal", choices=("normal", "readonly", "nx", "guard", "oom"))
    parser.add_argument("--memory", type=int, default=128, choices=(32, 64, 128, 256, 512))
    parser.add_argument("--iso", type=Path)
    parser.add_argument("--timeout", type=float, default=45)
    parser.add_argument("--label", default="")
    args = parser.parse_args()
    if args.label and not re.fullmatch(r"[a-z0-9-]+", args.label):
        parser.error("--label must contain lowercase letters, digits or hyphens only")
    try:
        run(args)
    except (AssertionError, OSError, RuntimeError, TimeoutError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: memory/{args.scenario} {args.firmware}: {error}; see build/tests", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
