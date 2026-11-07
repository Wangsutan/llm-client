# llm-client —— ESP32 局域网 / 云端流式大模型聊天客户端

在 ESP32 上跑一个**流式 AI 聊天助手**：板子自带网页界面，既可以直接连局域网里自建的
**Ollama**（`/api/chat`），也可以连 **OpenAI 兼容的云端 API**（默认按 DeepSeek 配置），
两个后端在网页里随时切换。板载 LittleFS 存网页，浏览器负责渲染，板子只做「收流 → 解压 →
解析 → 逐块转发」。

> 目标板：ESP32（4 MB flash，`default` 分区方案）。用 arduino-esp32 core 3.3.x + C++20 编写，
> 以 Arduino 源码形式提供（`llm-client.ino` 为入口），用 arduino-cli 编译。

## 特性

- **流式多轮对话**：上游逐 token 返回，板子逐块转发为 NDJSON，浏览器边收边显示。
- **双后端**：本地 Ollama（局域网）与 OpenAI 兼容云 API（DeepSeek 实测），网页设置面板切换，选择写入 NVS。
- **多会话管理**：新建 / 切换会话、会话标题、历史摘要缓存（浏览器 localStorage）。
- **历史预算管理**：按模型设置字符预算，超预算的早期对话由 `/api/summarize` 压缩成摘要。
- **请求体 gzip 上传** + **固定 48 KB 静态工作区**：明文整块放在 `.bss`，零拷贝解析，超限返回 413。
- **网页界面**：中文 / English 切换、浅色 / 深色主题、五种配色、代码块语法高亮与下载、Markdown 渲染。
- **内存监控页** `/mem.html`：实时堆叠图 + 回放最近一次对话的内存采样（独立 81 端口，对话阻塞时也能读）。
- **串口 / 网页双交互模式**（`UI_MODE` 可配）。
- **内存探针**：各阶段堆占用、峰值、栈水位可复现（`tools/memprobe/`、`/api/mem`）。

完整模块划分与重构记录见 [`docs/`](docs/README.md)。

## 硬件与软件要求

| 项 | 要求 |
|---|---|
| 开发板 | ESP32 系列（4 MB flash；`default` 分区方案下应用区 + 1408 KB LittleFS） |
| 工具链 | `arduino-cli`；esp32 core `3.3.x`（本项目在 `3.3.12` 上验证） |
| 依赖库 | `ArduinoJson`（core 之外唯一需要装的库，v7 风格 `JsonDocument`） |
| 后端 | 局域网里能跑 Ollama 的主机，或一个 OpenAI 兼容 API 的 Key（如 DeepSeek） |
| 电脑 | 用于编译烧录；网页端浏览器需支持 `CompressionStream`（不支持时自动回退明文上传，仍可用） |

## 目录结构

```
llm-client.ino     入口：连 Wi-Fi、按 UI_MODE 起网页 / 串口界面
llm_client.*       配置管理（NVS 持久化）、后端分发
llm_http.*         两后端的统一流式管线（Ollama / OpenAI 兼容）
llm_protocol.*     协议解析纯函数（Ollama JSON 行 / OpenAI SSE 行）
payload.*          请求体管线：gzip 解压 + 固定工作区 + 413 判定
web_server.*       路由表与生命周期        web_api.*   配置 / 模型 / 内存接口
web_chat.*         聊天与摘要处理          web_static.* LittleFS 静态文件
http_io/chunked/response + http_stream.*   HTTP 流式工具（URL 解析 / 阻塞读写 / chunked / 响应头）
mem_monitor.*      独立 81 端口监控任务     memprobe.*  内存埋点
console_ui.*       串口输入界面            wifi_manager.* Wi-Fi 连接与重连
secrets.example.h  配置模板（复制成 secrets.h 填真实值；secrets.h 不入库）
web/               前端单页（index.html / style.css / app.js / mem.html + marked、highlight 本地包）
docs/              技术文档（内存实测与占用图、数据流图、开发记录）
tools/             构建、前端回归测试、公网反代、内存探针、源码打印、Graphviz 渲染
```

## 快速开始

### 1. 装工具链

```bash
# arduino-cli（以官方脚本为例）
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
export PATH="$HOME/bin:$PATH"          # 或把 arduino-cli 放到自己的 PATH 里

arduino-cli core update-index
arduino-cli core install esp32:esp32   # 3.3.x
arduino-cli lib install ArduinoJson
```

### 2. 写配置 `secrets.h`

```bash
cp secrets.example.h secrets.h
chmod 600 secrets.h          # 里面是 Wi-Fi 密码和 API Key
```

`secrets.h` 已被 `.gitignore` 忽略，**不会**进版本库；仓库里只有不含真实值的 `secrets.example.h`。

逐项说明：

