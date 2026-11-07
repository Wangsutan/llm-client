# http_stream 模块拆分记录（2026-11-03）

> 起因：`http_stream.h` 到 234 行时（含前一轮 #3/#5/#6/#7/#9/#10 改进），一个头文件里混着
> 四类职责 —— URL 解析、阻塞字节读写、chunked 请求体编码、响应头与流式 body 读取。
> 用户要求按职责拆分成更小的文件。
>
> 结论：**行为不变的纯文件搬移**，调用方与协议全部未动，固件大小与拆分前完全一致。

## 一、拆分后布局

| 文件 | 职责 | 行数（拆分前 → 后） |
|---|---|---|
| `http_stream.h` | 聚合头：三行 include + 说明（对外唯一入口） | 234 → 10 |
| `http_io.h` | 基础 IO：`Endpoint` / `parseUrl` / `readByteBlocking` / `writeAll` | 新增 55 |
| `http_chunked.h` | 请求体 chunked 编码：`chunkedSend` / `chunkedEnd` | 新增 26 |
| `http_response.h` | 响应头与流式 body：`readResponseHead` / `StreamEnd` / `streamLines` | 新增 161 |
| `http_stream.cpp` | `parseUrl` 实现（未动） | 25 → 25 |

## 二、共享方式

- `http_stream.h` 作为聚合头 `include` 三个子头；调用方（`llm_http.cpp`、`http_stream.cpp`）仍只
  `include "http_stream.h"`，**零改动**。
- 依赖方向单向：`http_chunked.h` / `http_response.h` → `http_io.h`，无循环 include。
- 全部仍为 header-only 模板，不新增编译单元；固件大小与拆分前一致。
- include 集合变化：原 `http_stream.h` 的 `<functional>` 只挂在 response 侧，io/chunked 不再拖它。

## 三、行为不变清单

- `parseUrl` / `readByteBlocking` / `writeAll` / `chunkedSend` / `chunkedEnd` /
  `readResponseHead` / `StreamEnd` / `streamLines` 的签名与实现逐字未变；
- chunked 0 块 / 超时 / 断开 / 协议错误的语义保持（拆分前刚实施的 #6/#10）；
- `llm_http.cpp` 与前端协议未动。

## 四、验证

- **编译**：`arduino-cli compile`（esp32 core 3.3.12，default 分区）通过；
  Sketch 1,133,472 B（86%）、Global variables 109,760 B —— 与拆分前**完全一致**。
- **格式化**：`clang-format --dry-run` 对新文件 0 差异。
- **烧录回归**（板子 IP `192.168.1.50`，`.21` 串口抓 IP + curl，10/10 通过）：
  静态页、marked.min.js、config、models、chat（gzip/明文）、summarize、空 message 400、
  坏 JSON 400，串口日志 `[http] stream end: normal`。

## 五、未动的东西

- `http_stream.cpp` 的 `parseUrl` 实现、`llm_http.cpp`、前端协议；
- 未新增任何功能、未改任何协议字段。

## 六、人工验收（2026-11-03）

- 验收人：项目作者。
- 范围：`http_stream.h` 原 234 行整体拆分为 `http_io.h` / `http_chunked.h` / `http_response.h`
  + 聚合头。
- 方式：对照拆分前后逐块实现，确认函数签名与实现逐字未变、调用方零改动、编译产物一致。
- 结论：**通过**。拆分提交 `be49c99` 作为当前基线。
