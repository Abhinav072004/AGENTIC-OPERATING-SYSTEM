#!/usr/bin/env python3
"""Boot the exact packaged interactive release on BIOS and UEFI."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time
from boot_test import QMP,validate_screen
from desktop_test import Input,capture
from qemu import REPO,command


def run(firmware):
    release=REPO/'build/release'; manifest=json.loads((release/'manifest.json').read_text())
    iso=release/f'agenticos-{manifest["version"]}.iso'
    assert hashlib.sha256(iso.read_bytes()).hexdigest()==manifest['artifacts'][iso.name]['sha256']
    output=REPO/'build/tests'/('release-'+firmware); output.mkdir(parents=True,exist_ok=True)
    serial=output/'serial.log'; serial.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='agenticos-release-') as directory:
        qmp_path=Path(directory)/'qmp.sock'
        cmd=command(firmware,directory,headless=True,iso=iso)+['-serial',f'file:{serial}','-qmp',f'unix:{qmp_path},server=on,wait=off']
        (output/'command.json').write_text(json.dumps({'command':cmd,'iso_sha256':manifest['artifacts'][iso.name]['sha256']},indent=2)+'\n')
        qmp=None
        with (output/'qemu.log').open('wb') as log:
            process=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT)
            try:
                def wait_for(marker):
                    deadline=time.monotonic()+40
                    while time.monotonic()<deadline:
                        text=serial.read_text(errors='replace') if serial.exists() else ''
                        assert process.poll() is None and '[PANIC]' not in text and '[ERROR]' not in text,text[-4000:]
                        if marker in text: return text
                        time.sleep(0.025)
                    raise TimeoutError(text[-4000:])
                text=wait_for('GUI READY'); assert 'AGENTICOS: READY' in text
                for phase in range(1,15): assert text.count(f'AGENTICOS: PHASE{phase} PASS')==1
                qmp=QMP(qmp_path); capture(qmp,output,'release-desktop'); Input(qmp).key('esc'); wait_for('AGENTICOS: SESSION EXIT')
                registers=qmp.execute('human-monitor-command',{'command-line':'info registers'}); assert 'HLT=1' in registers and 'CPL=0' in registers
                (output/'registers.txt').write_text(registers)
                qmp.execute('screendump',{'filename':str(output/'screen.ppm')}); validate_screen(output/'screen.ppm',output/'screen.png')
                qmp.execute('quit'); assert process.wait(timeout=5)==0
                print(f'PASS: packaged {manifest["version"]} {firmware.upper()} ISO: all fourteen stages, live desktop, clean session exit and kernel halt')
            finally:
                if qmp is not None: qmp.close()
                if process.poll() is None: process.terminate(); process.wait(timeout=5)


if __name__=='__main__':
    for firmware in ('bios','uefi'): run(firmware)
