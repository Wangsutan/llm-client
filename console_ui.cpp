#include <Arduino.h>

#include "console_ui.h"

// 串口输入行的上限。没有它的话，串口噪声或一次超长粘贴（始终不出现 \n）会把这个
// 全局 String 一直喂到把堆吃光。
static const size_t UI_LINE_MAX = 512;

static String s_line;
static bool s_overflow = false;

void uiBegin() {
  s_line = "";
  s_overflow = false;
}

void uiPrompt() {
  Serial.println();
  Serial.println("请输入问题（回车发送）:");
}

bool uiReadLine(String& line) {
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (s_overflow) {
        // 这一行超长被丢弃：清状态、给个提示，但不把内容送进模型
        s_overflow = false;
        s_line = "";
        Serial.println();
        Serial.printf("[ui] 输入超过 %u 字符，已丢弃\n", (unsigned)UI_LINE_MAX);
        return false;
      }
      String q = s_line;
      s_line = "";
      q.trim();
      line = q;
      return true;
    }
    if (s_line.length() < UI_LINE_MAX) {
      s_line += (char)c;
      Serial.print((char)c);  // 回显
    } else if (!s_overflow) {
      s_overflow = true;  // 超限后不再累积、不再回显，等换行符把它清掉
      Serial.print("...");
    }
  }
  return false;
}
