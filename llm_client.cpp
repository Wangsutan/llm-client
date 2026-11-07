#include "llm_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>

#include "llm_http.h"
#include "secrets.h"

static Preferences gPrefs;
static int gBackend = LLM_BACKEND;
static String gModelOllama = OLLAMA_MODEL;
static String gModelDeepseek = OPENAI_MODEL;
static bool gInitialized = false;

void llmInit() {
  if (gInitialized) {
    return;
  }
  gPrefs.begin("llmcfg", false);
  gBackend = gPrefs.getInt("backend", LLM_BACKEND);
  gModelOllama = gPrefs.getString("ollama_model", OLLAMA_MODEL);
  gModelDeepseek = gPrefs.getString("deepseek_model", OPENAI_MODEL);
  gInitialized = true;
  Serial.printf("[cfg] backend=%d ollama=%s deepseek=%s\n", gBackend, gModelOllama.c_str(),
                gModelDeepseek.c_str());
}

int llmGetBackend() {
  return gBackend;
}

String llmGetModel() {
  return gBackend == 1 ? gModelDeepseek : gModelOllama;
}

bool llmSetBackend(int backend) {
  if (backend != 0 && backend != 1) {
    return false;
  }
  // 值没变就不写 NVS：每次 putInt 都要擦写一个扇区（几十毫秒、还会禁用 flash cache）
  if (backend == gBackend) {
    return true;
  }
  gBackend = backend;
  if (!gPrefs.putInt("backend", backend)) {
    Serial.println("[cfg] nvs write failed: backend");
  }
  Serial.printf("[cfg] backend -> %d\n", backend);
  return true;
}

bool llmSetModel(const String& model) {
  if (model.isEmpty()) {
    return false;
  }
  if (gBackend == 1) {
    if (model == gModelDeepseek) {
      return true;
    }
    gModelDeepseek = model;
    if (!gPrefs.putString("deepseek_model", model)) {
      Serial.println("[cfg] nvs write failed: deepseek_model");
    }
  } else {
    if (model == gModelOllama) {
      return true;
    }
    gModelOllama = model;
    if (!gPrefs.putString("ollama_model", model)) {
      Serial.println("[cfg] nvs write failed: ollama_model");
    }
  }
  Serial.printf("[cfg] model -> %s\n", model.c_str());
  return true;
}

int llmGetHistoryBudget() {
  if (gBackend == 1) {
    return HISTORY_BUDGET_DEEPSEEK;
  }
  String model = gModelOllama;
  if (model.indexOf("gpt-oss") >= 0 || model.indexOf("20b") >= 0) {
    return HISTORY_BUDGET_20B;
  }
  if (model.indexOf("qwen3") >= 0) {
    return HISTORY_BUDGET_QWEN3;
  }
  return HISTORY_BUDGET_DEFAULT;
}

String llmGetConfigJson() {
  JsonDocument doc;
  doc["backend"] = gBackend;
  doc["model"] = llmGetModel();
  doc["ollama_model"] = gModelOllama;
  doc["deepseek_model"] = gModelDeepseek;
  doc["history_budget"] = llmGetHistoryBudget();
  doc["summary_input_max"] = SUMMARY_INPUT_MAX;
  String s;
  serializeJson(doc, s);
  return s;
}

String llmGetModelsJson() {
  JsonDocument out;
  JsonArray ollama = out["ollama"].to<JsonArray>();
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(10000);
  if (http.begin(String(OLLAMA_BASE_URL) + "/api/tags")) {
    int code = http.GET();
    if (code == 200) {
      JsonDocument tags;
      if (!deserializeJson(tags, http.getString())) {
        for (JsonVariant v : tags["models"].as<JsonArray>()) {
          String name = v["name"].as<String>();
          if (name.length()) {
            ollama.add(name);
          }
        }
      }
    }
    http.end();
  }
  JsonArray ds = out["deepseek"].to<JsonArray>();
  ds.add("deepseek-chat");
  ds.add("deepseek-reasoner");
  out["current_backend"] = gBackend;
  out["current_model"] = llmGetModel();
  String s;
  serializeJson(out, s);
  return s;
}

bool llmStream(const String& prompt, JsonArray history, String& fullAnswer,
               const std::function<void(const String&)>& onDelta,
               const std::function<bool()>& shouldStop, size_t* totalBytes) {
  return backendStream(llmBackendSpec(gBackend), llmGetModel(), prompt, history, fullAnswer,
                       onDelta, shouldStop, totalBytes);
}
