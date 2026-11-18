#!/usr/bin/env python3
"""Exercise real PS/2, PCI, ATA PIO and e1000 rings on private QEMU devices."""
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time
from boot_test import QMP, validate_diagnostics, validate_screen
from qemu import REPO, command


def run(firmware):
    output = REPO / 'build/tests' / ('drivers-' + firmware)
    output.mkdir(parents=True, exist_ok=True)
    serial = output / 'serial.log'
    serial.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='agenticos-drivers-') as directory, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as endpoint:
        work = Path(directory)
        disk = work / 'scratch.raw'
        original = bytearray(1024 * 1024)
        original[:19] = b'AgenticOS TEST DISK'
        disk.write_bytes(original)
        endpoint.bind(('127.0.0.1', 0))
        endpoint.setblocking(False)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reserve:
            reserve.bind(('127.0.0.1', 0))
            guest_port = reserve.getsockname()[1]
        qmp_path = work / 'qmp.sock'
        iso = REPO / 'build/driver-scenario/agenticos.iso'
        cmd = command(firmware, directory, headless=True, iso=iso)
        cmd += ['-device', 'isa-ide,id=testide,iobase=0x1f0,iobase2=0x3f6,irq=14',
                '-drive', f'if=none,format=raw,id=testdisk,file={disk}', '-device', 'ide-hd,drive=testdisk,bus=testide.0',
                '-netdev', f'socket,id=n0,udp=127.0.0.1:{endpoint.getsockname()[1]},localaddr=127.0.0.1:{guest_port}',
                '-device', 'e1000,netdev=n0,mac=52:54:00:12:34:56',
                '-serial', f'file:{serial}', '-qmp', f'unix:{qmp_path},server=on,wait=off']
        (output / 'command.json').write_text(json.dumps({'command': cmd, 'iso_sha256': hashlib.sha256(iso.read_bytes()).hexdigest()}, indent=2) + '\n')
        qmp = None
        with (output / 'qemu.log').open('wb') as log:
            process = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 40
                injected = False
                sequences = []
                while time.monotonic() < deadline:
                    assert process.poll() is None, (output / 'qemu.log').read_text()
                    text = serial.read_text(errors='replace') if serial.exists() else ''
                    assert '[PANIC]' not in text and '[ERROR]' not in text, text[-5000:]
                    if qmp is None and qmp_path.exists():
                        qmp = QMP(qmp_path)
                    if 'AGENTICOS: DRIVER INPUT WAIT' in text and not injected:
                        qmp.execute('input-send-event', {'events': [
                            {'type': 'key', 'data': {'down': True, 'key': {'type': 'qcode', 'data': 'a'}}},
                            {'type': 'key', 'data': {'down': False, 'key': {'type': 'qcode', 'data': 'a'}}},
                            {'type': 'btn', 'data': {'down': True, 'button': 'left'}},
                            {'type': 'rel', 'data': {'axis': 'x', 'value': 12}},
                            {'type': 'rel', 'data': {'axis': 'y', 'value': 7}}]})
                        injected = True
                    while True:
                        try:
                            packet, address = endpoint.recvfrom(4096)
                        except BlockingIOError:
                            break
                        assert len(packet) >= 34 and packet[12:14] == b'\x88\xb5' and packet[15:34] == b'AGENTICOS_DRIVER_TX', packet.hex()
                        assert packet[6:12] == bytes.fromhex('525400123456')
                        assert packet[14] == len(sequences)
                        sequences.append(packet[14])
                        reply = packet[6:12] + bytes.fromhex('020000000001') + packet[12:15] + b'AGENTICOS_DRIVER_RX'
                        endpoint.sendto(reply.ljust(64, b'\0'), address)
                    if 'AGENTICOS: PHASE13 PASS' in text:
                        break
                    time.sleep(0.01)
                else:
                    raise TimeoutError(text[-5000:])
                assert sequences == list(range(24)) and injected
                for marker in ('STORAGE', 'INPUT', 'NETWORK'):
                    assert text.count(f'AGENTICOS: DRIVER {marker} PASS') == 1
                validate_diagnostics(text, 'normal', 'INFO')
                registers = qmp.execute('human-monitor-command', {'command-line': 'info registers'})
                assert 'HLT=1' in registers and 'CPL=0' in registers
                (output / 'registers.txt').write_text(registers)
                qmp.execute('screendump', {'filename': str(output / 'screen.ppm')})
                validate_screen(output / 'screen.ppm', output / 'screen.png')
                qmp.execute('quit')
                assert process.wait(timeout=5) == 0
                after = disk.read_bytes()
                assert after[:512] == original[:512] and after[1024:] == original[1024:]
                assert after[512:1024] == bytes((i ^ 0x5a) & 255 for i in range(512))
                (output / 'results.json').write_text(json.dumps({'ethernet_sequences': sequences, 'input': 'QMP a + relative mouse + left button', 'scratch_disk_sector1_sha256': hashlib.sha256(after[512:1024]).hexdigest()}, indent=2) + '\n')
                print(f'PASS: {firmware.upper()} PCI discovery, real PS/2 events, ATA identify/read/write/bounds and 24 bidirectional Ethernet frames (ring wrap)')
            finally:
                if qmp is not None:
                    qmp.close()
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)


if __name__ == '__main__':
    subprocess.run(['make', '-j2', 'BUILD=build/driver-scenario', 'BOOT_TEST=drivers', 'iso', 'check-elf'], cwd=REPO, check=True)
    for firmware in ('bios', 'uefi'):
        run(firmware)
