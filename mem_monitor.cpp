#include "mem_monitor.h"

#include <WiFi.h>

#include "memprobe.h"

static WiFiServer g_memServer(81);
static bool g_started = false;

// 读 HTTP 请求行（GET /path HTTP/1.1），返回路径；超时返回空
static String readRequestPath(WiFiClient &c) {
  String line;
  unsigned long t0 = millis();
  while (millis() - t0 < 3000) {
    int b = c.read();
    if (b < 0) {
      break;
    }
    if (b == '\n') {
      break;
    }
    if (b != '\r') {
      line += (char)b;
    }
  }
  int sp1 = line.indexOf(' ');
  if (sp1 < 0) {
    return "";
  }
  int sp2 = line.indexOf(' ', sp1 + 1);
  return line.substring(sp1 + 1, sp2 < 0 ? line.length() : sp2);
}

static void sendResponse(WiFiClient &c, const char *body) {
  String head = "HTTP/1.1 200 OK\r\n";
  head += "Content-Type: application/json\r\n";
  head += "Access-Control-Allow-Origin: *\r\n";
  head += "Cache-Control: no-store\r\n";
  head += "Connection: close\r\n";
  head += "Content-Length: ";
  head += String((unsigned)strlen(body));
  head += "\r\n\r\n";
  c.write((const uint8_t *)head.c_str(), head.length());
  c.write((const uint8_t *)body, strlen(body));
}

// 监控任务：只处理 /api/mem（不碰文件系统，避免与主任务并发读 LittleFS）
static void memServerTask(void *) {
  for (;;) {
    WiFiClient c = g_memServer.available();
    if (!c) {
      vTaskDelay(20 / portTICK_PERIOD_MS);
      continue;
    }
    String path = readRequestPath(c);
    if (path == "/api/mem") {
      String body = memGetJson();
      sendResponse(c, body.c_str());
    } else {
      const char *r = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      c.write((const uint8_t *)r, strlen(r));
    }
    c.stop();
  }
}

void memServerBegin(void) {
  if (g_started) {
    return;
  }
  g_started = true;
  g_memServer.begin();
  xTaskCreate(memServerTask, "memmon", 8192, nullptr, 1, nullptr);
  Serial.println("[mem] monitor http://<ip>:81/api/mem");
}
