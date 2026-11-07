# http_stream 模块改进记录（2026-11-03）

> 起因：外部 AI 对 `http_stream` 模块提交了 10 条审阅意见。逐条复核后，确认其中
> 6 条属实且有价值，实施为本次提交（`cc926f9`）；2 条为位置错位/当前用不上，2 条暂不动。
>
> 结论：**成功路径行为不变**（路由、前端协议、响应格式均未动），只增强健壮性与可诊断性。

## 一、复核结论（10 条）

| # | 意见 | 复核 | 处理 |
|---|---|---|---|
| 1 | parseUrl 标 https 但无 TLS | 位置错位：TLS 在 `llm_http.cpp` 的 `NetworkClientSecure` 分支实现 | 不改 |
| 2 | parseUrl 不支持 IPv6/userinfo | 属实，当前只用 IPv4/域名 | 不改（用不上） |
| 3 | 头名区分大小写 | 属实：小写 `transfer-encoding` 会漏判，chunk 尺寸行被当数据 | **已修** |
| 4 | 不解析 Content-Length | 属实，但上游响应全是 chunked | 不改 |
| 5 | 读头超时硬编码 60s | 属实 | **已修**（参数化，默认 60s） |
| 6 | `len <= 0` 不区分 0 结束与异常 | 属实：超时/断流被当成正常结束，回答静默截断 | **已修** |
| 7 | trailer 只消费固定 2 字节 | 属实：带 Trailer: 头时解析错乱 | **已修** |
| 8 | chunked 忽略外层 idleTimeout | 属实但无实际影响（内层每字节已带超时） | 不改 |
| 9 | `%lx` 移植性 | 属实但轻微（ESP32 32 位安全） | **已修**（`%x` + `(unsigned)`） |
| 10 | 无错误原因回传 | 属实，与 #6 同源 | **已修**（串口日志） |

## 二、改动内容

### #3 响应头名大小写不敏感（`readResponseHead`）
- 原：`trimmed.startsWith("Transfer-Encoding")`（大小写敏感）。
- 现：取冒号前头名，`toLowerCase()` 后与 `transfer-encoding` 比较；值同样小写后查 `chunked`。
- 收益：上游发小写头名时不再把 chunk 尺寸行当数据行解析（SSE 流错乱）。

### #5 读头超时参数化（`readResponseHead`）
- 签名：`readResponseHead(client, outChunked, timeoutMs = 60000)`，内部 `readByteBlocking` 用该参数。
- 调用点不变（默认 60s）。

### #6 / #10 chunked 结束原因区分 + 日志
- 新增 `enum StreamEnd { kStreamNormal, kStreamStopped, kStreamTimeout, kStreamDisconnected, kStreamProtocol }`。
- `streamLines` 由 `void` 改为返回 `StreamEnd`：
  - `0` 块 → `kStreamNormal`；
  - 尺寸行非法 → `kStreamProtocol`；
  - 读失败 → 按 `client.connected()` 区分 `kStreamTimeout` / `kStreamDisconnected`；
  - `onLine` 返回 false → `kStreamStopped`。
- `llm_http.cpp` 收尾打印 `[http] stream end: normal|stopped|timeout|disconnected|protocol-error`，
  弱网下"回答被静默截断"可以直接从串口区分原因。
- 上层返回值保持原语义（`total > 0`），前端协议不变。

### #7 trailer 规范消费（`streamLines`）
- 原：数据块后固定读 2 字节（CRLF），0 块后直接结束。
- 现：新增 `skipTrailer()` 逐行读到空行为止；0 块后也走它（兼容带 `Trailer:` 头的 chunked 尾）。

### #9 分块大小格式明确化（`chunkedSend`）
- `%lx` + `(unsigned long)len` → `%x` + `(unsigned)len`（ESP32 32 位，语义明确）。

## 三、验证

- **编译**：`arduino-cli compile`（esp32 core 3.3.12，default 分区）通过。
  固件 1,132,592 → 1,133,472 B（+880 B，+0.08%）；Global variables 109,760 B 不变。
- **格式化**：`clang-format --dry-run` 0 差异。
- **烧录回归**（板子 IP `192.168.1.50`，`.21` 串口抓 IP + curl，10/10 通过）：
  静态页、marked.min.js、config、models、chat（gzip/明文）、summarize、空 message 400、
  坏 JSON 400，以及串口日志出现 `[http] stream end: normal`。

## 四、未实施与已知遗留

- #4 Content-Length、#8 外层 idleTimeout：场景用不上/无实际影响，暂不动；
- `lineBuf` 无上限（上游发超长无换行行时 String 吃堆）：理论风险，未改，留待需要时处理；
- `llm_http.cpp` 的 `prepareSecure()` 仍用 `setInsecure()`（demo 折衷，注释已自述），
  连公网 DeepSeek 时有 MITM 风险，登记为项目级遗留。

## 五、人工验收（2026-11-03）

- 验收人：项目作者。
- 范围：`http_stream` 模块原始代码 + 本轮 6 项改进（#3/#5/#6/#7/#9/#10）。
- 方式：对照原始实现与改进后的逐块 diff，确认成功路径行为不变（路由/前端协议/响应格式未动），
  改进点均为健壮性与可诊断性增强。
- 结论：**通过**。改进提交 `cc926f9` 作为当前基线。
