#!/usr/bin/env python3
"""ESP32 聊天项目反向代理（含 Basic 认证 + 流式转发）。

监听 127.0.0.1:8099，反代到 192.168.1.50:80。
只绑定回环地址，对外仅经 frpc 隧道暴露。
"""

import base64
import hmac
import http.client
import os
import socket
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

UP_HOST = os.environ.get("UP_HOST", "192.168.1.50")
UP_PORT = int(os.environ.get("UP_PORT", "80"))
MEM_PORT = int(os.environ.get("MEM_PORT", "81"))  # /api/mem 走独立监控端口
LISTEN_HOST = os.environ.get("LISTEN_HOST", "127.0.0.1")
LISTEN_PORT = int(os.environ.get("LISTEN_PORT", "8099"))
AUTH_USER = os.environ.get("AUTH_USER", "esp32")
AUTH_PASS = os.environ.get("AUTH_PASS", "")

# 逐跳头，不应转发
HOP_BY_HOP = {
    "connection",
    "keep-alive",
    "proxy-authenticate",
    "proxy-authorization",
    "te",
    "trailers",
    "transfer-encoding",
    "upgrade",
}

CHUNK = 8192
UP_TIMEOUT = 600  # 模型推理可能很慢


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "esp32chat-proxy"

    def log_message(self, fmt, *args):
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    # ---------- 认证 ----------
    def _authorized(self):
        if not AUTH_PASS:
            return True
        hdr = self.headers.get("Authorization", "")
        if not hdr.startswith("Basic "):
            return False
        try:
            raw = base64.b64decode(hdr[6:].strip()).decode("utf-8", "replace")
        except Exception:
            return False
        user, _, pwd = raw.partition(":")
        ok_u = hmac.compare_digest(user, AUTH_USER)
        ok_p = hmac.compare_digest(pwd, AUTH_PASS)
        return ok_u and ok_p

    def _require_auth(self):
        if self._authorized():
            return True
        body = b"401 Unauthorized\n"
        self.send_response(401)
        self.send_header("WWW-Authenticate", 'Basic realm="ESP32 Chat", charset="UTF-8"')
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)
        return False

    # ---------- 转发 ----------
    def _proxy(self):
        if not self._require_auth():
            return

        length = self.headers.get("Content-Length")
        body = None
        if length:
            try:
                n = int(length)
                body = self.rfile.read(n) if n > 0 else b""
            except ValueError:
                body = None

        fwd = {}
        for k, v in self.headers.items():
            if k.lower() in HOP_BY_HOP or k.lower() == "host":
                continue
            if k.lower() == "authorization":
                continue  # 不把代理口令透给上游
            fwd[k] = v
        fwd["Host"] = UP_HOST

        # /api/mem 走独立监控端口（81），避免对话流式期间主循环阻塞
        up_port = MEM_PORT if self.path.startswith("/api/mem") else UP_PORT
        try:
            conn = http.client.HTTPConnection(UP_HOST, up_port, timeout=UP_TIMEOUT)
            conn.request(self.command, self.path, body=body, headers=fwd)
            resp = conn.getresponse()
        except socket.timeout:
            return self._err(504, b"Gateway Timeout\n")
        except Exception as exc:
            return self._err(502, ("Bad Gateway: %s\n" % exc).encode())

        # 响应头：过滤逐跳头
        out_headers = []
        for k, v in resp.getheaders():
            if k.lower() in HOP_BY_HOP:
                continue
            out_headers.append((k, v))

        has_len = resp.length is not None
        self.send_response(resp.status, resp.reason)
        for k, v in out_headers:
            if k.lower() == "content-length" and not has_len:
                continue
            self.send_header(k, v)
        if has_len:
            self.send_header("Content-Length", str(resp.length))
        else:
            self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()

        try:
            if has_len:
                remaining = resp.length
                while remaining > 0:
                    piece = resp.read1(min(CHUNK, remaining))
                    if not piece:
                        break
                    self.wfile.write(piece)
                    self.wfile.flush()
                    remaining -= len(piece)
            else:
                # 流式（SSE 等）：边收边发，chunked 编码
                while True:
                    piece = resp.read1(CHUNK)
                    if not piece:
                        break
                    self.wfile.write(b"%x\r\n" % len(piece) + piece + b"\r\n")
                    self.wfile.flush()
                self.wfile.write(b"0\r\n\r\n")
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            try:
                conn.close()
            except Exception:
                pass

    def _err(self, code, msg):
        self.send_response(code)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(msg)))
        self.end_headers()
        self.wfile.write(msg)

    do_GET = _proxy
    do_POST = _proxy
    do_PUT = _proxy
    do_DELETE = _proxy
    do_HEAD = _proxy
    do_OPTIONS = _proxy


def main():
    if not AUTH_PASS:
        sys.stderr.write("WARN: AUTH_PASS 为空，代理未启用认证\n")
    srv = ThreadingHTTPServer((LISTEN_HOST, LISTEN_PORT), Handler)
    srv.daemon_threads = True
    sys.stderr.write(
        "esp32chat-proxy: %s:%d -> %s:%d  auth=%s\n"
        % (LISTEN_HOST, LISTEN_PORT, UP_HOST, UP_PORT, "on" if AUTH_PASS else "off")
    )
    srv.serve_forever()


if __name__ == "__main__":
    main()
