#pragma once

#include <Arduino.h>

#include <functional>

#include "http_io.h"

// 读 HTTP 响应头：返回状态码（失败 -1），outChunked 标记 Transfer-Encoding: chunked。
// timeoutMs 为读头空闲超时（默认 60s，流式响应头的合理上限）。
template <typename Client>
int readResponseHead(Client& client, bool& outChunked, unsigned long timeoutMs = 60000) {
  int statusCode = -1;
  outChunked = false;
  bool headerDone = false;
  String headLine;
  while (!headerDone) {
    int c = readByteBlocking(client, timeoutMs);
    if (c < 0) {
      return -1;
    }
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      String trimmed = headLine;
      headLine = "";
      trimmed.trim();
      if (trimmed.length() == 0) {
        headerDone = true;
        break;
      }
      if (statusCode == -1) {
        int sp = trimmed.indexOf(' ');
        if (sp > 0) {
          statusCode = trimmed.substring(sp + 1).toInt();
        }
      } else {
        // 头名大小写不敏感（RFC 7230）：取冒号前的头名再比较
        int colonPos = trimmed.indexOf(':');
        if (colonPos > 0) {
          String name = trimmed.substring(0, colonPos);
          name.trim();
          name.toLowerCase();
          if (name == "transfer-encoding") {
            String value = trimmed.substring(colonPos + 1);
            value.toLowerCase();
            if (value.indexOf("chunked") >= 0) {
              outChunked = true;
            }
          }
        }
      }
    } else {
      headLine += (char)c;
    }
  }
  return statusCode;
}

// 流式读取的结束原因：区分"正常读完"与"超时 / 断开 / 协议错误"，便于上层日志与诊断
enum class StreamEnd {
  kStreamNormal = 0,    // chunked 读到 0 块 / 非 chunked 连接关闭（干净结束）
  kStreamStopped,       // onLine 返回 false（调用方主动停止）
  kStreamTimeout,       // 空闲超时
  kStreamDisconnected,  // 读到一半连接断开
  kStreamProtocol,      // chunked 协议错误（尺寸行非法）
};

// 流式读取 HTTP body（自动处理 chunked 编码），按行回调；onLine 返回 false 提前停止。
// 返回本次结束的原因（见 StreamEnd）。
template <typename Client>
StreamEnd streamLines(Client& client, bool chunked,
                      const std::function<bool(const String&)>& onLine,
                      unsigned long idleTimeoutMs) {
  using enum StreamEnd;  // 本函数内直接用 kStreamXxx 名字

  long chunkLeft = 0;
  bool trailerPending = false;
  String lineBuf;

  // 读掉 trailer 区：0 块之后、或数据块 CRLF 之后，读到空行为止。
  // 标准空 trailer 就是一行 CRLF；带 Trailer: 头时逐行吞掉，不再只读固定 2 字节。
  auto skipTrailer = [&]() -> bool {
    for (;;) {
      String line;
      for (;;) {
        int c = readByteBlocking(client, idleTimeoutMs);
        if (c < 0) return false;
        if (c == '\n') break;
        if (c != '\r') line += (char)c;
      }
      if (line.isEmpty()) return true;
    }
  };

  // 读 payload 字节：-1=失败（超时/断开，由调用方判），-2=0 块正常结束，-3=协议错误
  auto readPayloadByte = [&]() -> int {
    if (chunkLeft > 0) {
      int c = readByteBlocking(client, idleTimeoutMs);
      if (c < 0) return -1;
      if (--chunkLeft == 0) trailerPending = true;
      return c;
    }
    if (!chunked) {
      return readByteBlocking(client, idleTimeoutMs);
    }
    if (trailerPending) {
      if (!skipTrailer()) return -1;
      trailerPending = false;
    }
    String sizeLine;
    for (;;) {
      int c = readByteBlocking(client, idleTimeoutMs);
      if (c < 0) return -1;
      if (c == '\n') break;
      if (c != '\r') sizeLine += (char)c;
    }
    sizeLine.trim();
    if (sizeLine.isEmpty()) return -3;
    char* endp = nullptr;
    long len = strtol(sizeLine.c_str(), &endp, 16);
    if (endp == sizeLine.c_str() || *endp != '\0' || len < 0) return -3;
    if (len == 0) {
      if (!skipTrailer()) return -1;
      return -2;
    }
    chunkLeft = len;
    int c = readByteBlocking(client, idleTimeoutMs);
    if (c < 0) return -1;
    if (--chunkLeft == 0) trailerPending = true;
    return c;
  };

  unsigned long last = millis();
  while (true) {
    int c = readPayloadByte();
    if (c == -2) return kStreamNormal;
    if (c == -3) return kStreamProtocol;
    if (c < 0) {
      if (chunked) {
        return client.connected() ? kStreamTimeout : kStreamDisconnected;
      }
      if (!client.connected()) return kStreamNormal;  // 非 chunked：连接关闭 = 正常读完
      if (millis() - last > idleTimeoutMs) return kStreamTimeout;
      continue;
    }
    last = millis();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      String line = lineBuf;
      lineBuf = "";
      line.trim();
      if (line.length() && !onLine(line)) {
        return kStreamStopped;
      }
    } else {
      lineBuf += (char)c;
    }
  }
}
