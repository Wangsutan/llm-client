#pragma once
#include <Arduino.h>

// 请求期内存探针。
//
// 为什么不用 ESP.getMinFreeHeap()：它是 since-boot 单调不增值，第二次请求
// 读到的还是第一次的谷底，无法归因到单轮。这里在请求开始时把水位重置，
// 由 memSample 逐点更新，得到的是"本轮相对起点"的净占用与峰值。
//
// 注意 stackleft（uxTaskGetStackHighWaterMark）仍是 since-boot 的，
// API 无法复位；只有当本轮比历史更深时才会下降，读它要记住这一点。

// 一次请求内最多记录的采样点数（memBegin 清空，memSample 追加）
constexpr size_t MEM_SAMPLE_MAX = 16;

struct MemSamplePoint {
  char name[16];       // 阶段名（截断保存）
  uint32_t ms;         // 相对 memBegin 的毫秒
  uint32_t free;       // 当时空闲堆
  uint32_t maxAlloc;   // 当时最大连续块
  uint32_t stackLeft;  // 当时栈剩余
};

struct MemProbeState {
  uint32_t base;     // 本轮起点空闲堆
  uint32_t minFree;  // 本轮最低空闲堆
  uint32_t total;    // 堆总量（getHeapSize，一次读取）
  bool active;
  uint32_t t0;   // memBegin 的 millis
  size_t count;  // 已记录采样点数
  MemSamplePoint points[MEM_SAMPLE_MAX];
};

extern MemProbeState g_mem;

void memBegin(const char *tag);
void memSample(const char *tag);
void memEnd(void);

// 序列化实时内存状态 + 最近一次请求的采样序列（给 /api/mem 用）
String memGetJson(void);
