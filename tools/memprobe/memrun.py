#!/usr/bin/env python3
"""ESP32 llm-client 内存探针采集：boot / Ollama 多轮 / gzip 上传 / DeepSeek TLS。

串口与 HTTP 并行；串口原始日志落盘，最后按 [mem] 行分组打印。
"""
import json
import re
import sys
import threading
import time
import urllib.request
import uuid
import gzip as gzmod

import serial

PORT = "/dev/ttyUSB0"
BASE = "http://192.168.1.50"
RAW = "/tmp/memrun-raw.log"

lines = []
stop = False
lock = threading.Lock()


def reader():
    s = serial.Serial()
    s.port = PORT
    s.baudrate = 115200
    s.timeout = 0.2
    s.dtr = False
    s.rts = False
    s.open()
    try:
        while not stop:
            d = s.read(4096)
            if d:
                with lock:
                    lines.append(d.decode("utf-8", "replace"))
    finally:
        s.close()


def take():
    with lock:
        out = "".join(lines)
        lines.clear()
    return out


def hard_reset():
    s = serial.Serial()
    s.port = PORT
    s.baudrate = 115200
    s.dtr = False   # IO0 高 -> 正常启动，不进下载模式
    s.rts = False
    s.open()
    time.sleep(0.05)
    s.rts = True    # EN 拉低 -> 复位
    time.sleep(0.15)
    s.rts = False   # 释放 -> 运行
    s.close()


def multipart(obj, gz=False):
    payload = json.dumps(obj).encode()
    name = "payload.json.gz" if gz else "payload.json"
    if gz:
        payload = gzmod.compress(payload, 9)
    boundary = "----esp32" + uuid.uuid4().hex
    body = b""
    body += ("--" + boundary + "\r\n").encode()
    body += ('Content-Disposition: form-data; name="payload"; filename="' + name + '"\r\n').encode()
    body += b"Content-Type: application/json\r\n\r\n"
    body += payload + b"\r\n"
    body += ("--" + boundary + "--\r\n").encode()
    return body, "multipart/form-data; boundary=" + boundary


def post(path, body, ct, timeout=300):
    req = urllib.request.Request(BASE + path, data=body, headers={"Content-Type": ct})
    t0 = time.time()
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return True, r.read(), time.time() - t0
    except Exception as e:  # noqa: BLE001
        return False, repr(e).encode(), time.time() - t0


def chat(msg, history=None, gz=False, timeout=300):
    obj = {"message": msg, "history": history or []}
    body, ct = multipart(obj, gz=gz)
    return post("/api/chat", body, ct, timeout)


def set_backend(b):
    body = json.dumps({"backend": b}).encode()
    return post("/api/config", body, "application/json", timeout=20)


MEM_RE = re.compile(
    r"\[mem\] (\S+)\s+free=(\d+)\s+used=(\d+)\s+peak=(\d+)\s+blk=(\d+)\s+stackleft=(\d+)"
)
BEGIN_RE = re.compile(r"\[mem\] ===== begin: (\S+) \| heapsize=(\d+) free=(\d+) blk=(\d+)")
SUM_RE = re.compile(r"\[mem\] ===== summary: peak_used=(\d+) \(([\d.]+)% of (\d+)\) lowest_free=(\d+)")


def parse(text):
    begin = BEGIN_RE.search(text)
    summ = SUM_RE.search(text)
    rows = [
        {
            "stage": m.group(1),
            "free": int(m.group(2)),
            "used": int(m.group(3)),
            "peak": int(m.group(4)),
            "blk": int(m.group(5)),
            "stackleft": int(m.group(6)),
        }
        for m in MEM_RE.finditer(text)
    ]
    return {
        "begin": (
            {"tag": begin.group(1), "heapsize": int(begin.group(2)),
             "free": int(begin.group(3)), "blk": int(begin.group(4))}
            if begin else None
        ),
        "summary": (
            {"peak_used": int(summ.group(1)), "pct": float(summ.group(2)),
             "total": int(summ.group(3)), "lowest": int(summ.group(4))}
            if summ else None
        ),
        "rows": rows,
    }


def dump(label, res, ok=None, dur=None):
    print("\n--- %s%s ---" % (label, "" if ok is None else ("  ok=%s %.1fs" % (ok, dur))))
    if res["begin"]:
        b = res["begin"]
        print("  heap total=%d  free=%d  blk=%d" % (b["heapsize"], b["free"], b["blk"]))
    for r in res["rows"]:
        print("  %-15s free=%-6d used=%-5d peak=%-5d blk=%-6d stackleft=%d" % (
            r["stage"], r["free"], r["used"], r["peak"], r["blk"], r["stackleft"]))
    if res["summary"]:
        s = res["summary"]
        print("  PEAK used=%d (%.1f%% of %d) lowest_free=%d" % (
            s["peak_used"], s["pct"], s["total"], s["lowest"]))


def main():
    global stop
    rawf = open(RAW, "w", encoding="utf-8")
    print("== 复位板子，抓启动 ==")
    hard_reset()
    t = threading.Thread(target=reader, daemon=True)
    t.start()
    deadline = time.time() + 15
    boot = ""
    while time.time() < deadline:
        time.sleep(0.5)
        seg = take()
        boot += seg
        if "[web] http://" in boot:
            break
    time.sleep(1.0)
    boot += take()
    rawf.write(boot)
    print(boot.strip()[-800:])

    print("\n== 场景 A：Ollama 连续 3 轮 ==")
    for i in range(3):
        ok, _body, dur = chat("hi")
        time.sleep(1.0)
        seg = take()
        rawf.write(seg)
        dump("Ollama 轮%d" % (i + 1), parse(seg), ok, dur)

    print("\n== 场景 B：Ollama 长回答 ==")
    ok, _body, dur = chat("用三句话介绍一下太阳")
    time.sleep(1.0)
    seg = take()
    rawf.write(seg)
    dump("Ollama 长回答", parse(seg), ok, dur)

    print("\n== 场景 C：前端同款 gzip 上传（带历史 ~2KB）==")
    hist = [{"role": "user", "content": "这是第 %d 轮问题：太阳的质量是多少？" % i} for i in range(4)]
    hist += [{"role": "assistant", "content": "太阳质量约为 1.989e30 kg，约地球的 33 万倍。"}] * 4
    ok, _body, dur = chat("hi", history=hist, gz=True)
    time.sleep(1.0)
    seg = take()
    rawf.write(seg)
    dump("gzip+history", parse(seg), ok, dur)

    print("\n== 场景 D：DeepSeek（TLS）==")
    okc, body, durc = set_backend(1)
    time.sleep(1.5)
    take()
    if okc:
        ok, _body, dur = chat("hi")
        time.sleep(1.0)
        seg = take()
        rawf.write(seg)
        dump("DeepSeek", parse(seg), ok, dur)
    else:
        print("切换后端失败:", body[:200])

    print("\n== GPS 恢复：切回 Ollama ==")
    okc, body, _ = set_backend(0)
    time.sleep(1.0)
    seg = take() or ""
    rawf.write(seg)
    print("切回 backend=0 ok=%s" % okc)

    stop = True
    rawf.close()
    print("\n原始串口日志:", RAW)


if __name__ == "__main__":
    main()
