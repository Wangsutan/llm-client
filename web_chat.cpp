#include <ArduinoJson.h>
#include <WebServer.h>

#include "llm_client.h"
#include "llm_limits.h"
#include "memprobe.h"
#include "payload.h"
#include "secrets.h"
#include "web_internal.h"

// 请求体管线实例（工作区在 .bss；WebServer 串行处理，单实例足够）
static PayloadReceiver s_payload;

// ---------- 业务 ----------

// upload 回调转给 payload 接收器（它自己持有静态工作区与 gzip 解压器）
void handleUpload() {
  s_payload.uploading(webServer());
}

void handleChat() {
  WebServer &server = webServer();
  JsonDocument in;
  if (!s_payload.read(server, in)) {
    return;
  }
  String question = in["message"].as<String>();
  if (question.isEmpty()) {
    s_payload.release();
    sendError(400, "empty message");
    return;
  }
  // 历史上下文：直接引用 in 文档中的数组，避免序列化/再解析副本
  JsonArray history = in["history"].as<JsonArray>();
  memSample("parsed");
  Serial.printf("[web] chat: %s (history %u items, heap %u, stack left %u)\n", question.c_str(),
                history.size(), (unsigned)ESP.getFreeHeap(),
                (unsigned)uxTaskGetStackHighWaterMark(NULL));

  // 逐 token 转发：合并小片段后以 NDJSON chunk 发送
  server.sendHeader("Cache-Control", "no-store");
  server.chunkResponseBegin("application/x-ndjson");
  memSample("chunk_begin");
  String answer;  // 只是给 llmStream 的落点：库里只填前 LLM_KEEP_MAX 字节，不再存全文
  String pending;
  size_t answerBytes = 0;
  auto flushPending = [&]() {
    if (!pending.length()) {
      return;
    }
    JsonDocument d;
    d["delta"] = pending;
    String s;
    serializeJson(d, s);
    s += '\n';
    server.chunkWrite(s.c_str(), s.length());
    pending = "";
  };
  // 前端断开（刷新页面/关标签/断网）后就没必要继续把上游吐的字节堆进 String 了
  auto shouldStop = [&]() { return !server.client().connected(); };
  bool ok = llmStream(
      question, history, answer,
      [&](const String &delta) {
        pending += delta;
        if (pending.length() >= 64) {
          flushPending();
        }
      },
      shouldStop, &answerBytes);
  memSample("llm_done");
  flushPending();
  if (answerBytes >= LLM_ANSWER_MAX) {
    // 回答被上限截断：明确告诉前端，而不是让它以为模型就说了这么多
    const char *note = "{\"delta\":\"\\n\\n（回答超过长度上限，已截断）\"}\n";
    server.chunkWrite(note, strlen(note));
  } else if (!ok) {
    const char *line = "{\"error\":\"llm request failed\"}\n";
    server.chunkWrite(line, strlen(line));
  }
  const char *doneLine = "{\"done\":true}\n";
  server.chunkWrite(doneLine, strlen(doneLine));
  memSample("done_written");
  server.chunkResponseEnd();
  memSample("resp_end");
  Serial.printf("[web] done: answer %u bytes, heap %u, stack left %u\n", (unsigned)answerBytes,
                (unsigned)ESP.getFreeHeap(), (unsigned)uxTaskGetStackHighWaterMark(NULL));
  s_payload.release();  // 本轮结束：只标记工作区空闲（历史字符串还指在里面，下一轮才会被覆盖）
  memSample("released");
  memEnd();
}

// 历史压缩：入参和 /api/chat 走同一条上传管线（multipart 文件字段 + gzip + payloadCap），
// 所以请求体不再经过 WebServer 的 _currentArgs，也不会出现"整包复制四份"。
void handleSummarize() {
  WebServer &server = webServer();
  JsonDocument in;
  if (!s_payload.read(server, in)) {
    return;
  }
  // 先截断再复制：以前是 as<String>() 整包复制完才 substring，白花一份内存
  const char *raw = in["text"].as<const char *>();
  size_t len = raw ? strnlen(raw, SUMMARY_INPUT_MAX + 1) : 0;
  if (len > SUMMARY_INPUT_MAX) {
    len = SUMMARY_INPUT_MAX;
  }
  if (len == 0) {
    s_payload.release();
    sendError(400, "empty text");
    return;
  }
  String text(raw, len);
  s_payload.release();  // 文本已复制出来，工作区立刻可以给下一个请求用（上游可能要跑几十秒）
  Serial.printf("[web] summarize: %u chars\n", text.length());
  String prompt =
      "请用不超过200字总结以下对话的核心内容（用户是谁、讨论了什么"
      "、得出什么结论）：\n\n" +
      text;
  String answer;
  bool ok = llmStream(prompt, JsonArray(), answer, [](const String &) {});
  if (!ok) {
    sendError(502, "summarize failed");
    return;
  }
  JsonDocument out;
  out["summary"] = answer;
  String s;
  serializeJson(out, s);
  sendJson(200, s);
}
