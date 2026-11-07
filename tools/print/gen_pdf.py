#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把 llm-client 的 C++ 源码排版成一份可彩色打印的 HTML（再由 Chromium 打 PDF）。

- 浅色高亮（省墨、彩打仍分得出语法类别）
- 行号、文件头（序号/路径/行数）、连续排版（不强行每个文件另起一页）
- secrets.h 按安全要求排除，页内留说明
"""
import os
import unicodedata
from pygments import highlight
from pygments.lexers import CppLexer
from pygments.formatters import HtmlFormatter
from pygments.style import Style
from pygments.token import (
    Comment, Keyword, Name, Number, Operator, Punctuation, String, Token,
)

SRC = os.path.dirname(os.path.abspath(__file__))
COMMIT = "067421c"
DATE = "2026-11-01"

# 逻辑顺序：入口 → 配置/分发 → 后端 → 请求体 → 网页 → 流式读写 → 探针 → 串口 → Wi-Fi → 限制
ORDER = [
    "llm-client.ino",
    "llm_client.h", "llm_client.cpp",
    "llm_http.h", "llm_http.cpp",
    "payload.h", "payload.cpp",
    "web_server.h", "web_server.cpp",
    "http_stream.h", "http_stream.cpp",
    "memprobe.h", "memprobe.cpp",
    "console_ui.h", "console_ui.cpp",
    "wifi_manager.h", "wifi_manager.cpp",
    "llm_limits.h",
]
EXCLUDED = ["secrets.h"]


class PrintLight(Style):
    """浅色、可彩打：注释/关键字/字符串/数字四类能分开，墨量克制。"""

    background_color = "#ffffff"
    default_style = ""
    styles = {
        Token: "#1f2328",
        Comment: "italic #5f6f80",
        Comment.Preproc: "#7a3e9d",
        Comment.PreprocFile: "#7a3e9d",
        Keyword: "bold #0b4f9c",
        Keyword.Type: "#0b6b9c",
        Name.Builtin: "#8a3b00",
        Name.Function: "#8250df",
        Name.Class: "#0b6b9c",
        Name.Namespace: "#0b4f9c",
        String: "#17692b",
        String.Char: "#17692b",
        String.Escape: "#a04100",
        Number: "#8a3b00",
        Operator: "#1f2328",
        Punctuation: "#1f2328",
    }


def cjk_width(s):
    return sum(2 if unicodedata.east_asian_width(c) in ("W", "F") else 1 for c in s)


files = [f for f in ORDER if os.path.exists(os.path.join(SRC, f))]
rows = []
for f in files:
    lines = open(os.path.join(SRC, f), encoding="utf-8").read().splitlines()
    rows.append((f, len(lines), max((cjk_width(l) for l in lines), default=0)))

total_lines = sum(r[1] for r in rows)

# 自己按行渲染：pygments 的 linenos 列宽度不足时会把每个行号挤成两行，整份被撑大一倍
TOKEN_COLOR = [
    (lambda t: t in Comment, "#5f6f80", "italic"),
    (lambda t: t in Comment.Preproc, "#7a3e9d", "normal"),
    (lambda t: t in String, "#17692b", "normal"),
    (lambda t: t in Number, "#8a3b00", "normal"),
    (lambda t: t in Keyword.Type, "#0b6b9c", "bold"),
    (lambda t: t in Keyword, "#0b4f9c", "bold"),
    (lambda t: t in Name.Function, "#8250df", "normal"),
    (lambda t: t in Name.Class, "#0b6b9c", "normal"),
    (lambda t: t in Name.Builtin, "#8a3b00", "normal"),
    (lambda t: t in Operator, "#1f2328", "normal"),
    (lambda t: t in Punctuation, "#1f2328", "normal"),
]


def token_style(ttype):
    for test, color, weight in TOKEN_COLOR:
        if test(ttype):
            return color, weight
    return "#1f2328", "normal"


def render_lines(code):
    """按行输出 HTML，行号与代码行一一对应（token 跨行时按行切开）。"""
    import html as _html
    from pygments import lex
    from pygments.lexers import CppLexer as _L

    lines = [[]]
    for ttype, value in lex(code, _L()):
        color, weight = token_style(ttype)
        parts = value.split("\n")
        for idx, part in enumerate(parts):
            if idx > 0:
                lines.append([])
            if part:
                style = f"color:{color}"
                if weight != "normal":
                    style += f";font-weight:{weight}"
                lines[-1].append(f'<span style="{style}">{_html.escape(part)}</span>')
    out = []
    for i, seg in enumerate(lines, 1):
        out.append(f'<div class="ln"><span class="n">{i}</span>'
                   + "".join(seg) + "</div>")
    return "\n".join(out)

parts = []

TREE = """
llm-client/
├── llm-client.ino          入口：UI_MODE 决定串口/网页，setup/loop
├── llm_client.{h,cpp}      运行时配置（NVS）、后端分发、模型列表
├── llm_http.{h,cpp}        后端描述符 + 流式请求公共流程（明码/TLS）
├── payload.{h,cpp}         请求体管线：upload 回调、gzip 解压、静态工作区
├── web_server.{h,cpp}      HTTP 路由、聊天/摘要/配置接口
├── http_stream.{h,cpp}     URL 解析、chunked 发送、响应头、按行流式读取
├── memprobe.{h,cpp}        请求期内存探针（[mem] 串口打点）
├── console_ui.{h,cpp}      串口输入行读取（上限 512 字符、回显）
├── wifi_manager.{h,cpp}    Wi-Fi 连接与断线重连
├── llm_limits.h            长度上限、静态工作区大小
└── secrets.h               （未收录：凭据）
"""

parts.append(f"""<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8">
<title>llm-client C++ 源码</title>
<style>
/* 页面边距交给 Chromium 的 page.pdf margin 选项；这里不能再设 @page margin，
   否则会覆盖掉、正文顶到 y=0 与页眉重叠 */
