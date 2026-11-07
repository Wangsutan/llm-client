#pragma once

// 独立端口的轻量内存监控接口（81）。
// 板子主循环的 WebServer 是同步单线程：聊天流式期间 handleClient 被占住，
// 80 端口的 /api/mem 会排队；这里用独立 FreeRTOS 任务 + 独立 WiFiServer，
// 保证对话进行中仍能实时读到内存数据。
void memServerBegin(void);
