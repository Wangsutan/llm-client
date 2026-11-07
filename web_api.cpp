#include <ArduinoJson.h>
#include <WebServer.h>

#include "llm_client.h"
#include "memprobe.h"
#include "web_internal.h"

// ---------- 配置 ----------

void handleConfigGet() {
  sendJson(200, llmGetConfigJson());
}

void handleConfigPost() {
  WebServer& server = webServer();
  String body = server.arg("plain");
  JsonDocument in;
  if (!body.length() || deserializeJson(in, body)) {
    sendError(400, "bad json");
    return;
  }
  if (in["backend"].is<int>()) {
    llmSetBackend(in["backend"].as<int>());
  }
  if (!in["model"].isNull()) {
    llmSetModel(in["model"].as<String>());
  }
  sendJson(200, llmGetConfigJson());
}

void handleModels() {
  sendJson(200, llmGetModelsJson());
}

// 实时内存状态 + 最近一次请求的采样序列（给内存监控页 /mem.html 用）
void handleMem() {
  sendJson(200, memGetJson());
}
