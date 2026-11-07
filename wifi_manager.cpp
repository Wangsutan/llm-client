#include <WiFi.h>

#include "secrets.h"
#include "wifi_manager.h"

static bool connectOnce(unsigned long timeoutMs) {
  Serial.printf("[wifi] connecting to %s ...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
    delay(400);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] connected, ip=%s rssi=%d dBm\n", WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());
    return true;
  }
  Serial.println("[wifi] FAILED");
  return false;
}

bool wifiConnect(unsigned long timeoutMs) {
  return connectOnce(timeoutMs);
}

bool wifiEnsure() {
  if (WiFi.status() == WL_CONNECTED) return true;
  Serial.println("[wifi] connection lost, reconnecting...");
  return connectOnce(20000);
}
