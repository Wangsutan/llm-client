#!/bin/bash
# 渲染 docs/ 下所有 Graphviz 源（.dot）为同目录的 svg + png
#
# 依赖：graphviz（dot）、Source Han Sans SC 或 Noto Sans CJK SC 字体
#
# 注意（重要）：Graphviz 版本会改变布局与产物字节数。
#   - 仓库中现有 svg/png 由 graphviz 2.42.4 在 dpi=96 下渲染
#   - 用 16.x 重新渲染，尺寸与字节数都会不同（布局算法升级），
#     这不是图画错了，是版本差异
#   - 想复现已提交的产物：在装有 2.42.4 的机器上、DPI=96 渲染
#   本脚本 DPI 默认 96，以尽量贴近原始产物。
#
# 用法：tools/render-graphviz.sh [目录]   默认扫 docs/
set -euo pipefail
cd "$(dirname "$0")/.."          # 项目根

DPI="${DPI:-96}"
SCAN="${1:-docs}"

if ! command -v dot >/dev/null; then
    echo "错误：找不到 dot，请先安装 graphviz" >&2
    exit 1
fi

GV_VER="$(dot -V 2>&1 | grep -oE "[0-9]+\.[0-9]+\.[0-9]+" || echo unknown)"
echo "== 渲染 Graphviz（dot $GV_VER, dpi=$DPI, 扫描 $SCAN/）=="
case "$GV_VER" in
    2.42.*) ;;
    *) echo "   提示：已提交产物由 2.42.4 生成，当前版本布局可能不同（非错误）" ;;
esac

mapfile -t files < <(find "$SCAN" -name "*.dot" -type f | sort)
if [ ${#files[@]} -eq 0 ]; then
    echo "$SCAN/ 下没有 .dot 文件" >&2
    exit 0
fi

for f in "${files[@]}"; do
    base="${f%.dot}"
    printf "  %-40s " "${f#"$SCAN"/}"
    dot -Tsvg "$f" -o "$base.svg"
    dot -Tpng -Gdpi="$DPI" "$f" -o "$base.png"
    printf "svg %8s  png %8s\n" \
        "$(stat -c%s "$base.svg")" "$(stat -c%s "$base.png")"
done

echo "== 完成，共 ${#files[@]} 张 =="
