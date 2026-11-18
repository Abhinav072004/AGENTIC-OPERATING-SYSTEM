#!/usr/bin/env python3
"""Exercise agent planning/review/approval through the actual native serial shell."""
import json
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time
from boot_test import QMP, validate_screen
from qemu import REPO, command

def run(firmware):
    output=REPO/'build/tests'/('agent-shell-'+firmware); output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='agenticos-agent-shell-') as work:
        uart=Path(work)/'uart.sock'; qmp_path=Path(work)/'qmp.sock'; cmd=command(firmware,work,headless=True,iso=REPO/'build/shell-scenario/agenticos.iso')+['-chardev',f'socket,id=uart,path={uart},server=on,wait=off','-serial','chardev:uart','-qmp',f'unix:{qmp_path},server=on,wait=off']
        (output/'command.json').write_text(json.dumps(cmd,indent=2)+'\n'); captured=bytearray(); channel=qmp=None
        with (output/'qemu.log').open('wb') as log:
            process=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT)
            try:
                def wait_for(marker,after=0):
                    nonlocal channel
                    deadline=time.monotonic()+45
                    while time.monotonic()<deadline:
                        assert process.poll() is None
                        if channel is None and uart.exists(): channel=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM); channel.connect(str(uart)); channel.setblocking(False)
                        if channel:
                            while True:
                                try: part=channel.recv(65536)
                                except BlockingIOError: break
                                if not part: break
                                captured.extend(part)
                        text=captured.decode(errors='replace').replace('\r\n','\n'); assert '[PANIC]' not in text and '[ERROR]' not in text,text[-5000:]
                        if marker in text[after:]: return text
                        time.sleep(.01)
                    raise TimeoutError(marker+'\n'+text[-5000:])
                wait_for('/home > '); qmp=QMP(qmp_path)
                def command_line(line,expected=' > '):
                    start=len(captured.decode(errors='replace').replace('\r\n','\n')); channel.sendall((line+'\n').encode()); text=wait_for(expected,start); text=wait_for(' > ' if line!='exit' else 'AGENTICOS: SESSION EXIT',start); return text[start:]
                answer=command_line('agent plan "mkdir /home/shell-agent; write /home/shell-agent/note \'shell workflow\'; read /home/shell-agent/note"'); id=int(re.search(r'job=(\d+) Review',answer)[1]); assert '3. read' in answer and 'text="shell workflow"' in answer
                assert 'error=-2' in command_line('stat /home/shell-agent')
                assert f'job={id} Review' in command_line('agent jobs')
                assert '1. mkdir' in command_line(f'agent review {id}')
                assert 'Done completed=3/3' in command_line(f'agent approve {id}')
                assert 'shell workflow' in command_line('cat /home/shell-agent/note')
                assert 'event=4' in command_line(f'agent audit {id}')
                command_line(f'agent release {id}'); assert 'error=-3' in command_line(f'agent status {id}')
                answer=command_line('agent plan "delete /home/shell-agent/note; delete /home/shell-agent"'); id=int(re.search(r'job=(\d+) Review',answer)[1]); assert 'error=-1' in command_line(f'agent approve {id}'); assert 'Review completed=0/2' in command_line(f'agent status {id}'); assert 'Done completed=2/2' in command_line(f'agent approve {id} delete'); command_line(f'agent release {id}')
                answer=command_line('agent plan "write /home/shell-cancel \'never\'"'); id=int(re.search(r'job=(\d+) Review',answer)[1]); assert 'Cancelled completed=0/1' in command_line(f'agent cancel {id}'); command_line(f'agent release {id}'); assert 'error=-2' in command_line('stat /home/shell-cancel')
                answer=command_line('agent model "create /home/no-network"'); assert 'error=-100' in answer and 'Model bridge unavailable' in answer
                id=int(re.search(r'job=(\d+) Failed',answer)[1]); command_line(f'agent release {id}'); assert 'No active agent jobs' in command_line('agent jobs')
                command_line('exit','AGENTICOS: SESSION EXIT'); registers=qmp.execute('human-monitor-command',{'command-line':'info registers'}); assert 'HLT=1' in registers and 'CPL=0' in registers
                qmp.execute('screendump',{'filename':str(output/'screen.ppm')}); validate_screen(output/'screen.ppm',output/'screen.png'); qmp.execute('quit'); assert process.wait(timeout=5)==0
                print(f'PASS: {firmware.upper()} native shell planning, immutable action review, approved multi-step VFS workflow, deletion consent, audit, cancellation, stale IDs and unavailable model handling')
            finally:
                (output/'serial.log').write_bytes(captured)
                if channel: channel.close()
                if qmp: qmp.close()
                if process.poll() is None: process.terminate(); process.wait(timeout=5)
if __name__=='__main__':
    subprocess.run(['make','-j2','BUILD=build/shell-scenario','BOOT_TEST=shell','iso','check-elf'],cwd=REPO,check=True)
    for firmware in ('bios','uefi'): run(firmware)
