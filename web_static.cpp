#include <LittleFS.h>
#include <WebServer.h>

#include "web_internal.h"

// ---------- 静态文件（LittleFS） ----------

// 挂载 LittleFS（网页文件系统由 tools/build.sh 打包烧录）
static bool ensureFs() {
  static int state = 0;  // 0=未尝试，1=成功，-1=失败
  if (state == 0) {
    state = LittleFS.begin(true) ? 1 : -1;  // true: 挂载失败时格式化
    Serial.println(state == 1 ? "[fs] LittleFS mounted" : "[fs] LittleFS mount failed");
  }
  return state == 1;
}

static void serveFile(const char* path, const char* contentType) {
  WebServer& server = webServer();
  if (!ensureFs()) {
    server.sendHeader("Cache-Control", "no-store");
    server.send(500, "text/plain; charset=utf-8", "LittleFS mount failed");
    return;
  }
  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("[web] file not found: %s\n", path);
    server.sendHeader("Cache-Control", "no-store");
    server.send(404, "text/plain; charset=utf-8", "file not found");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, contentType, f);
  f.close();
}

void handleRoot() {
  serveFile("/index.html", "text/html; charset=utf-8");
}

void handleMarked() {
  serveFile("/marked.min.js", "application/javascript; charset=utf-8");
}

void handleHighlight() {
  serveFile("/highlight.min.js", "application/javascript; charset=utf-8");
}

void handleCss() {
  serveFile("/style.css", "text/css; charset=utf-8");
}

void handleApp() {
  serveFile("/app.js", "application/javascript; charset=utf-8");
}

void handleMemPage() {
  serveFile("/mem.html", "text/html; charset=utf-8");
}
