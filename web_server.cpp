#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>

#include "mem_monitor.h"

#include "web_internal.h"
#include "web_server.h"

// 唯一的 WebServer 实例。各 handler 都通过 webServer() 取用。
static WebServer s_server(80);

WebServer& webServer() {
  return s_server;
}

// ---------- 响应小工具：统一 no-store，省掉满屏重复的 sendHeader ----------

void sendJson(int code, const String& body) {
  s_server.sendHeader("Cache-Control", "no-store");
  s_server.send(code, "application/json", body);
}

// codeName 非空时额外给前端一个机器可读的错误码（如 payload_too_large）
void sendError(int code, const char* msg, const char* codeName) {
  JsonDocument e;
  e["error"] = msg;
  if (codeName) {
    e["code"] = codeName;
  }
  String s;
  serializeJson(e, s);
  sendJson(code, s);
}

// ---------- 生命周期与路由表 ----------

void webBegin() {
  s_server.on("/", handleRoot);
  s_server.on("/style.css", handleCss);
  s_server.on("/app.js", handleApp);
  s_server.on("/mem.html", handleMemPage);
  memServerBegin();  // 独立 81 端口：对话阻塞主循环时仍可读内存数据
  s_server.on("/marked.min.js", handleMarked);
  s_server.on("/highlight.min.js", handleHighlight);
  s_server.on("/api/chat", HTTP_POST, handleChat, handleUpload);
  s_server.on("/api/summarize", HTTP_POST, handleSummarize, handleUpload);
  s_server.on("/api/config", HTTP_GET, handleConfigGet);
  s_server.on("/api/config", HTTP_POST, handleConfigPost);
  s_server.on("/api/models", HTTP_GET, handleModels);
  s_server.on("/api/mem", HTTP_GET, handleMem);
  s_server.begin();
  Serial.printf("[web] http://%s/\n", WiFi.localIP().toString().c_str());
}

void webHandle() {
  s_server.handleClient();
}
