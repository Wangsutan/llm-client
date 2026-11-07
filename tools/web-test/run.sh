#!/bin/bash
# 前端 headless 回归测试入口：主聊天页 + 内存监控页
# 用法：./tools/web-test/run.sh
#   环境变量：
#     BASE_URL     板子地址（默认 http://192.168.1.50）
#     CHROME_PATH  Chromium 可执行文件（默认自动查找 playwright 缓存）
#     SKIP_IF_DOWN 设 0 时板子不可达按失败处理（默认跳过并返回 0）
# 退出码：0 通过（或板子不在线且允许跳过）；1 有断言失败；2 环境问题（无 node / 无 Chromium）
set -euo pipefail
cd "$(dirname "$0")"

if ! command -v node >/dev/null 2>&1; then
  echo "需要 Node.js（建议 v20+）" >&2
  exit 2
fi

if [ ! -d node_modules ]; then
  echo "== 安装依赖（puppeteer-core）=="
  npm install --no-audit --no-fund
fi

node runner.js "$@"
