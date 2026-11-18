#!/usr/bin/env python3
"""QEMU launch configuration shared by interactive runs and boot tests."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[1]


def command(firmware, workdir, *, headless=False, debug=False, iso=None):
    root = Path(os.environ.get("AGENTICOS_SYSROOT", "/"))
    share = root / "usr/share"
    args = [
        "qemu-system-x86_64", "-machine", "q35", "-accel", "tcg",
        "-cpu", "qemu64", "-m", "128M", "-smp", "1", "-nic", "none",
        "-cdrom", str(iso or REPO / "build/agenticos.iso"),
        "-boot", "order=d", "-no-reboot", "-no-shutdown", "-monitor", "none",
        "-L", str(share / "qemu"), "-vga", "std",
        "-global", f"VGA.romfile={share / 'seabios/vgabios-stdvga.bin'}",
    ]
    if firmware == "uefi":
        code = share / "OVMF/OVMF_CODE_4M.fd"
        variables = share / "OVMF/OVMF_VARS_4M.fd"
        if not code.is_file() or not variables.is_file():
            raise FileNotFoundError("OVMF 4M firmware missing; see docs/SETUP.md")
        private_vars = Path(workdir) / "OVMF_VARS.fd"
        shutil.copyfile(variables, private_vars)
        args += ["-drive", f"if=pflash,format=raw,unit=0,readonly=on,file={code}",
                 "-drive", f"if=pflash,format=raw,unit=1,file={private_vars}"]
    else:
        args += ["-bios", str(share / "seabios/bios-256k.bin")]
    if headless:
        args += ["-display", "none"]
    if debug:
        args += ["-S", "-gdb", "tcp:127.0.0.1:1234"]
    return args


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", choices=("bios", "uefi"), default="bios")
    parser.add_argument("--headless", action="store_true")
    parser.add_argument("--display", default="none",
                        help="QEMU display backend, e.g. none, gtk, vnc=127.0.0.1:1")
    parser.add_argument("--debug", action="store_true")
    parser.add_argument("--network", choices=("none", "user"), default="none")
    parser.add_argument("--iso", type=Path, help="Boot a selected AgenticOS ISO")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="agenticos-run-") as workdir:
        cmd = command(args.firmware, workdir,
                      headless=args.headless or args.display == "none", debug=args.debug, iso=args.iso)
        if not args.headless and args.display != "none":
            cmd += ["-display", args.display]
        if args.network == "user":
            cmd += ["-netdev", "user,id=agenticos", "-device", "e1000,netdev=agenticos,mac=52:54:00:12:34:56"]
        cmd += ["-serial", "stdio"]
        try:
            return subprocess.call(cmd)
        except KeyboardInterrupt:
            return 130


if __name__ == "__main__":
    raise SystemExit(main())