| 宏 | 含义 | 示例 |
|---|---|---|
| `WIFI_SSID` / `WIFI_PASS` | 板子要连的 2.4 GHz Wi-Fi（ESP32 不支持 5 GHz） | `"my-ap"` / `"********"` |
| `UI_MODE` | 交互方式：`0` 仅串口、`1` 仅网页、`2` 串口 + 网页（默认） | `2` |
| `LLM_BACKEND` | 开机默认后端：`0` Ollama、`1` OpenAI 兼容（网页里还能再切） | `0` |
| `OLLAMA_BASE_URL` | Ollama 服务地址，**不带** `/api/chat` | `"http://192.168.1.10:11434"` |
| `OLLAMA_MODEL` | 默认 Ollama 模型名（网页下拉从 `/api/tags` 拉取） | `"qwen2.5:7b"` |
| `OPENAI_BASE_URL` | OpenAI 兼容服务的 base，**不带** `/chat/completions` | `"https://api.deepseek.com/v1"` |
| `OPENAI_API_KEY` | 对应服务的 API Key | `"sk-..."` |
| `OPENAI_MODEL` | 默认云端模型名 | `"deepseek-chat"` |
| `HISTORY_BUDGET_DEFAULT` | 默认历史字符预算（其他模型都落到它） | `12000` |
| `HISTORY_BUDGET_20B` / `_DEEPSEEK` / `_QWEN3` | 按模型名匹配的专用预算 | `40000` / `25000` / `15000` |
| `SUMMARY_INPUT_MAX` | 单次摘要压缩允许喂给模型的字符上限 | `10000` |

> 网页设置面板里的「后端 / 模型」选择会写进 NVS，**覆盖** `LLM_BACKEND` / 模型默认值；
> 想回到 secrets.h 的默认值，把 NVS 里的 `llmcfg` 命名空间清掉（或重新烧录整片 flash）。

**Ollama 侧要注意**：默认 Ollama 只监听 `127.0.0.1`，板子连不上。启动时放行局域网：

```bash
OLLAMA_HOST=0.0.0.0:11434 ollama serve
# 然后先在本机确认：curl http://<主机IP>:11434/api/tags
```

### 3. 编译 + 烧录

一键（编译固件 → 烧录固件 → 打包 `web/` 为 LittleFS → 烧录文件系统）：

```bash
./tools/build.sh                       # 默认串口 /dev/ttyUSB0
PORT=/dev/ttyACM0 ./tools/build.sh     # 换串口
ACLI=~/bin/arduino-cli ./tools/build.sh
```

手动等价步骤（不想用脚本时）：

```bash
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=default .
arduino-cli upload  -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32:PartitionScheme=default .

# 网页文件系统：default 分区方案下 spiffs 区在 0x290000、大小 1408K
MKFS=$(find ~/.arduino15/packages/esp32/tools/mklittlefs -name mklittlefs -type f | head -1)
ESPTOOL=$(find ~/.arduino15/packages/esp32/tools/esptool_py -name esptool -type f | head -1)
"$MKFS" -c web -b 4096 -p 256 -s 0x160000 /tmp/littlefs.bin
"$ESPTOOL" --port /dev/ttyUSB0 write-flash 0x290000 /tmp/littlefs.bin
```

> 换了分区方案（比如改成 `huge_app` / 自定义表）要同步改 `FS_OFFSET` / `FS_SIZE`，
> 否则网页文件系统会烧到错误位置，表现为网页 404 或 `LittleFS mount failed`。

### 4. 连上板子

串口助手（115200）里会看到：

```
[llm-client] boot OK (UI_MODE=2)
[wifi] connecting to my-ap ...
[wifi] connected, ip=192.168.1.50 rssi=-52 dBm
[web] http://192.168.1.50/
```

浏览器打开 `http://<板子IP>/` 即可。本文示例统一用 `192.168.1.50`，实际以串口打印为准。

### 5. 网页用法

- **聊天**：输入框回车或点「发送」；回答流式出现，结束后渲染 Markdown 与代码高亮。
- **会话**：左上「☰」打开会话侧栏，「＋ 新建会话」开新会话；会话与摘要保存在浏览器 localStorage。
- **设置**（右上「⚙」）：语言（中文 / English）、主题（浅色 / 深色）、配色、后端（Ollama / DeepSeek）、模型。
  点「保存并生效」后写 NVS，下一次请求即用新配置。
- **代码块**：右上角标注语言，可一键复制 Markdown 源、按语言扩展名下载。
- **内存监控**：`http://<板子IP>/mem.html`，实时堆叠图 + 回放最近一次对话的采样。
  ESP32 上还并行开了 **81 端口**，只服务 `/api/mem` —— 长回答流式期间主循环被占，
  页面仍能从 `http://<板子IP>:81/api/mem` 取到实时数据。

串口模式（`UI_MODE` 为 0 或 2 时）：串口里输入问题回车，回答边收边打印到串口。

