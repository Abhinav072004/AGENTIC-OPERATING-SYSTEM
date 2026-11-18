#!/usr/bin/env python3
"""Exercise the native desktop through real QEMU PS/2 input and framebuffer."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time
import zlib
from boot_test import QMP, validate_screen
from qemu import REPO, command


def capture(qmp, output, name):
    ppm, png = output / (name + '.ppm'), output / (name + '.png')
    previous = None
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        qmp.execute('screendump', {'filename': str(ppm)})
        with ppm.open('rb') as source:
            assert source.readline() == b'P6\n'
            width, height = map(int, source.readline().split()); assert source.readline() == b'255\n'; pixels = source.read()
        if pixels == previous:
            break
        previous = pixels
        time.sleep(0.1)
    else:
        raise TimeoutError('Desktop framebuffer did not settle after input')
    assert (width, height) == (800, 600) and len(pixels) == width * height * 3
    palette = Counter(pixels[i:i+3] for i in range(0, len(pixels), 3))
    assert all(bytes.fromhex(value) in palette for value in ('101a2b', '285da3', 'ffffff', '18283f'))
    assert len(palette) >= 12 and palette[bytes.fromhex('101a2b')] > 50000
    def chunk(kind, data): return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    rows = b''.join(b'\0' + pixels[y*width*3:(y+1)*width*3] for y in range(height))
    png.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
    return pixels


class Input:
    def __init__(self, qmp): self.qmp, self.x, self.y = qmp, 400, 300
    def events(self, events): self.qmp.execute('input-send-event', {'events': events}); time.sleep(0.025)
    def key(self, code, shift=False):
        def event(key, down): return {'type': 'key', 'data': {'down': down, 'key': {'type': 'qcode', 'data': key}}}
        events = ([event('shift', True)] if shift else []) + [event(code, True), event(code, False)] + ([event('shift', False)] if shift else [])
        self.events(events)
    def type(self, text):
        codes = {' ': 'spc', '/': 'slash', '-': 'minus', '.': 'dot', "'": 'apostrophe', '\n': 'ret'}
        shifted = {'!': '1', '*': '8', '+': 'equal', '"': 'apostrophe', '(': '9', ')': '0'}
        for character in text:
            if character in shifted: self.key(shifted[character], True)
            else: self.key(codes.get(character, character.lower()), character.isupper())
    def move(self, x, y):
        while (self.x, self.y) != (x,y):
            dx,dy = max(-45,min(45,x-self.x)), max(-45,min(45,y-self.y))
            self.events([{'type':'rel','data':{'axis':'x','value':dx}},{'type':'rel','data':{'axis':'y','value':dy}}]); self.x += dx; self.y += dy
    def button(self, down): self.events([{'type':'btn','data':{'button':'left','down':down}}])
    def click(self,x,y): self.move(x,y); self.button(True); self.button(False)


def run(firmware):
    output = REPO / 'build/tests' / ('desktop-' + firmware); output.mkdir(parents=True, exist_ok=True)
    serial = output / 'serial.log'; serial.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='agenticos-desktop-') as directory:
        qmp_path = Path(directory) / 'qmp.sock'; iso = REPO / 'build/desktop-scenario/agenticos.iso'
        cmd = command(firmware,directory,headless=True,iso=iso) + ['-serial',f'file:{serial}','-qmp',f'unix:{qmp_path},server=on,wait=off']
        (output/'command.json').write_text(json.dumps({'command':cmd,'iso_sha256':hashlib.sha256(iso.read_bytes()).hexdigest()},indent=2)+'\n')
        qmp = None
        with (output/'qemu.log').open('wb') as log:
            process = subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT)
            try:
                def wait_for(marker,after=0):
                    deadline = time.monotonic()+35
                    while time.monotonic()<deadline:
                        text = serial.read_text(errors='replace') if serial.exists() else ''
                        assert process.poll() is None and '[PANIC]' not in text and '[ERROR]' not in text, text[-4000:]
                        if marker in text[after:]: return text
                        time.sleep(0.025)
                    raise TimeoutError(marker+'\n'+text[-4000:])
                wait_for('GUI READY'); live_start=len(serial.read_text()); qmp = QMP(qmp_path); controls = Input(qmp)
                initial = capture(qmp,output,'desktop-initial')
                controls.type('6*7\n'); wait_for('GUI calc=42',live_start)
                controls.move(550,250); controls.button(True); controls.move(450,180); controls.button(False); wait_for('GUI drag completed')
                pixels = capture(qmp,output,'desktop-dragged')
                assert pixels[(177*800+600)*3:(177*800+600)*3+3] == bytes.fromhex('285da3')
                assert pixels != initial
                controls.click(350,100); controls.click(350,178); wait_for('GUI file opened'); capture(qmp,output,'desktop-files')
                controls.click(100,100); controls.type("write /home/gui-note 'keyboard file'\n"); wait_for('GUI terminal command')
                controls.type('cat /home/gui-note\n'); controls.type('run /bin/hello\n')
                time.sleep(0.3)
                text = serial.read_text(); assert text.count('text="GUI terminal command"') == 3
                controls.click(50,580); controls.click(70,436)  # focus calculator through launcher
                controls.click(652,180); wait_for('GUI close')
                controls.click(50,580); controls.click(70,436)  # reopen calculator
                time.sleep(0.2)
                assert serial.read_text()[live_start:].count('text="GUI launch"') == 1
                capture(qmp,output,'desktop-final')
                qmp.execute('stop'); registers = qmp.execute('human-monitor-command',{'command-line':'info registers'}); (output/'live-registers.txt').write_text(registers); qmp.execute('cont')
                controls.key('esc'); wait_for('AGENTICOS: SESSION EXIT')
                registers = qmp.execute('human-monitor-command',{'command-line':'info registers'}); assert 'HLT=1' in registers and 'CPL=0' in registers
                (output/'registers.txt').write_text(registers)
                qmp.execute('screendump',{'filename':str(output/'screen.ppm')}); validate_screen(output/'screen.ppm',output/'screen.png')
                qmp.execute('quit'); assert process.wait(timeout=5) == 0
                (output/'results.json').write_text(json.dumps({'calculator':42,'drag_delta':[-100,-70],'file_preview':True,'terminal_commands':3,'closed_and_reopened':True},indent=2)+'\n')
                print(f'PASS: {firmware.upper()} real desktop framebuffer, PS/2 calculator, focus/drag, file preview, terminal file/child workflow, launcher, close/reopen and clean exit')
            finally:
                if qmp is not None: qmp.close()
                if process.poll() is None: process.terminate(); process.wait(timeout=5)


if __name__ == '__main__':
    subprocess.run(['make','-j2','BUILD=build/desktop-scenario','BOOT_TEST=desktop','iso','check-elf'],cwd=REPO,check=True)
    for firmware in ('bios','uefi'): run(firmware)
