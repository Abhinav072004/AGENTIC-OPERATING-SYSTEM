#!/usr/bin/env python3
"""Drive the native CPL3 shell through an actual bidirectional serial socket."""
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time
from boot_test import QMP, validate_screen
from qemu import REPO, command


def run(firmware):
    output = REPO / 'build/tests' / ('shell-' + firmware)
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='agenticos-shell-') as directory:
        work = Path(directory)
        uart, qmp_path = work / 'uart.sock', work / 'qmp.sock'
        iso = REPO / 'build/shell-scenario/agenticos.iso'
        cmd = command(firmware, directory, headless=True, iso=iso)
        cmd += ['-chardev', f'socket,id=uart,path={uart},server=on,wait=off', '-serial', 'chardev:uart', '-qmp', f'unix:{qmp_path},server=on,wait=off']
        (output / 'command.json').write_text(json.dumps({'command': cmd, 'iso_sha256': hashlib.sha256(iso.read_bytes()).hexdigest()}, indent=2) + '\n')
        channel, qmp = None, None
        captured = bytearray()
        with (output / 'qemu.log').open('wb') as log:
            process = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 40
                stage = 0
                commands = [
                    ('uname', 'AgenticOS 0.12.0 x86_64'),
                    ('mkdir demo', '/home > '), ('cd demo', '/home/demo > '),
                    ('write "quoted file" "hello from serial"', '/home/demo > '),
                    ('cat "quoted file"', '\nhello from serial\n'),
                    ('mv "quoted file" renamed', '/home/demo > '),
                    ('stat renamed', 'file bytes=17'), ('chmod 000 renamed', '/home/demo > '),
                    ('cat renamed', 'error=-13'), ('chmod 600 renamed', '/home/demo > '),
                    ('cat renamed', '\nhello from serial\n'), ('id', 'uid=1000'), ('ps', 'PID TID STATE TICKS NAME'),
                    ('run /bin/hello', 'exit=0'),
                    ('echo "unfinished', 'error: unterminated quote'),
                    ('cat missing', 'error=-2'),
                    ('x' * 260, 'error: line too long'),
                    ('rm renamed', '/home/demo > '), ('cd ..', '/home > '), ('rm demo', '/home > '),
                    ('exit', 'AGENTICOS: PHASE12 PASS')]
                sent_at = 0
                text = ''
                while time.monotonic() < deadline:
                    assert process.poll() is None, (output / 'qemu.log').read_text()
                    if channel is None and uart.exists():
                        channel = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); channel.connect(str(uart)); channel.setblocking(False)
                    if qmp is None and qmp_path.exists():
                        qmp = QMP(qmp_path)
                    if channel is not None:
                        while True:
                            try:
                                chunk = channel.recv(65536)
                            except BlockingIOError:
                                break
                            if not chunk:
                                break
                            captured.extend(chunk)
                    text = captured.decode(errors='replace').replace('\r\n', '\n')
                    assert '[PANIC]' not in text and '[ERROR]' not in text, text[-4000:]
                    if stage == 0 and '/home > ' in text:
                        channel.sendall((commands[0][0] + '\n').encode()); sent_at = len(text); stage = 1
                    elif stage > 0 and commands[stage - 1][1] in text[sent_at:]:
                        if stage == len(commands):
                            break
                        # Wait for the next prompt before writing another command.
                        if '> ' not in text[sent_at:] and commands[stage - 1][0] != 'exit':
                            time.sleep(0.01); continue
                        channel.sendall((commands[stage][0] + '\n').encode()); sent_at = len(text); stage += 1
                    time.sleep(0.01)
                else:
                    raise TimeoutError(f'stage={stage} command={commands[max(stage-1,0)][0]!r}\n{text[-5000:]}')
                assert stage == len(commands) and 'AGENTICOS: SHELL USER PASS' in text
                assert 'Hello from native AgenticOS user mode' in text
                (output / 'serial.log').write_bytes(captured)
                registers = qmp.execute('human-monitor-command', {'command-line': 'info registers'})
                assert 'CPL=0' in registers and 'HLT=1' in registers
                (output / 'registers.txt').write_text(registers)
                qmp.execute('screendump', {'filename': str(output / 'screen.ppm')})
                validate_screen(output / 'screen.ppm', output / 'screen.png')
                qmp.execute('quit'); assert process.wait(timeout=5) == 0
                print(f'PASS: {firmware.upper()} native serial shell: quotes, file workflow, process inspection, child execution, malformed/oversized input and exit cleanup')
            finally:
                (output / 'serial.log').write_bytes(captured)
                if channel is not None: channel.close()
                if qmp is not None: qmp.close()
                if process.poll() is None: process.terminate(); process.wait(timeout=5)


if __name__ == '__main__':
    subprocess.run(['make', '-j2', 'BUILD=build/shell-scenario', 'BOOT_TEST=shell', 'iso', 'check-elf'], cwd=REPO, check=True)
    for firmware in ('bios', 'uefi'): run(firmware)
