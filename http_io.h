#pragma once

#include <Arduino.h>

// HTTP 目标解析结果
struct Endpoint {
  String host;
  int port;
  String path;
};

// 解析 http(s)://host[:port]/path
Endpoint parseUrl(const String& url);

// 阻塞读一个字节：数据未到则等待重试，超时/断开返回 -1
template <typename Client>
int readByteBlocking(Client& client, unsigned long timeoutMs) {
  unsigned long t0 = millis();
  while (true) {
    int c = client.read();
    if (c >= 0) {
      return c & 0xFF;
    }
    if (!client.connected()) {
      return -1;
    }
    if (millis() - t0 > timeoutMs) {
      return -1;
    }
    delay(5);
  }
}

// 写满（处理 TCP 部分写），超时/断开返回 false
template <typename Client>
bool writeAll(Client& client, const char* data, size_t len, unsigned long timeoutMs) {
  size_t sent = 0;
  unsigned long t0 = millis();
  while (sent < len) {
    size_t n = client.write((const uint8_t*)data + sent, len - sent);
    if (n > 0) {
      sent += n;
      t0 = millis();
      continue;
    }
    if (!client.connected()) {
      return false;
    }
    if (millis() - t0 > timeoutMs) {
      return false;
    }
    delay(10);
  }
  return true;
}
