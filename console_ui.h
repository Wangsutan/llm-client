#pragma once

#include <Arduino.h>

// 串口交互界面：提示语与非阻塞行读取
void uiBegin();
void uiPrompt();
bool uiReadLine(String& line);  // 每帧调用；凑齐一行(回车)返回 true，已做回显与 trim
