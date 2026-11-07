#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <functional>

// 一个后端的全部差异集中在这个描述符里：地址、鉴权、路径、超时、协议解析。
// 公共流程（建连 / 请求头 / 分块发送 messages / 读响应头 / 逐行回调 / 收尾）
// 只在 llm_http.cpp 里写一份，ollama 与 OpenAI 兼容后端共用。
struct BackendSpec {
  const char *name;            // 日志用："ollama" / "openai"
  const char *baseUrl;         // 如 http://192.168.1.10:11434
  const char *chatPath;        // 相对路径（不带前导 /）："api/chat" / "chat/completions"
  bool tls;                    // true：走 NetworkClientSecure
  const char *authName;        // 额外请求头名；nullptr 表示没有
  String (*authValue)();       // 额外请求头值
  unsigned long streamIdleMs;  // 流式读空闲超时
  // 协议层：流式行 → 增量文本（取到返回 true）
  bool (*parseStreamLine)(const String &line, String &delta);
};

// 0 = Ollama，1 = OpenAI 兼容（DeepSeek）
const BackendSpec &llmBackendSpec(int backend);

// 流式：逐段收到回答时调用 onDelta；fullAnswer 只保留前 LLM_KEEP_MAX 字节；
// shouldStop 返回 false 时立即停止读上游；totalBytes 回传回答总字节数。
[[nodiscard]] bool backendStream(const BackendSpec &spec, const String &model, const String &prompt,
                                 JsonArray history, String &fullAnswer,
                                 const std::function<void(const String &)> &onDelta,
                                 const std::function<bool()> &shouldStop, size_t *totalBytes);
