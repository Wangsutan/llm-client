#pragma once

#include <ArduinoJson.h>
#include <WebServer.h>

// 板载 HTTP 服务内部共享声明：只在 web_*.cpp 之间使用，不对外暴露。
// 对外接口仍只有 web_server.h 的 webBegin() / webHandle()。

// 唯一的 WebServer 实例（定义在 web_server.cpp）
WebServer& webServer();

// 统一 JSON 响应（带 Cache-Control: no-store）；codeName 非空时附带机器可读错误码
void sendJson(int code, const String& body);
void sendError(int code, const char* msg, const char* codeName = nullptr);

// 静态文件服务（web_static.cpp）
void handleRoot();
void handleMarked();
void handleHighlight();
void handleCss();
void handleApp();
void handleMemPage();

// 配置接口（web_api.cpp）
void handleConfigGet();
void handleConfigPost();
void handleModels();
void handleMem();

// 聊天 / 摘要接口（web_chat.cpp）
void handleUpload();
void handleChat();
void handleSummarize();
