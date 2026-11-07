#pragma once

// Wi-Fi 连接与断线重连
bool wifiConnect(unsigned long timeoutMs);  // 主动连接，成功打印并返回 true
bool wifiEnsure();                          // 已连接返回 true；断开则重连
