# esp32chat-proxy

把板上这个 Web 界面从内网转发到公网时用的反向代理。板子本身**没有鉴权**，
直接暴露端口等于把你的模型额度交给扫描器，所以中间加这一层。

## 链路

```
公网 <ECS>:8090
  └─ frps:7000 ── <内网某台跑 frpc 的机> [esp32chat]
       └─ 127.0.0.1:8099  proxy.py（本脚本，Basic 认证 + 流式转发）
            └─ <板子 IP>:80  ESP32 聊天
```

## 为什么是 Python 标准库

目标机上只有 `python3`，没有 nginx / caddy。这层职责单一（认证 + 透传），
不值得为它装一个 Web 服务器。

## 要点

- 只绑 `127.0.0.1`，对外仅经 frp 隧道，不直接暴露在局域网
- Basic 认证用 `hmac.compare_digest` 做常量时间比较
- **不把 `Authorization` 头透给上游**，否则口令会进板子的请求日志
- 支持 chunked 流式转发：`/api/chat` 返回 `application/x-ndjson`，
  代理逐块 flush，回复边生成边显示，不会等整包结束
- `UP_TIMEOUT=600`：模型推理慢，超时给足
- 过滤逐跳头（Connection / Transfer-Encoding / Upgrade 等）

## 配置

全部走环境变量，建议放 `EnvironmentFile`（`chmod 600`），不要写进单元文件：

| 变量 | 默认 | 说明 |
|---|---|---|
| `UP_HOST` | `192.168.1.50` | 板子地址 |
| `UP_PORT` | `80` | 板子端口 |
| `LISTEN_HOST` | `127.0.0.1` | 监听地址，保持回环 |
| `LISTEN_PORT` | `8099` | 监听端口 |
| `AUTH_USER` | `esp32` | Basic 用户名 |
| `AUTH_PASS` | 空 | Basic 口令；**为空则不做认证**，仅调试用 |

口令生成：

```bash
python3 -c "import secrets,string;a=string.ascii_letters+string.digits;print(''.join(secrets.choice(a) for _ in range(16)))"
```

## systemd 用户单元

```ini
[Unit]
Description=ESP32 聊天项目反向代理
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
EnvironmentFile=/path/to/env
WorkingDirectory=/path/to/esp32chat
ExecStart=/usr/bin/python3 /path/to/proxy.py
Restart=always
RestartSec=3

[Install]
WantedBy=default.target
```

用户单元要 `loginctl enable-linger <user>`，否则登出后进程随之结束、重启不恢复。

## 自测

```bash
# 无认证应 401
curl -s -o /dev/null -w '%{http_code}\n' http://127.0.0.1:8099/
# 带认证应 200
curl -s -o /dev/null -w '%{http_code}\n' -u "esp32:$PW" http://127.0.0.1:8099/
# 流式应逐块到达
echo '{"message":"回答：OK","history":[]}' > /tmp/t.json
curl -sN -u "esp32:$PW" -X POST http://127.0.0.1:8099/api/chat \
     -F "payload=@/tmp/t.json;type=application/json"
```

## 排查

```bash
systemctl --user status esp32chat
journalctl --user -u esp32chat -n 50
```

安全组放行与否的判据：`/dev/tcp` **超时 = 被拦**，**connection refused = 已放行但无监听**。

## 已知局限

- 单一共享口令，没有按用户区分，也没有失败次数限制
- 明文 HTTP，口令在传输中是 base64 而非加密；公网上跑敏感内容应加 TLS
