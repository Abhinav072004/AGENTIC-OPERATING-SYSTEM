#!/usr/bin/env python3
"""Native graphical agent workflows using real PS/2 input and private user state."""
import ctypes as C
import json
import re
import subprocess
import tempfile
import time
from pathlib import Path
from boot_test import QMP, validate_screen
from desktop_test import Input, capture, input_progress
from applications_test import symbols, virtual
from qemu import REPO, command
class Action(C.Structure): _fields_=[('tool',C.c_uint32),('flags',C.c_uint32),('path',C.c_char*128),('other',C.c_char*128),('text',C.c_char*128)]
class Plan(C.Structure): _fields_=[('abi',C.c_uint32),('count',C.c_uint32),('actions',Action*8)]
class Info(C.Structure): _fields_=[('id',C.c_uint64),('owner',C.c_uint64),('uid',C.c_uint32),('state',C.c_uint32),('completed',C.c_uint32),('total',C.c_uint32),('error',C.c_int32),('bytes',C.c_uint32),('destructive',C.c_uint32),('result',C.c_char*128)]
class Audit(C.Structure): _fields_=[('sequence',C.c_uint64),('job',C.c_uint64),('owner',C.c_uint64),('uid',C.c_uint32),('event',C.c_uint32),('tool',C.c_uint32),('status',C.c_int32),('path',C.c_char*128),('other',C.c_char*128)]
class Panel(C.Structure): _fields_=[('intent',C.c_char*512),('length',C.c_uint64),('id',C.c_uint64),('backend',C.c_uint32),('action',C.c_uint32),('seen',C.c_uint32),('plan',Plan),('info',Info),('audit',Audit),('has_plan',C.c_bool),('deletion',C.c_bool),('audit_mode',C.c_bool),('message',C.c_char*96)]
assert C.sizeof(Plan)==3144 and C.sizeof(Panel)==4264

