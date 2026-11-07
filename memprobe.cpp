#include "memprobe.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

MemProbeState g_mem = {};

// 记录一个采样点（供 /api/mem 的"最近一次请求"回放用）
static void recordPoint(const char *stage) {
  if (!g_mem.active || g_mem.count >= MEM_SAMPLE_MAX) {
    return;
  }
  MemSamplePoint &p = g_mem.points[g_mem.count++];
  strncpy(p.name, stage, sizeof(p.name) - 1);
  p.name[sizeof(p.name) - 1] = '\0';
  p.ms = (uint32_t)(millis() - g_mem.t0);
  p.free = ESP.getFreeHeap();
  p.maxAlloc = ESP.getMaxAllocHeap();
  p.stackLeft = uxTaskGetStackHighWaterMark(NULL);
}

// 统一输出格式，便于外部脚本解析：
//   [mem] <stage> free=<n> used=<n> peak=<n> blk=<n> stackleft=<n>
static void emit(const char *stage) {
  const uint32_t free = ESP.getFreeHeap();
  if (free < g_mem.minFree) {
    g_mem.minFree = free;
  }
  Serial.printf("[mem] %-12s free=%-6u used=%-6u peak=%-6u blk=%-6u stackleft=%u\n", stage,
                (unsigned)free, (unsigned)(g_mem.base > free ? g_mem.base - free : 0),
                (unsigned)(g_mem.base > g_mem.minFree ? g_mem.base - g_mem.minFree : 0),
                (unsigned)ESP.getMaxAllocHeap(), (unsigned)uxTaskGetStackHighWaterMark(NULL));
}

void memBegin(const char *tag) {
  static bool capsDumped = false;
  if (!capsDumped) {
    capsDumped = true;
    // getHeapSize() 只是 MALLOC_CAP_INTERNAL 的总和；map 里的 _heap_start.._heap_end
    // 只是末尾一段。这里把每个 cap 的实际总量一次性打出来，用于和链接期对账。
    Serial.printf("[mem] caps total: default=%u internal=%u dma=%u bit8=%u iram8=%u\n",
                  (unsigned)heap_caps_get_total_size(MALLOC_CAP_DEFAULT),
                  (unsigned)heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_total_size(MALLOC_CAP_DMA),
                  (unsigned)heap_caps_get_total_size(MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_total_size(MALLOC_CAP_IRAM_8BIT));
    heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);
  }
  g_mem.total = ESP.getHeapSize();
  g_mem.base = ESP.getFreeHeap();
  g_mem.minFree = g_mem.base;
  g_mem.active = true;
  g_mem.t0 = millis();
  g_mem.count = 0;
  recordPoint(tag);
  // 起点行带上堆总量与 chip 信息，用来和链接期 map 对账
  Serial.printf("[mem] ===== begin: %s | heapsize=%u free=%u blk=%u | chip=%s rev%d %dMHz\n", tag,
                (unsigned)g_mem.total, (unsigned)g_mem.base, (unsigned)ESP.getMaxAllocHeap(),
                ESP.getChipModel(), (int)ESP.getChipRevision(), (int)getCpuFrequencyMhz());
}

void memSample(const char *tag) {
  if (!g_mem.active) {
    return;
  }
  recordPoint(tag);
  emit(tag);
}

void memEnd(void) {
  if (!g_mem.active) {
    return;
  }
  recordPoint("end");
  emit("end");
  Serial.printf("[mem] ===== summary: peak_used=%u (%.1f%% of %u) lowest_free=%u\n",
                (unsigned)(g_mem.base > g_mem.minFree ? g_mem.base - g_mem.minFree : 0),
                g_mem.base > g_mem.minFree
                    ? 100.0 * (double)(g_mem.base - g_mem.minFree) / (double)g_mem.total
                    : 0.0,
                (unsigned)g_mem.total, (unsigned)g_mem.minFree);
  g_mem.active = false;
}

// 实时内存状态 + 最近一次请求的采样序列（JSON，给 /api/mem 用）
String memGetJson(void) {
  JsonDocument doc;
  doc["total"] = ESP.getHeapSize();
  doc["free"] = ESP.getFreeHeap();
  // 还没有跑过请求时 minFree 为 0（未初始），用当前 free 兜底，避免页面显示 0
  doc["minFree"] = g_mem.minFree ? g_mem.minFree : ESP.getFreeHeap();
  doc["maxAlloc"] = ESP.getMaxAllocHeap();
  doc["stackLeft"] = uxTaskGetStackHighWaterMark(NULL);
  doc["active"] = g_mem.active;
  JsonArray arr = doc["samples"].to<JsonArray>();
  for (size_t i = 0; i < g_mem.count; ++i) {
    const MemSamplePoint &p = g_mem.points[i];
    JsonObject o = arr.add<JsonObject>();
    o["name"] = p.name;
    o["ms"] = p.ms;
    o["free"] = p.free;
    o["maxAlloc"] = p.maxAlloc;
    o["stackLeft"] = p.stackLeft;
  }
  String s;
  serializeJson(doc, s);
  return s;
}
