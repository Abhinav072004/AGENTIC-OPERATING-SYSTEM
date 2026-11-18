#!/usr/bin/env python3
"""Drive native apps and inspect their real loaded state through QEMU debugging."""
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import time
from boot_test import QMP, validate_screen
from desktop_test import Input, capture
from qemu import REPO, command


def symbols(elf):
    result = subprocess.check_output(['readelf','-sW',str(elf)],text=True)
    return {match[2]:int(match[1],16) for match in re.finditer(r'\d+:\s+([0-9a-f]+)\s+\d+\s+OBJECT\s+(?:GLOBAL|LOCAL)\s+DEFAULT\s+\S+\s+(\S+)',result)}


def physical(qmp,address,length):
    dump = qmp.execute('human-monitor-command',{'command-line':f'xp /{length}bx 0x{address:x}'})
    data = []
    for line in dump.splitlines():
        if ':' in line: data.extend(int(value,16) for value in re.findall(r'0x([0-9a-f]{2})\b',line.split(':',1)[1]))
    assert len(data) == length,dump
    return bytes(data)


def virtual(qmp,root,address,length):
    result = bytearray()
    while len(result)<length:
        current = address+len(result); table = root
        for shift in (39,30,21,12):
            entry = struct.unpack('<Q',physical(qmp,table+((current>>shift)&511)*8,8))[0]
            assert entry&5 == 5 and not (shift>12 and entry&128),'Expected private user 4K mapping'
            table = entry&0x000ffffffffff000
        amount = min(length-len(result),4096-(current&4095))
        result.extend(physical(qmp,table+(current&4095),amount))
    return bytes(result)


def run(firmware):
    output = REPO/'build/tests'/('applications-'+firmware); output.mkdir(parents=True,exist_ok=True)
    serial = output/'serial.log'; serial.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='agenticos-apps-') as directory:
        qmp_path = Path(directory)/'qmp.sock'; build = REPO/'build/desktop-scenario'
        cmd = command(firmware,directory,headless=True,iso=build/'agenticos.iso')+['-serial',f'file:{serial}','-qmp',f'unix:{qmp_path},server=on,wait=off']
        (output/'command.json').write_text(json.dumps(cmd,indent=2)+'\n')
        qmp = None
        with (output/'qemu.log').open('wb') as log:
            process = subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT)
            try:
                def wait_for(marker,after=0):
                    deadline=time.monotonic()+35
                    while time.monotonic()<deadline:
                        text=serial.read_text(errors='replace') if serial.exists() else ''
                        assert process.poll() is None and '[PANIC]' not in text and '[ERROR]' not in text,text[-4000:]
                        if marker in text[after:]: return text
                        time.sleep(0.025)
                    if qmp is not None:
                        capture(qmp,output,'failure')
                        if 'expression' in locations:
                            (output/'failure-expression.bin').write_bytes(snapshot(locations['expression'],64))
                    raise TimeoutError(marker+'\n'+text[-4000:])
                text=wait_for('GUI READY'); qmp=QMP(qmp_path); controls=Input(qmp)
                root=int(re.findall(r'case=/bin/desktop pid=\d+ cr3=0x([0-9a-f]+)',text)[-1],16)
                locations=symbols(build/'user/desktop.elf')
                opened = {1,2,3}
                def launch(app):
                    before=len(serial.read_text()); controls.click(50,580); controls.click(70,356+(app-1)*34+12)
                    wait_for('GUI focus' if app in opened else 'GUI launch',before); opened.add(app)
                def choose_path(path):
                    before=len(serial.read_text()); controls.click(220,185); wait_for('APP editor path',before)
                    controls.type(path+'\n')
                def snapshot(address,length):
                    qmp.execute('stop')
                    try: return virtual(qmp,root,address,length)
                    finally: qmp.execute('cont')
                launch(4); choose_path('/home/week13.txt'); controls.type('Native editor.'); controls.key('left'); controls.type(' works')
                before=len(serial.read_text())
                def key(key,down): return {'type':'key','data':{'down':down,'key':{'type':'qcode','data':key}}}
                controls.events([key('ctrl',True),key('s',True),key('s',False),key('ctrl',False)])
                wait_for('APP editor saved',before)
                controls.type('!'); before=len(serial.read_text()); controls.click(642,150); time.sleep(0.15)
                assert 'text="GUI close"' not in serial.read_text()[before:]
                capture(qmp,output,'editor-unsaved')
                controls.click(440,185); before=len(serial.read_text()); choose_path('/home/week13.txt'); wait_for('APP editor opened',before)
                expected=b'Native editor works.'
                assert snapshot(locations['editor']+128,len(expected)+1)==expected+b'\0'
                capture(qmp,output,'editor-saved')
                launch(3); controls.type('2*(3+4)-5\n'); wait_for('GUI calc=9')
                launch(5); controls.click(280,375); wait_for('APP monitor refreshed')
                free,managed=struct.unpack('<QQ',snapshot(locations['monitor']+8,16)); assert managed>=free>1000
                capture(qmp,output,'monitor')
                launch(6); controls.click(510,234); wait_for('APP theme slate')
                assert snapshot(locations['apps_slate_theme'],1)==b'\1'
                launch(4); before=len(serial.read_text()); choose_path('/home/.settings'); wait_for('APP editor opened',before)
                assert snapshot(locations['editor']+128,2)==b'1\0'
                launch(6); controls.click(350,234); wait_for('APP theme ocean'); assert snapshot(locations['apps_slate_theme'],1)==b'\0'
                capture(qmp,output,'applications')
                controls.key('esc'); wait_for('AGENTICOS: SESSION EXIT')
                registers=qmp.execute('human-monitor-command',{'command-line':'info registers'}); assert 'CPL=0' in registers and 'HLT=1' in registers
                (output/'registers.txt').write_text(registers); qmp.execute('screendump',{'filename':str(output/'screen.ppm')}); validate_screen(output/'screen.ppm',output/'screen.png')
                qmp.execute('quit'); assert process.wait(timeout=5)==0
                (output/'results.json').write_text(json.dumps({'editor_file':expected.decode(),'saved_preference':'1','free_frames':free,'managed_frames':managed,'calculator':9,'unsaved_close_guard':True},indent=2)+'\n')
                print(f'PASS: {firmware.upper()} native editor insertion/Ctrl+S/reopen and unsaved guard, expression calculator, live monitor, saved settings and independent private-memory readback')
            finally:
                if qmp is not None: qmp.close()
                if process.poll() is None: process.terminate(); process.wait(timeout=5)


if __name__=='__main__':
    subprocess.run(['make','-j2','BUILD=build/desktop-scenario','BOOT_TEST=desktop','iso','check-elf'],cwd=REPO,check=True)
    for firmware in ('bios','uefi'): run(firmware)
