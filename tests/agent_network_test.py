#!/usr/bin/env python3
"""Real guest TCP -> host bridge -> HTTP provider fixture on BIOS and UEFI."""
import http.server
import json
from pathlib import Path
import socketserver
import struct
import subprocess
import sys
import tempfile
import threading
import time
from boot_test import QMP, validate_screen
from qemu import REPO, command
sys.path.insert(0,str(REPO/"tools"))
import model_bridge as bridge
from model_bridge_test import Provider
class Fixture(bridge.Handler):
    def handle(self):
        self.request.settimeout(3)
        try:
            intent=bridge.request_intent(self.request)
            self.server.events.append(intent)
            if intent=="wire magic": self.request.sendall(struct.pack("<4siI",b"BAD!",0,3144)); return
            if intent=="wire length": self.request.sendall(struct.pack("<4siI",b"AGP1",0,100000)); return
            if intent=="wire partial": self.request.sendall(struct.pack("<4siI",b"AGP1",0,3144)+bytes(10)); return
            if intent=="wire timeout": time.sleep(2); return
            plan=bridge.completion(intent,self.server.url,self.server.model,self.server.key,timeout=2)
            bridge.reply(self.request,0,plan)
        except (ValueError,KeyError,IndexError,TypeError): bridge.reply(self.request,-22)
        except OSError:
            try: bridge.reply(self.request,-5)
            except OSError: pass

def run(firmware,server):
    output=REPO/"build/tests"/("agent-model-"+firmware); output.mkdir(parents=True,exist_ok=True)
    serial=output/"serial.log"; serial.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix="agenticos-model-") as temporary:
        qmp_path=Path(temporary)/"qmp.sock"
        cmd=command(firmware,temporary,headless=True,iso=REPO/"build/agent-model/agenticos.iso")
        cmd += ["-netdev","user,id=model","-device","e1000,netdev=model,mac=52:54:00:12:34:56","-serial",f"file:{serial}","-qmp",f"unix:{qmp_path},server=on,wait=off"]
        (output/"command.json").write_text(json.dumps(cmd,indent=2)+"\n")
        with (output/"qemu.log").open("wb") as log:
            process=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT); qmp=None
            try:
                deadline=time.monotonic()+90
                while time.monotonic()<deadline:
                    text=serial.read_text(errors="replace") if serial.exists() else ""
                    if process.poll() is not None: raise RuntimeError((output/"qemu.log").read_text())
                    if "[PANIC]" in text: raise AssertionError(text[-6000:])
                    if "AGENTICOS: READY\n" in text: break
                    time.sleep(.05)
                else: raise TimeoutError(text[-5000:])
                assert "[user-fault] case=/bin/agent " not in text
                assert "AGENTICOS: MODEL BRIDGE PASS" in text and "AGENTICOS: AGENT4 PASS" in text and "[agent-inference] case=1 status=0" in text
                assert server.events[-9:]==["model good","model unsafe","model malformed","model error","wire magic","wire length","wire partial","wire timeout","wire timeout"], server.events
                qmp=QMP(qmp_path); qmp.execute("screendump",{"filename":str(output/"screen.ppm")}); validate_screen(output/"screen.ppm",output/"screen.png"); qmp.execute("quit"); process.wait(timeout=5)
            finally:
                if qmp: qmp.close()
                if process.poll() is None: process.terminate(); process.wait(timeout=5)
    print(f"PASS: {firmware.upper()} real e1000/TCP model bridge, HTTP fixture, unapproved/no-effect review, unsafe/malformed/provider/wire/timeout rejection, in-flight cancel and all 8 sockets reclaimed")

def main():
    with http.server.ThreadingHTTPServer(("127.0.0.1",0),Provider) as provider:
        threading.Thread(target=provider.serve_forever,daemon=True).start()
        url=f"http://127.0.0.1:{provider.server_port}/v1/chat/completions"
        with bridge.Server(("127.0.0.1",0),url,"fixture","fixture-key",Fixture) as server:
            server.events=[]; threading.Thread(target=server.serve_forever,daemon=True).start()
            subprocess.run(["make","-j2","BUILD=build/agent-model","BOOT_TEST=agent-model",f"AGENT_BRIDGE_PORT={server.server_address[1]}","iso"],cwd=REPO,check=True,stdout=(REPO/"build/tests/agent-model-build.log").open("w"),stderr=subprocess.STDOUT)
            for firmware in ("bios","uefi"): run(firmware,server)
            assert Provider.calls==["model good","model unsafe","model malformed","model error"]*2
            server.shutdown()
        provider.shutdown()
if __name__=="__main__": main()