## HTTP 接口

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/` `/style.css` `/app.js` `/marked.min.js` `/highlight.min.js` `/mem.html` | LittleFS 静态文件 |
| POST | `/api/chat` | `multipart/form-data`，单文件字段 `payload`；内容为 `{message, history[]}` JSON（`.gz` 结尾则按 gzip 解压）。响应为 NDJSON：`{"delta":"..."}` 逐行，`{"done":true}` 结束，出错为 `{"error":...}` |
| POST | `/api/summarize` | 同理，body `{text}`，返回 `{"summary":"..."}` |
| GET/POST | `/api/config` | 读 / 写后端与模型（写后持久化到 NVS） |
| GET | `/api/models` | 拉 Ollama `/api/tags` 的模型列表 + 云端候选模型 |
| GET | `/api/mem` | 实时内存状态 + 最近一次请求的采样序列（80 端口也提供，81 端口独立服务） |

错误码：`400`（空消息 / 坏 JSON）、`413`（`payload_too_large`，明文超 48 KB 工作区）；
回答超过 `LLM_ANSWER_MAX`（64 K 字符）会被截断并在流里注明。

## 内存与预算

- 请求明文整块进 **48 KB 静态工作区**（`llm_limits.h` 的 `WORK_BUF_SIZE`），解压目标也是它；
  单请求明文上限 = 48 KB − 1 KB 余量，**不随堆碎片浮动**。
- 板上只保留回答前缀（`LLM_KEEP_MAX` = 2 KB），完整文本留在浏览器。
- 不调优时一轮对话的爆点估算、摘要机制为什么安全，见
  [`docs/开发/对话长度与内存上限-2026-11-04.md`](docs/开发/对话长度与内存上限-2026-11-04.md)；
  实测数据与占用图见 [`docs/内存/`](docs/内存/)。
- 想省 flash 可以沿用 `build_opt.h`（`-fno-exceptions` 等，实测 −2.9%）。

## 工具

| 工具 | 用途 |
|---|---|
| `tools/build.sh` | 编译 + 烧录固件与 LittleFS |
| `tools/web-test/` | 前端 headless 回归测试（puppeteer-core 实机访问板子，改页面后先跑） |
| `tools/esp32chat-proxy/` | 公网暴露用的反代（回环监听 + Basic 认证 + 流式转发） |
| `tools/memprobe/` | 内存探针采集（串口 + HTTP 并行，Ollama / gzip / TLS 各阶段） |
| `tools/print/` | 把 C++ 源码排版成可彩色打印的 PDF（自动排除 secrets.h） |
| `tools/render-graphviz.sh` | 渲染 `docs/` 下 Graphviz 源为 svg/png |

## 安全说明（重要）

- 板载网页**没有任何登录鉴权**：不要把 80 / 81 端口直接暴露到公网。
  如需外网访问，用 `tools/esp32chat-proxy/` 这类「只监听回环 + Basic 认证 + 反向代理」的方案，
  再经隧道（frp 等）转发。
- `secrets.h` 含 Wi-Fi 密码与 API Key，已在 `.gitignore` 中；**别把它提交上去**。
  仓库与历史里都不含真实凭据。
- 历史会话保存在浏览器 localStorage，清浏览器数据即可清除；`/api/summarize` 会把丢弃的
  对话拼进摘要请求发往当前后端，介意的话把预算调大或定期新建会话。

## 常见问题

| 现象 | 排查 |
|---|---|
| 网页 404 / `LittleFS mount failed` | 网页文件系统没烧或烧错位置：重跑 `tools/build.sh` 的第 3、4 步，确认分区方案与 `FS_OFFSET` 一致 |
| 能连 Wi-Fi 但 Ollama 报错 | 板子连不上 `OLLAMA_BASE_URL`：确认 Ollama 以 `OLLAMA_HOST=0.0.0.0` 启动、防火墙放行，本机 `curl` 先验证 |
| 设置里模型列表为空 | `/api/models` 需要板子能访问 Ollama `/api/tags`；云端候选固定为 `deepseek-chat` / `deepseek-reasoner` |
| 提问返回 413 | 请求明文超过 48 KB 工作区（历史预算调太大或一次粘太长的文本），调小预算或减少轮数 |
| 云后端首问很慢 | TLS 握手在 ESP32 上要几百毫秒到 1 秒级，属正常；复用连接后续会快 |
| 长回答突然停 | 超过 `LLM_ANSWER_MAX`（64 K 字符）主动截断，流里会额外发一条「已截断」提示 |

## 文档

技术文档索引见 [`docs/README.md`](docs/README.md)：内存实测、内存占用图、数据流图、
模块拆分与重构记录、前端回归测试、对话长度与内存上限分析等。

## License

MIT，见 [LICENSE](LICENSE)。
