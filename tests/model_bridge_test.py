#!/usr/bin/env python3
"""Exercise provider HTTP handling and strict plan encoding, without credentials."""
import http.server
import json
from pathlib import Path
import socket
import struct
import sys
import threading
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
import model_bridge as bridge
class Provider(http.server.BaseHTTPRequestHandler):
    calls=[]
    def log_message(self,*args): pass
    def do_POST(self):
        assert self.path=="/v1/chat/completions"
        assert self.headers.get("Authorization")=="Bearer fixture-key"
        body=json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        intent=body["messages"][-1]["content"]; self.calls.append(intent)
        if intent=="model error": self.send_error(503); return
        content = {"actions":[{"tool":"write","path":"/home/model-check","text":"model fixture"}]}
        if intent=="model unsafe": content["actions"][0]["path"]="/etc/passwd"
        content="not JSON" if intent=="model malformed" else json.dumps(content)
        raw=json.dumps({"choices":[{"message":{"content":content}}]}).encode()
        self.send_response(200); self.send_header("Content-Length",str(len(raw))); self.end_headers(); self.wfile.write(raw)

def main():
    valid={"actions":[{"tool":"write","path":"/home/a","text":"literal; delete /etc"},{"tool":"move","path":"/home/a","to":"/home/b"},{"tool":"system"}]}
    data=bridge.encode_plan(valid); assert len(data)==3144 and struct.unpack("<II",data[:8])==(1,3)
    for value in ({"actions":[]},{"actions":[{"tool":"run","path":"/home/a"}]},{"actions":[{"tool":"delete","path":"/home"}]},{"actions":[{"tool":"read","path":"/home/../etc"}]},{"actions":[{"tool":"write","path":"/home/a","text":"x"*128}]},{"actions":[{"tool":"system","approved":True}]}):
        try: bridge.encode_plan(value)
        except ValueError: pass
        else: raise AssertionError(value)
    try: json.loads('{"actions":[],"actions":[]}',object_pairs_hook=bridge.unique)
    except ValueError: pass
    else: raise AssertionError("duplicate key accepted")
    with http.server.ThreadingHTTPServer(("127.0.0.1",0),Provider) as provider:
        threading.Thread(target=provider.serve_forever,daemon=True).start()
        url=f"http://127.0.0.1:{provider.server_port}/v1/chat/completions"
        with bridge.Server(("127.0.0.1",0),url,"fixture","fixture-key") as server:
            threading.Thread(target=server.serve_forever,daemon=True).start()
            for intent,expected in (("model good",0),("model unsafe",-22),("model malformed",-22),("model error",-5)):
                with socket.create_connection(server.server_address,timeout=3) as stream:
                    stream.sendall(struct.pack("<4sI",b"AGI1",len(intent))+intent.encode()); header=bridge.receive(stream,12); magic,status,size=struct.unpack("<4siI",header); assert magic==b"AGP1" and status==expected
                    if status==0: assert size==3144 and len(bridge.receive(stream,size))==3144
                    else: assert size==0
            server.shutdown()
        provider.shutdown()
    assert len(Provider.calls)==4
    print("PASS: real HTTP provider fixture, host-only key, strict schema/duplicate/size/scope checks, binary bridge and provider failure propagation")
if __name__=="__main__": main()
