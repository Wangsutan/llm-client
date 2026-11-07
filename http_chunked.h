#pragma once

#include "http_io.h"

// 发送一个 HTTP chunked 分块（用于流式请求体，避免整份 body 落地）
template <typename Client>
bool chunkedSend(Client& client, const char* data, size_t len, unsigned long timeoutMs) {
  if (len == 0) {
    return true;
  }
  char head[16];
  int n = snprintf(head, sizeof(head), "%x\r\n", (unsigned)len);
  if (!writeAll(client, head, n, timeoutMs)) {
    return false;
  }
  if (!writeAll(client, data, len, timeoutMs)) {
    return false;
  }
  return writeAll(client, "\r\n", 2, timeoutMs);
}

// 结束 chunked 请求体
template <typename Client>
bool chunkedEnd(Client& client, unsigned long timeoutMs) {
  return writeAll(client, "0\r\n\r\n", 5, timeoutMs);
}