def run(firmware):
    output=REPO/'build/tests'/('agent-ui-'+firmware); output.mkdir(parents=True,exist_ok=True)
    serial=output/'serial.log'; serial.unlink(missing_ok=True); build=REPO/'build/desktop-scenario'
    with tempfile.TemporaryDirectory(prefix='agenticos-agent-ui-') as work:
        qmp_path=Path(work)/'qmp.sock'; cmd=command(firmware,work,headless=True,iso=build/'agenticos.iso')+['-serial',f'file:{serial}','-qmp',f'unix:{qmp_path},server=on,wait=off']
        (output/'command.json').write_text(json.dumps(cmd,indent=2)+'\n')
        with (output/'qemu.log').open('wb') as log:
            process=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT); qmp=None
            try:
                def wait_for(marker,after=0):
                    deadline=time.monotonic()+45
                    while time.monotonic()<deadline:
                        text=serial.read_text(errors='replace') if serial.exists() else ''
                        assert process.poll() is None and '[PANIC]' not in text and '[ERROR]' not in text,text[-4000:]
                        if marker in text[after:]: return text
                        time.sleep(.02)
                    raise TimeoutError(marker+'\n'+text[-4000:])
                text=wait_for('GUI READY'); qmp=QMP(qmp_path); controls=Input(qmp,input_progress(serial)); live_start=len(text)
                root=int(re.findall(r'case=/bin/desktop pid=\d+ cr3=0x([0-9a-f]+)',text)[-1],16); locations=symbols(build/'user/desktop.elf')
                def snapshot(address,length):
                    qmp.execute('stop')
                    try: return virtual(qmp,root,address,length)
                    finally: qmp.execute('cont')
                def panel(): return Panel.from_buffer_copy(snapshot(locations['agent_panel'],C.sizeof(Panel)))
                def wait_state(state):
                    deadline=time.monotonic()+12
                    while time.monotonic()<deadline:
                        info=Info.from_buffer_copy(snapshot(locations['agent_panel']+Panel.info.offset,C.sizeof(Info)))
                        if info.state==state: return info
                        time.sleep(.03)
                    raise AssertionError((state,info.state,info.error))
                def clear():
                    def key(code,down): return {'type':'key','data':{'down':down,'key':{'type':'qcode','data':code}}}
                    controls.events([key('ctrl',True),key('u',True),key('u',False),key('ctrl',False)])
                def submit(intent,failed=False):
                    clear(); controls.type(intent); before=len(serial.read_text()); controls.key('ret'); wait_for('AGENT GUI failed' if failed else 'AGENT GUI review',before); wait_state(6 if failed else 1)
                    if not failed:
                        capture(qmp,output,'current-review'); assert panel().has_plan
                controls.click(190,20)
                payload='reviewed payload '+'x'*106+' END'; assert len(payload)==127
                submit("mkdir /home/gui-agent then write /home/gui-agent/note '"+payload+"' then read /home/gui-agent/note")
                data=panel(); assert data.plan.count==3 and data.seen==1 and data.info.completed==0
                before=len(serial.read_text()); controls.click(160,481); assert panel().info.state==1 and 'text="AGENT GUI approved"' not in serial.read_text()[before:]
                controls.click(320,211); data=panel(); assert data.action==1 and bytes(data.plan.actions[1].text)==payload.encode() and data.seen==3
                pixels=capture(qmp,output,'agent-review')
                # Text past the first 63 characters must survive the GUI_TEXT ABI's
                # per-call limit; this area was blank when a label was truncated.
                assert any(pixels[(y*800+x)*3:(y*800+x)*3+3]==bytes.fromhex('dce8f5') for y in range(385,393) for x in range(556,674))
                controls.click(320,211); assert panel().seen==7; before=len(serial.read_text()); controls.click(160,481); wait_for('AGENT GUI done',before); info=wait_state(4); assert info.completed==3 and bytes(info.result)==payload.encode()
                capture(qmp,output,'agent-completed')
                controls.click(500,481); assert panel().audit_mode and panel().audit.sequence>0; controls.click(225,211); assert not panel().audit_mode
                controls.click(90,100); controls.type('cat /home/gui-agent/note\n'); terminal=snapshot(locations['terminal'],18*64).replace(b'\0',b''); assert payload.encode() in terminal
                controls.click(190,20); submit("write /home/agent-never 'cancelled'"); before=len(serial.read_text()); controls.click(280,481); wait_for('AGENT GUI cancelled',before); assert wait_state(5).completed==0
                submit('delete /home/gui-agent/note then delete /home/gui-agent'); controls.click(320,211); assert panel().seen==3 and panel().info.destructive==1
                before=len(serial.read_text()); controls.click(160,481); assert panel().info.state==1 and not panel().deletion and 'text="AGENT GUI approved"' not in serial.read_text()[before:]
                controls.click(160,449); assert panel().deletion; before=len(serial.read_text()); controls.click(160,481); wait_for('AGENT GUI done',before); assert wait_state(4).completed==2
                submit('unknown xyzzy',failed=True); assert panel().info.error==-22
                controls.click(330,181); assert panel().backend==1; submit('make a directory /home/no-network',failed=True); assert panel().info.error==-100 and panel().info.completed==0
                capture(qmp,output,'agent-model-unavailable')
                controls.click(90,100); controls.type('stat /home/agent-never\n'); terminal=snapshot(locations['terminal'],18*64).replace(b'\0',b''); assert b'error=-2' in terminal
                live=serial.read_text()[live_start:]; assert '[user-fault] case=/bin/agent ' not in live
                roots=[int(value,16) for value in re.findall(r'case=/bin/agent pid=\d+ cr3=0x([0-9a-f]+)',live)]; assert len(roots)>=5 and all(value!=root for value in roots)
                controls.key('esc'); wait_for('AGENTICOS: SESSION EXIT'); qmp.execute('screendump',{'filename':str(output/'screen.ppm')}); validate_screen(output/'screen.ppm',output/'screen.png'); qmp.execute('quit'); assert process.wait(timeout=5)==0
                print(f'PASS: {firmware.upper()} native Agent panel, full 127-byte literal, every-action/deletion review guards, exact VFS readback, audit, pending cancel, inference failures, private workers and exit reclamation')
            except Exception:
                if qmp:
                    qmp.execute("stop")
                    (output/"failure-registers.txt").write_text(qmp.execute("human-monitor-command",{"command-line":"info registers"}))
                    if "locations" in locals(): (output/"failure-panel.bin").write_bytes(virtual(qmp,root,locations["agent_panel"],C.sizeof(Panel)))
                    capture(qmp,output,"failure")
                raise
            finally:
                if qmp: qmp.close()
                if process.poll() is None: process.terminate(); process.wait(timeout=5)
if __name__=='__main__':
    subprocess.run(['make','-j2','BUILD=build/desktop-scenario','BOOT_TEST=desktop','iso','check-elf'],cwd=REPO,check=True)
    for firmware in ('bios','uefi'): run(firmware)
