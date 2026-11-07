# web_server 模块拆分记录（2026-11-03）

> 起因：`web_server.cpp` 到 229 行时，一个文件里混着五类职责 —— JSON 响应小工具、
> LittleFS 静态文件、`/api/config` 与 `/api/models` 配置接口、`/api/chat` 流式聊天与
> `/api/summarize` 摘要、以及服务器生命周期。用户在人审机上审完该模块后要求按职责拆分。
>
> 结论：**行为不变的纯文件搬移**，HTTP 协议、路由表、错误文案、日志前缀、内存打点顺序全部未动。

## 一、拆分后布局

| 文件 | 职责 | 行数（拆分前 → 后） |
|---|---|---|
| `web_server.h` | 对外接口：`webBegin()` / `webHandle()`（不变） | 5 → 5 |
| `web_internal.h` | 模块内部共享声明：`webServer()` 引用、`sendJson()` / `sendError()`、各 handler 原型 | 新增 |
| `web_server.cpp` | 唯一的 `WebServer` 实例、`sendJson()` / `sendError()`、路由表、`webBegin()` / `webHandle()` | 229 → 50 |
| `web_static.cpp` | LittleFS 静态文件服务：`/`（index.html）、`/marked.min.js` | 新增 43 |
| `web_api.cpp` | 配置接口：`GET/POST /api/config`、`GET /api/models` | 新增 32 |
| `web_chat.cpp` | 聊天与摘要：`POST /api/chat`（流式 NDJSON）、`POST /api/summarize`、upload 回调绑定 | 新增 126 |

拆分后每个文件单一职责，最大的是 `web_chat.cpp`（126 行），仍是单条业务管线，不再混杂
静态文件与配置接口。

## 二、共享方式

- `WebServer` 实例改为 `s_server`，模块内通过 `webServer()` 返回引用；对外接口不变。
- `sendJson()` / `sendError()` 从 `static` 改为模块内部函数，声明统一放 `web_internal.h`，
  避免每个拆分文件各自复制一份（原来 15 处 `Cache-Control: no-store` 的收敛点仍只有一处）。
- `web_internal.h` 只被 `web_*.cpp` 引用，`.ino` 仍只 include `web_server.h`。

## 三、行为不变清单

- 路由表、URI、HTTP 方法、`handleUpload` 上传回调绑定方式完全一致；
- `sendJson()` / `sendError()` 的 `Cache-Control: no-store` 与错误码文案一致；
- `/api/chat` 的 chunked 流式输出、截断提示、失败行、`done` 行、memSample 打点顺序一致；
- `/api/summarize` 的截断/释放顺序与摘要提示词一致；
- 日志前缀（`[web]` / `[fs]`）不变；
- 拆分时 `web_chat.cpp` 需要 `#include "secrets.h"` 才能拿到 `SUMMARY_INPUT_MAX`
  （该宏定义在 secrets.h，原文件即如此，不是新增依赖）。

## 四、验证

- **编译**：`arduino-cli compile`（esp32 core 3.3.12，`PartitionScheme=default`）通过。

| 项 | 拆分前基线 | 拆分后 |
|---|---|---|
| 固件大小 | 1,130,368 B（86%） | 1,132,592 B（+2,224 B，+0.2%） |
| Global variables | 109,760 B（33%） | 109,760 B（不变） |

  flash 略增来自拆分后多个独立编译单元（各文件一个 `.o`）与外部链接函数无法同文件内联，
  量级可忽略；堆布局与内存口径不变。

- **格式化**：`clang-format --dry-run`（项目 `.clang-format`，Google 风格）对新文件 0 差异。
- **回归**（烧录后实测，板子 IP `192.168.1.50`，`.21` 上串口抓 IP + curl，9/9 通过）：

| 项 | 结果 |
|---|---|
| `GET /`（index.html） | 200，页面内容正常 |
| `GET /marked.min.js` | 200 |
| `GET /api/config` | 200，JSON 含 `backend` / `model` |
| `GET /api/models` | 200 |
| `POST /api/chat`（gzip multipart，与前端一致） | 200，NDJSON 流式，含 `"done":true` |
| `POST /api/chat`（明文 multipart） | 200，NDJSON 流式，含 `"done":true` |
| `POST /api/summarize`（gzip multipart） | 200，JSON 含 `summary` |
| `POST /api/chat` 空 message | 400 |
| `POST /api/chat` 坏 JSON | 400 |

## 五、未动的东西

- `web_server.h` 的接口注释仍只提聊天接口，未改（避免噪音 diff）；
- 前端 `web/index.html` 与 LittleFS 打包流程未动；
- 未新增任何功能、未改任何协议字段。

## 六、人工验收（2026-11-03）

- 验收人：项目作者。
- 方式：在人审机上逐功能块核对拆分前后的实现（`server` → `webServer()` 引用、`static` 去掉、
  include 重新分配、clang-format 格式统一），确认路由表、响应文案、日志前缀、`memSample`
  打点顺序、`payloadRelease` 时机均未变化，无逻辑改动。
- 结论：**通过**。web_server 模块拆分重构（提交 `0dff924`）及烧录回归（9/9）经人工验收，作为当前基线。
