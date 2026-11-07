#!/bin/bash
# 构建 + 烧录：固件 + LittleFS 网页文件系统
#
# 依赖：arduino-cli（在 PATH 里，或用 ACLI=/path/to/arduino-cli 指定）、
#       esp32 core（自带 mklittlefs / esptool）、项目根目录下已填好的 secrets.h
#
# 用法：
#   ./tools/build.sh                    # 编译 + 烧录固件 + 烧录网页文件系统
#   PORT=/dev/ttyACM0 ./tools/build.sh  # 换串口
#   ACLI=~/bin/arduino-cli ./tools/build.sh
set -euo pipefail
cd "$(dirname "$0")/.."

ACLI="${ACLI:-$(command -v arduino-cli || true)}"
PORT="${PORT:-/dev/ttyUSB0}"
FQBN="esp32:esp32:esp32:PartitionScheme=default"
BUILD_DIR="/tmp/llm-client-build"

if [ -z "$ACLI" ] || [ ! -x "$ACLI" ]; then
  echo "错误：找不到 arduino-cli，请安装后用 ACLI=/path/to/arduino-cli 指定" >&2
  exit 1
fi
if [ ! -f secrets.h ]; then
  echo "错误：缺少 secrets.h，请先 cp secrets.example.h secrets.h 并填写配置" >&2
  exit 1
fi

CORE="$HOME/.arduino15/packages/esp32"
MKFS="$(find "$CORE/tools/mklittlefs" -name mklittlefs -type f 2>/dev/null | head -1 || true)"
ESPTOOL="$(find "$CORE/tools/esptool_py" -name esptool -type f 2>/dev/null | head -1 || true)"
if [ -z "$MKFS" ] || [ -z "$ESPTOOL" ]; then
  echo "错误：找不到 esp32 core 的 mklittlefs / esptool，请先 arduino-cli core install esp32:esp32" >&2
  exit 1
fi
FS_SIZE="0x160000"    # default 分区 spiffs 大小（1408K）
FS_OFFSET="0x290000"  # spiffs 分区偏移

echo "== 1/4 编译固件 =="
"$ACLI" compile --fqbn "$FQBN" --build-path "$BUILD_DIR" .

echo "== 2/4 烧录固件 =="
"$ACLI" upload -p "$PORT" --fqbn "$FQBN" --input-dir "$BUILD_DIR" .

echo "== 3/4 打包 LittleFS 网页文件系统（web/ 目录）=="
"$MKFS" -c web -b 4096 -p 256 -s "$FS_SIZE" /tmp/llm-client-littlefs.bin

echo "== 4/4 烧录文件系统 =="
"$ESPTOOL" --port "$PORT" write-flash "$FS_OFFSET" /tmp/llm-client-littlefs.bin

echo "== 完成 =="
