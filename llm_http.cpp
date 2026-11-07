#include "llm_http.h"

#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>

#include "http_stream.h"
#include "llm_limits.h"
#include "llm_protocol.h"
#include "memprobe.h"
#include "secrets.h"

// ============================================================
// 后端描述符（差异只有这几行）
// ============================================================

static const BackendSpec kOllamaSpec = {
    .name = "ollama",
    .baseUrl = OLLAMA_BASE_URL,
    .chatPath = "api/chat",
    .tls = false,
    .authName = nullptr,
    .authValue = nullptr,
    .streamIdleMs = 300000,
    .parseStreamLine = ollamaParseLine,
};

static const BackendSpec kOpenaiSpec = {
    .name = "openai",
    .baseUrl = OPENAI_BASE_URL,
    .chatPath = "chat/completions",
    .tls = true,
    .authName = "Authorization",
    .authValue = openaiAuthValue,
    .streamIdleMs = 60000,
    .parseStreamLine = openaiParseLine,
};

const BackendSpec &llmBackendSpec(int backend) {
  return backend == 1 ? kOpenaiSpec : kOllamaSpec;
}

// ============================================================
// 公共流程
// ============================================================

// 回答前缀累积：板上只留 LLM_KEEP_MAX 字节，完整文本走 onDelta
static void keepPrefix(String &fullAnswer, const String &delta) {
  if (fullAnswer.length() >= LLM_KEEP_MAX) {
    return;
  }
  const size_t room = LLM_KEEP_MAX - fullAnswer.length();
  fullAnswer.concat(delta.c_str(), delta.length() < room ? delta.length() : room);
}

// 流结束原因 → 日志名（http_stream.h 的 StreamEnd）
static const char *streamEndName(StreamEnd end) {
  using enum StreamEnd;
  switch (end) {
    case kStreamNormal:
      return "normal";
    case kStreamStopped:
      return "stopped";
    case kStreamTimeout:
      return "timeout";
    case kStreamDisconnected:
      return "disconnected";
    case kStreamProtocol:
      return "protocol-error";
  }
  return "unknown";
}

// 连接生命周期守卫：任何返回路径都保证 stop()，不用在 4 个出口各写一次
struct ClientGuard {
  NetworkClient &c;
  ~ClientGuard() { c.stop(); }
};

// 公共实现：明文与 TLS 客户端都从 NetworkClient 派生，流程只写这一份
static bool streamImpl(const BackendSpec &spec, NetworkClient &client, const String &model,
                       const String &prompt, JsonArray history, String &fullAnswer,
                       const std::function<void(const String &)> &onDelta,
                       const std::function<bool()> &shouldStop, size_t *totalBytes) {
  ClientGuard guard{client};
  Endpoint ep = parseUrl(spec.baseUrl);
  if (!ep.path.endsWith("/")) {
    ep.path += "/";
  }

  String head = "POST " + ep.path + spec.chatPath + " HTTP/1.1\r\n";
  head += "Host: " + ep.host + "\r\n";
  if (spec.authName) {
    head += String(spec.authName) + ": " + spec.authValue() + "\r\n";
  }
  head += "Content-Type: application/json\r\n";
  head += "Accept: text/event-stream\r\n";
  head += "Transfer-Encoding: chunked\r\n";
  head += "Connection: close\r\n\r\n";

  Serial.printf("[http] connect %s:%d\n", ep.host.c_str(), ep.port);
  memSample("pre_connect");
  if (!client.connect(ep.host.c_str(), ep.port)) {
    Serial.println("[http] connect failed");
    return false;
  }
  memSample(spec.tls ? "tls_up" : "tcp_up");
  if (!writeAll(client, head.c_str(), head.length(), 30000)) {
    Serial.println("[http] send failed");
    return false;
  }
  memSample("head_sent");

  // 逐条序列化发送（不建 messages 全量数组，历史内容随发随弃，省一份内存）
  bool first = true;
  auto sendMsg = [&](const String &role, const String &content) -> bool {
    if (!first) {
      if (!chunkedSend(client, ",", 1, 30000)) return false;
    }
    first = false;
    JsonDocument one;
    one["role"] = role;
    one["content"] = content;
    String s;
    serializeJson(one, s);
    return chunkedSend(client, s.c_str(), s.length(), 30000);
  };
  String start = "{\"model\":\"" + model + "\",\"stream\":true,\"messages\":[";
  bool ok = chunkedSend(client, start.c_str(), start.length(), 30000);
  if (ok) {
    for (JsonVariant v : history) {
      if (!sendMsg(v["role"].as<String>(), v["content"].as<String>())) {
        ok = false;
        break;
      }
    }
  }
  if (ok) {
    ok = sendMsg("user", prompt);
  }
  if (ok) {
    ok = chunkedSend(client, "]}", 2, 30000);
  }
  if (ok) {
    ok = chunkedEnd(client, 30000);
  }
  if (!ok) {
    Serial.println("[http] send failed");
    return false;
  }
  memSample("req_sent");

  bool chunked = false;
  int status = readResponseHead(client, chunked);
  if (status != 200) {
    Serial.printf("[http] bad status %d\n", status);
    return false;
  }
  memSample("resp_head");
  Serial.printf("[http] HTTP %d, streaming (chunked=%d)\n", status, (int)chunked);
  Serial.println("[answer] ----------------");

  size_t total = 0;
  bool firstDelta = true;
  StreamEnd streamEnd = streamLines(
      client, chunked,
      [&](const String &line) {
        // 前端断开、或回答已经太长：就地收工，不再往 String 里堆
        if (shouldStop && shouldStop()) {
          Serial.println("[http] stop requested, closing upstream");
          return false;
        }
        if (firstDelta) {
          firstDelta = false;
          memSample("first_delta");
        }
        if (total >= LLM_ANSWER_MAX) {
          Serial.printf("[http] answer cap %d reached, stop\n", (int)LLM_ANSWER_MAX);
          return false;
        }
        String delta;
        if (!spec.parseStreamLine(line, delta)) {
          return true;
        }
        total += delta.length();
        keepPrefix(fullAnswer, delta);
        onDelta(delta);
        return true;
      },
      spec.streamIdleMs);
  memSample("stream_end");
  memSample("upstream_closed");
  Serial.printf("[http] stream end: %s\n", streamEndName(streamEnd));
  if (totalBytes) {
    *totalBytes = total;
  }
  Serial.printf("\n[answer] ---------------- end (%u bytes, kept %u)\n", (unsigned)total,
                (unsigned)fullAnswer.length());
  return total > 0;
}

// 关掉证书校验（demo）：生产环境应改用 setCACert() 加载 CA
static void prepareSecure(NetworkClientSecure &secure, const Endpoint &ep) {
  if (ep.port == 443) {
    secure.setInsecure();
  }
  secure.setTimeout(60000);
}

bool backendStream(const BackendSpec &spec, const String &model, const String &prompt,
                   JsonArray history, String &fullAnswer,
                   const std::function<void(const String &)> &onDelta,
                   const std::function<bool()> &shouldStop, size_t *totalBytes) {
  if (spec.tls) {
    // TLS 客户端对象一构造就占一块 ssl 上下文，明文路径不该付这份钱，所以在调用点才构造
    NetworkClientSecure secure;
    prepareSecure(secure, parseUrl(spec.baseUrl));
    return streamImpl(spec, secure, model, prompt, history, fullAnswer, onDelta, shouldStop,
                      totalBytes);
  }
  NetworkClient plain;
  return streamImpl(spec, plain, model, prompt, history, fullAnswer, onDelta, shouldStop,
                    totalBytes);
}
