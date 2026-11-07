#include "llm_protocol.h"

#include <ArduinoJson.h>

#include "secrets.h"

// Ollama 流式：每行是一份完整 JSON，增量在 message.content
bool ollamaParseLine(const String &line, String &delta) {
  JsonDocument r;
  if (deserializeJson(r, line)) {
    return false;
  }
  const char *content = r["message"]["content"].as<const char *>();
  if (!content || !*content) {
    return false;
  }
  delta = content;
  return true;
}

// OpenAI 兼容流式：SSE，行以 "data: " 开头，结束标记 [DONE]，增量在 choices[0].delta.content
// 用 Filter 只解析需要的字段，省掉整份 JSON 的结构分配
bool openaiParseLine(const String &line, String &delta) {
  if (!line.startsWith("data:")) {
    return false;
  }
  String payload = line.substring(5);
  payload.trim();
  if (!payload.length() || payload == "[DONE]") {
    return false;
  }
  static JsonDocument filter;
  if (filter.isNull()) {
    filter["choices"][0]["delta"]["content"] = true;
  }
  JsonDocument r;
  DeserializationError err = deserializeJson(r, payload, DeserializationOption::Filter(filter));
  if (err) {
    Serial.printf("[sse-fail] %s\n", err.c_str());
    return false;
  }
  const char *content = r["choices"][0]["delta"]["content"].as<const char *>();
  if (!content || !*content) {
    return false;
  }
  delta = content;
  return true;
}

String openaiAuthValue() {
  return String("Bearer ") + OPENAI_API_KEY;
}
