// llm-client —— ESP32 局域网大模型客户端
//
// 交互方式由 secrets.h 的 UI_MODE 决定：0=仅串口，1=仅网页，2=串口+网页（默认）。
// 编译：arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=default
// 依赖：esp32:esp32 core + ArduinoJson
//
// 配置项（Wi-Fi 凭据、交互方式、后端/地址/模型名）全部在 secrets.h 中定义，
// 本文件不写死任何地址/模型（secrets.h 不入库，chmod 600）。

#include <WiFi.h>

#include "console_ui.h"
#include "llm_client.h"
#include "secrets.h"
#include "web_server.h"
#include "wifi_manager.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.printf("[llm-client] boot OK (UI_MODE=%d)\n", UI_MODE);
  uiBegin();
  llmInit();
  if (!wifiConnect(25000)) {
    Serial.println("[llm-client] abort: wifi not connected");
    return;
  }
#if UI_MODE != 0
  webBegin();
#endif
#if UI_MODE != 1
  uiPrompt();
#endif
}

void loop() {
#if UI_MODE != 0
  webHandle();
#endif
#if UI_MODE != 1
  String line;
  if (!uiReadLine(line)) return;
  if (line.length() == 0) {
    uiPrompt();
    return;
  }
  Serial.printf("[user] %s\n", line.c_str());
  if (!wifiEnsure()) {
    Serial.println("[http] no wifi, request skipped");
  } else {
    // 和网页走同一条流式管线：边收边打印到串口，板上只留前 LLM_KEEP_MAX 字节前缀，
    // 不再用非流式的 getString() 把整份响应读进堆
    String answer;
    bool ok =
        llmStream(line, JsonArray(), answer, [](const String &delta) { Serial.print(delta); });
    if (!ok) {
      Serial.println("[http] request failed");
    }
  }
  uiPrompt();
#elif UI_MODE == 1
  String line;
  if (uiReadLine(line)) {
    Serial.printf("[serial] 当前为仅网页模式，请用浏览器访问 http://%s/\n",
                  WiFi.localIP().toString().c_str());
  }
#endif
}
