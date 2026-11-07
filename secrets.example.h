#pragma once

// ============================================================
// 配置模板（可提交到版本库；真实配置请复制为 secrets.h 后填写）
//
//   cp secrets.example.h secrets.h
//
// 然后编辑 secrets.h 填入真实值（Wi-Fi 密码、API key 等）。
// secrets.h 已被 .gitignore 忽略，不会进入版本库。
// ============================================================

// Wi-Fi
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASS "your-wifi-password"

// 交互方式：0=仅串口，1=仅网页，2=串口+网页（默认）
#define UI_MODE 2

// 多轮上下文：历史字符预算（按模型匹配；测试可先调很小以快速触发摘要压缩）
// 改大后重新编译烧录即可生效
#define HISTORY_BUDGET_DEFAULT 12000
#define HISTORY_BUDGET_20B 40000
#define HISTORY_BUDGET_DEEPSEEK 25000
#define HISTORY_BUDGET_QWEN3 15000

// 摘要压缩：单次摘要输入的字符上限
#define SUMMARY_INPUT_MAX 10000

// 大模型后端选择：0 = Ollama（局域网 /api/chat），1 = OpenAI 兼容（DeepSeek 等 /v1/chat/completions）
#define LLM_BACKEND 0

// Ollama 后端配置
#define OLLAMA_BASE_URL "http://your-ollama-host:11434"
#define OLLAMA_MODEL "your-ollama-model"

// OpenAI 兼容后端配置（DeepSeek）
#define OPENAI_BASE_URL "https://api.deepseek.com/v1"
#define OPENAI_API_KEY "sk-your-deepseek-api-key"
#define OPENAI_MODEL "deepseek-chat"
