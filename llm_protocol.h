#pragma once

#include <Arduino.h>

// 各后端协议差异：流式行 → 增量文本（取到返回 true）、鉴权头值。
// 纯解析逻辑，与传输（建连 / 读写 / 收尾）解耦，便于单独修改与测试。
bool ollamaParseLine(const String &line, String &delta);
bool openaiParseLine(const String &line, String &delta);
String openaiAuthValue();
