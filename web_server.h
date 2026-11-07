#pragma once

// 板载 HTTP 服务器：提供聊天页面（GET /）与聊天接口（POST /api/chat）
void webBegin();   // 启动服务器（需 Wi-Fi 已连接）
void webHandle();  // 每帧调用，处理 HTTP 请求
