# ESP32 core WebServer 流式传输补丁

本机 ESP32 Arduino core 3.3.12 的 WebServer 在流式（chunked）响应时有两个缺陷，
已直接修改 `~/.arduino15/packages/esp32/hardware/esp32/3.3.12/libraries/WebServer/src/WebServer.cpp`：

1. `chunkWrite()` 一次 write 失败就静默中止（`_chunkedResponseActive=false`），
   客户端读得慢/发送缓冲满时会丢数据 → 改为 `_writeAllNetClient()` 循环写+重试（30s 超时）。
2. `setSSE()` 从未被调用，流式响应结束后 WebServer 立即关闭连接，TCP 发送缓冲里的数据丢失
   → `chunkResponseBegin()` 中调用 `_currentClient.setSSE(true)`，保持连接直到客户端读完。

## 修改点

- 新增文件级 static 函数 `_writeAllNetClient(NetworkClient&, const char*, size_t)`：
  循环 write，出错/断开时重试（30s），避免部分写被当成失败。
- `chunkWrite()` 中三处 `_chunkedClient.write(...)` 改为 `!_writeAllNetClient(...)`。
- `chunkResponseBegin()` 中 `_currentClientWrite(header...)` 改为 `_writeAllNetClient(...)`，
  并新增 `_currentClient.setSSE(true)`。

## 注意

升级 Arduino core 后补丁会丢失，需要重新应用；配合本项目 openai_client.cpp 的流式实现使用。
