#pragma once

#include <Arduino.h>

#include <ArduinoJson.h>

#include <functional>

// 初始化：从 NVS 载入运行时配置（缺省值来自 secrets.h）
void llmInit();

// 运行时配置访问
int llmGetBackend();                    // 0=Ollama, 1=OpenAI 兼容（DeepSeek）
String llmGetModel();                   // 当前后端的模型名
bool llmSetBackend(int backend);        // 设置并保存到 NVS
bool llmSetModel(const String& model);  // 设置并保存到 NVS
String llmGetConfigJson();              // 当前配置 JSON
String llmGetModelsJson();  // 可用模型列表 JSON（Ollama tags + DeepSeek 固定列表）
int llmGetHistoryBudget();  // 当前模型的历史字符预算（来自 secrets.h）

// 注意：只保留流式入口。串口模式也用 llmStream（逐段打印），
// 不再有"一次拿完整回答"的非流式路径，避免整份响应进堆。

// 流式：逐段收到回答时调用 onDelta，完整回答写入 fullAnswer
// shouldStop: 返回 false 时立即停止读取上游（前端断开、回答超长时用）
// totalBytes: 回传回答总字节数；fullAnswer 只保留前 LLM_KEEP_MAX 字节
[[nodiscard]] bool llmStream(const String& prompt, JsonArray history, String& fullAnswer,
                             const std::function<void(const String&)>& onDelta,
                             const std::function<bool()>& shouldStop = nullptr,
                             size_t* totalBytes = nullptr);