* {{ box-sizing: border-box; }}
body {{ margin: 0; font-family: 'DejaVu Sans Mono','Source Han Sans CN','Noto Sans CJK SC',monospace;
        font-size: 9pt; line-height: 1.25; color: #1f2328; }}
.cover {{ padding: 14mm 16mm 0 16mm; break-after: page; }}
h1 {{ font-family: 'Source Han Sans CN',sans-serif; font-size: 20pt; margin: 0 0 3mm; }}
.sub {{ font-size: 10pt; color: #444; margin-bottom: 5mm; font-family: 'Source Han Sans CN',sans-serif; }}
.meta {{ font-size: 8.6pt; line-height: 1.5; color: #333; margin-bottom: 5mm; font-family: 'Source Han Sans CN',sans-serif; }}
.meta b {{ color: #000; }}
h2 {{ font-family: 'Source Han Sans CN',sans-serif; font-size: 11.5pt; margin: 0 0 2mm; }}
table.tree {{ width: 100%; border-collapse: collapse; font-size: 8.2pt; break-inside: avoid; }}
table.tree td {{ padding: 0.25mm 2mm 0.25mm 0; vertical-align: top; }}
table.tree td.n {{ color: #888; text-align: right; width: 12mm; }}
.treebox {{ background: #f7f8fa; border: 0.4pt solid #dfe3e8; border-radius: 2mm; padding: 2.5mm 4mm;
            margin-bottom: 5mm; break-inside: avoid; }}
.note {{ background: #fff8e6; border: 0.4pt solid #e8d8a8; border-radius: 2mm; padding: 2mm 3mm;
         font-family: 'Source Han Sans CN',sans-serif; font-size: 8.4pt; color: #6a4a10; margin-bottom: 4mm; }}
.filehead {{ font-family: 'Source Han Sans CN',sans-serif; font-size: 10pt; font-weight: bold;
             border-bottom: 0.6pt solid #c9ced6; padding: 1.5mm 0 1mm; margin: 5mm 0 1.5mm;
             break-after: avoid; }}
.filehead .idx {{ color: #888; font-weight: normal; font-size: 8.6pt; margin-right: 2.5mm; }}
.filehead .ln {{ color: #666; font-weight: normal; font-size: 8.6pt; float: right; }}
/* 每行一个 block，行号做成 9mm 的内联块 + 整行悬垂缩进：
   行号与代码在同一行里，分页只会在行间断开；折行的续行落在代码列下方。
   （flex / absolute 的行号在 Chromium 打印时会被甩到下一页顶部，别用） */
.code {{ margin: 0; }}
.code .ln {{ display: block; padding-left: 12mm; text-indent: -12mm;
             white-space: pre-wrap; word-break: break-word; break-inside: avoid; }}
.code .n {{ display: inline-block; width: 10.5mm; margin-right: 1.5mm; text-align: right;
            color: #9aa4b0; font-size: 8.2pt; }}
.excluded {{ font-family: 'Source Han Sans CN',sans-serif; font-size: 8.6pt; color: #8a2a2a;
             background: #fbeaea; border: 0.4pt solid #e0bcbc; border-radius: 2mm;
             padding: 3mm 4mm; margin: 4mm 0; break-inside: avoid; }}
</style></head><body>
<div class="cover">
  <h1>llm-client · C++ 源码</h1>
  <div class="sub">ESP32 局域网大模型客户端 —— 供彩色打印 / 人工审阅</div>
  <div class="meta">
    <b>仓库</b>：<span>llm-client</span> ＠ <b>{COMMIT}</b><br>
    <b>日期</b>：{DATE}　<b>范围</b>：{len(files)} 个文件 / {total_lines} 行（不含 web/ 与 tools/）<br>
    <b>排印</b>：A4 · 9 pt 等宽 · 行号连续 · 浅色语法高亮（彩打；黑白打同样可读）<br>
    <b>生成</b>：pygments 2.17 高亮 + Chromium 打印
  </div>
  <div class="note">
    <b>secrets.h 未收录</b>：该文件含 Wi-Fi 账号密码与 API Key，按安全要求不打印。
    全文其余 {len(files)} 个文件完整收录。
  </div>
  <h2>文件清单</h2>
  <div class="treebox">
  <table class="tree">
    <tr><td class="n">#</td><td><b>文件</b></td><td class="n"><b>行数</b></td><td class="n"><b>最宽列</b></td></tr>
""")

for i, (f, n, w) in enumerate(rows, 1):
    parts.append(f'    <tr><td class="n">{i}</td><td>{f}</td><td class="n">{n}</td><td class="n">{w}</td></tr>\n')

parts.append(f"""    <tr><td class="n">—</td><td><i>合计</i></td><td class="n"><b>{total_lines}</b></td><td class="n"></td></tr>
  </table>
  </div>
  <h2>目录结构</h2>
  <div class="treebox" style="font-size:8pt; white-space:pre; line-height:1.42">
{TREE}
  </div>
</div>
""")

for i, (f, n, w) in enumerate(rows, 1):
    code = open(os.path.join(SRC, f), encoding="utf-8").read()
    body = render_lines(code)
    parts.append(f'<div class="filehead"><span class="idx">{i}/{len(files)}</span>{f}'
                 f'<span class="ln">{n} 行</span></div>\n')
    parts.append('<div class="code">' + body + "</div>\n")

parts.append("</body></html>\n")
out = os.path.join(SRC, "print.html")
open(out, "w", encoding="utf-8").write("".join(parts))
print("wrote", out, os.path.getsize(out), "bytes,", len(files), "files,", total_lines, "lines")
