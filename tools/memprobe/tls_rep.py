#!/usr/bin/env python3
"""连发多次 DeepSeek（TLS）请求，看堆碎片是否累积、最大连续块是否回弹。"""
import json, re, threading, time, urllib.request, uuid
import serial

PORT="/dev/ttyUSB0"; BASE="http://192.168.1.50"
lines=[]; lock=threading.Lock(); stop=False
def rd():
    s=serial.Serial(); s.port=PORT; s.baudrate=115200; s.timeout=0.2; s.dtr=False; s.rts=False; s.open()
    while not stop:
        d=s.read(4096)
        if d:
            with lock: lines.append(d.decode("utf-8","replace"))
    s.close()
def take():
    with lock: out="".join(lines); lines.clear()
    return out
def mp(obj):
    p=json.dumps(obj).encode(); b="----esp32"+uuid.uuid4().hex
    body=(("--"+b+"\r\n").encode()+b'Content-Disposition: form-data; name="payload"; filename="payload.json"\r\n'
      +b"Content-Type: application/json\r\n\r\n"+p+b"\r\n"+("--"+b+"--\r\n").encode())
    return body,"multipart/form-data; boundary="+b
def chat(msg):
    body,ct=mp({"message":msg,"history":[]})
    req=urllib.request.Request(BASE+"/api/chat",data=body,headers={"Content-Type":ct})
    t0=time.time()
    try:
        with urllib.request.urlopen(req,timeout=300) as r: r.read(); return True,time.time()-t0
    except Exception as e: return False,time.time()-t0
def cfg(b):
    body=json.dumps({"backend":b}).encode()
    req=urllib.request.Request(BASE+"/api/config",data=body,headers={"Content-Type":"application/json"})
    with urllib.request.urlopen(req,timeout=20) as r: return r.read()
R=re.compile(r"\[mem\] (\S+)\s+free=(\d+)\s+used=(\d+)\s+peak=(\d+)\s+blk=(\d+)\s+stackleft=(\d+)")
def show(tag,text):
    rows=[(m.group(1),int(m.group(2)),int(m.group(3)),int(m.group(4)),int(m.group(5)),int(m.group(6))) for m in R.finditer(text)]
    print("\n--- %s ---"%tag)
    for r in rows: print("  %-15s free=%-6d used=%-5d peak=%-5d blk=%-6d stk=%d"%r)
t=threading.Thread(target=rd,daemon=True); t.start(); time.sleep(1.5); take()
cfg(1); time.sleep(1.5); take()
for i in range(4):
    ok,dur=chat("hi")
    time.sleep(1.0)
    show("DeepSeek 第%d次 ok=%s %.1fs"%(i+1,ok,dur),take())
cfg(0); time.sleep(1.2); take()
time.sleep(0.5)
ok,dur=chat("hi"); time.sleep(1.0); show("切回 Ollama ok=%s %.1fs"%(ok,dur),take())
stop=True
