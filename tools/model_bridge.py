#!/usr/bin/env python3
"""Optional local AgenticOS bridge to an OpenAI-compatible chat completion API.

The API key stays in AGENTICOS_MODEL_KEY on this host. The guest sends only
bounded ASCII intent over QEMU's host gateway. Plans still require OS approval.
"""
import argparse
import json
import os
import socketserver
import struct
import threading
import urllib.error
import urllib.parse
import urllib.request
TOOLS = {name: i+1 for i, name in enumerate(("mkdir", "write", "read", "list", "move", "delete", "system"))}
PLAN_BYTES = 8 + 8 * 392
PROMPT = '''Return only a JSON object {"actions":[...]}, at most eight actions.
Each action has "tool" (mkdir,write,read,list,move,delete,system), "path" except
system; move also has "to"; write also has "text". No extra fields. Paths must
be canonical ASCII under /home, no . or .. components; mutations cannot target
/home itself. Text is literal ASCII with optional LF/tab, at most 127 bytes.
Never return code, shell commands, credentials, approval or permission grants.
If the intent is ambiguous or unsupported, return {"actions":[]}.
'''

def unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate key")
        result[key] = value
    return result

def field(value, *, path=False):
    if not isinstance(value, str) or len(value.encode("utf-8")) > 127 or any(ord(c)>126 or (ord(c)<32 and (path or c not in "\n\t")) for c in value):
        raise ValueError("invalid bounded ASCII field")
    if path:
        pieces = value.split("/")
        if not value.startswith("/home") or (value!="/home" and not value.startswith("/home/")) or any(p in ("", ".", "..") or len(p)>31 for p in pieces[1:]) or any(ord(c)<33 for c in value):
            raise ValueError("invalid workspace path")
    return value.encode("ascii").ljust(128, b"\0")

def encode_plan(value):
    if not isinstance(value, dict) or set(value)!={"actions"} or not isinstance(value["actions"], list) or not 1<=len(value["actions"])<=8:
        raise ValueError("invalid plan")
    result = struct.pack("<II",1,len(value["actions"]))
    for action in value["actions"]:
        if not isinstance(action, dict) or action.get("tool") not in TOOLS:
            raise ValueError("unknown tool")
        tool = action["tool"]
        allowed = {"tool"} | ({"path"} if tool!="system" else set()) | ({"to"} if tool=="move" else set()) | ({"text"} if tool=="write" else set())
        if set(action)!=allowed:
            raise ValueError("invalid action fields")
        path = action.get("path", "")
        if tool not in ("read", "list", "system") and path=="/home":
            raise ValueError("workspace root mutation")
        if tool=="move" and action["to"]=="/home":
            raise ValueError("workspace root destination")
        result += struct.pack("<II",TOOLS[tool],0) + (field(path,path=True) if tool!="system" else bytes(128)) + (field(action["to"],path=True) if tool=="move" else bytes(128)) + (field(action["text"]) if tool=="write" else bytes(128))
    return result.ljust(PLAN_BYTES,b"\0")

def completion(intent, url, model, key="", timeout=15):
    body = json.dumps({"model": model, "messages": [{"role":"system","content":PROMPT},{"role":"user","content":intent}], "temperature":0, "max_tokens":1200, "response_format":{"type":"json_object"}}).encode()
    headers = {"Content-Type":"application/json"}
    if key:
        headers["Authorization"] = "Bearer " + key
    request = urllib.request.Request(url, data=body, headers=headers)
    # Redirects could send credentials to another host; reject them.
    class NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, req, fp, code, msg, headers, newurl):
            return None
    with urllib.request.build_opener(NoRedirect).open(request, timeout=timeout) as response:
        raw = response.read(32769)
    if len(raw)>32768:
        raise ValueError("provider response too large")
    value = json.loads(raw,object_pairs_hook=unique)
    content = value["choices"][0]["message"]["content"]
    if not isinstance(content,str):
        raise ValueError("invalid provider message")
    return encode_plan(json.loads(content,object_pairs_hook=unique))

def receive(stream, size):
    data = bytearray()
    while len(data)<size:
        part = stream.recv(size-len(data))
        if not part:
            raise ValueError("incomplete request")
        data.extend(part)
    return bytes(data)

def request_intent(stream):
    header = receive(stream,8)
    magic, length = struct.unpack("<4sI",header)
    if magic!=b"AGI1" or not 1<=length<512:
        raise ValueError("invalid request header")
    intent = receive(stream,length).decode("ascii")
    if any(not 32<=ord(c)<=126 for c in intent):
        raise ValueError("invalid intent")
    return intent

def reply(stream, status, plan=b""):
    stream.sendall(struct.pack("<4siI",b"AGP1",status,len(plan))+plan)

class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        if not self.server.slots.acquire(blocking=False):
            reply(self.request,-11)
            return
        try:
            self.request.settimeout(20)
            intent = request_intent(self.request)
            plan = completion(intent,self.server.url,self.server.model,self.server.key)
            reply(self.request,0,plan)
        except (ValueError,KeyError,IndexError,TypeError,UnicodeError):
            reply(self.request,-22)
        except (OSError,urllib.error.URLError):
            try: reply(self.request,-5)
            except OSError: pass
        finally:
            self.server.slots.release()

class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True
    request_queue_size = 4
    def __init__(self,address,url,model,key="",handler=Handler):
        super().__init__(address,handler)
        self.url,self.model,self.key = url,model,key
        self.slots = threading.BoundedSemaphore(4)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url",required=True,help="Full HTTPS /v1/chat/completions endpoint")
    parser.add_argument("--model",required=True)
    parser.add_argument("--port",type=int,default=8088)
    parser.add_argument("--allow-local-http",action="store_true",help="Allow HTTP only to localhost for a local model server")
    args = parser.parse_args()
    url = urllib.parse.urlsplit(args.url)
    local_http = args.allow_local_http and url.scheme=="http" and url.hostname in ("localhost","127.0.0.1","::1")
    if url.scheme!="https" and not local_http or url.username or url.password or url.query or url.fragment:
        parser.error("Use an HTTPS endpoint, or explicit localhost HTTP; no embedded credentials/query")
    if not 1<=args.port<=65535:
        parser.error("invalid port")
    with Server(("127.0.0.1",args.port),args.url,args.model,os.environ.get("AGENTICOS_MODEL_KEY","")) as server:
        print(f"AgenticOS model bridge listening on 127.0.0.1:{args.port}; plans require guest approval",flush=True)
        try: server.serve_forever()
        except KeyboardInterrupt: pass
if __name__=="__main__":
    main()
