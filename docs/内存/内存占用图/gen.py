#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ESP32 内存/存储图 v6 —— 按聊天窗口实际显示宽度设计：W=1180, 最小字号 10.5。"""

W, H = 1300, 2110
FONT = "'Source Han Sans SC','Noto Sans CJK SC',sans-serif"
o = []
add = o.append
TX_ = "#141414"
AX_ = "#2b2b2b"

# 字号阶梯（最小 10.5，保证 1:1 显示可读）
F_TITLE, F_SUB = 23, 13
F_SEC, F_MAIN, F_BLK = 16, 12.5, 11.5
F_NOTE, F_TINY = 10.5, 10.5


def T(x, y, s, size=F_BLK, fill=TX_, anchor="start", weight="normal"):
    add(f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" fill="{fill}" text-anchor="{anchor}"'
        f' font-weight="{weight}">{s}</text>')


def R(x, y, w, h, fill, stroke="#555", sw=1.0, dash=None, rx=0):
    d = f' stroke-dasharray="{dash}"' if dash else ''
    add(f'<rect x="{x:.1f}" y="{y:.1f}" width="{max(w,0.6):.1f}" height="{max(h,0.6):.1f}"'
        f' fill="{fill}" stroke="{stroke}" stroke-width="{sw}" rx="{rx}"{d}/>')


def L(x1, y1, x2, y2, stroke="#555", sw=1, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ''
    add(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{stroke}"'
        f' stroke-width="{sw}"{d}/>')


C_DATA, C_OWN, C_OWN2 = "#8fa8c8", "#1f3d63", "#3d6491"
C_LIB, C_HEAP, C_USED, C_STACK = "#7e93ab", "#f9eccd", "#d9a441", "#c9793a"
C_CODE, C_FLASH, C_FS, C_CD, C_ROM = "#c3d0e0", "#efe4cb", "#d9e8c8", "#e8c6c6", "#b9b3a5"

add(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}"'
    f' font-family="{FONT}">')
add(f'<rect width="{W}" height="{H}" fill="#ffffff"/>')
add('<defs><pattern id="unused" width="7" height="7" patternUnits="userSpaceOnUse"'
    ' patternTransform="rotate(45)">'
    '<rect width="7" height="7" fill="#f6f5f0"/>'
    '<line x1="0" y1="0" x2="0" y2="7" stroke="#ddd8ca" stroke-width="2"/>'
    '</pattern></defs>')

T(W / 2, 42, "ESP32-D0WD-V3 内存与存储占用", F_TITLE, anchor="middle", weight="bold")
T(W / 2, 66, "llm-client @ 9b01ade　link map / 固件镜像 / 分区表 ＋ 2026-11-01 memprobe 实测",
  F_SUB, fill="#555", anchor="middle")

TOP, BOT = 120, 900
CW = 168                      # 柱宽

# ================= 1. Flash =================
FX = 96
fs = (BOT - TOP) / 0x400000
fy = lambda a: BOT - a * fs

R(FX, TOP, CW, BOT - TOP, "#fcfcfa", stroke=AX_, sw=1.5)
T(FX + CW / 2, TOP - 12, "Flash 4 MB", F_SEC, anchor="middle", weight="bold")

for lo, hi, col, st in [(0x000000, 0x001000, "#ffffff", "#ccc"),
                        (0x001000, 0x008000, C_FLASH, "#8a7a52"),
                        (0x008000, 0x009000, C_DATA, "#5a7a9a"),
                        (0x009000, 0x00E000, "#c4dcb4", "#5a7a52"),
                        (0x00E000, 0x010000, "#dcdcdc", "#999")]:
    R(FX, fy(hi), CW, fy(lo) - fy(hi), col, stroke=st, sw=0.8)

# 16 KB 引出
L(FX + CW + 2, fy(0x010000), FX + CW + 9, fy(0x010000), "#8a6a30", 1, dash="3,2")
L(FX + CW + 2, fy(0x000000), FX + CW + 9, fy(0x000000), "#8a6a30", 1, dash="3,2")
L(FX + CW + 9, fy(0x010000), FX + CW + 9, fy(0x000000), "#8a6a30", 1, dash="3,2")
T(FX + CW + 12, (fy(0x010000) + fy(0x000000)) / 2 - 10, "开头 16 KB", F_TINY, fill="#7a5a20")
T(FX + CW + 12, (fy(0x010000) + fy(0x000000)) / 2 + 4, "见下表 1–5", F_TINY, fill="#7a5a20")

A0O, A0S, BIN = 0x010000, 1280 * 1024, 1137696
a0t, a0b = fy(A0O + A0S), fy(A0O)
R(FX, a0t, CW, a0b - a0t, "#faf6ec", stroke="#8a7a52", sw=1.4)
T(FX + CW / 2, a0t + 22, "app0 固件", F_MAIN, anchor="middle", weight="bold")
T(FX + CW / 2, a0t + 39, "1,137,696 B", F_BLK, anchor="middle")
T(FX + CW / 2, a0t + 55, "占 1280 KB 的 86.8%", F_TINY, anchor="middle")

# 段条：放在文字下方（y 712..a0b），按字节比例分段
_seg_top, _seg_bot = 712.0, a0b
_seg_h = _seg_bot - _seg_top
_fy2 = lambda off: _seg_bot - off / A0S * _seg_h      # off=分区内偏移，整条=1280 KB
# 六段唯一数据源：自下而上（与固件内顺序一致）
SEGS = [("DROM .rodata", 166900, "#6fbf9a"),
        ("DRAM .data",   25416,  "#e39b4e"),
        ("IRAM 段①",      4268,  "#d9c33f"),
        ("IROM .text",  854248,  "#6b9bd4"),
        ("IRAM 段②",     86720,  "#a98cd8"),
        ("RTC_DATA",        32,  "#d96d6d")]
acc = 0x20
for _nm, n, col in SEGS:
    y_hi = _fy2(acc + n)          # 上边（y 小）
    y_lo = _fy2(acc)              # 下边（y 大）
    R(FX + 4, y_hi, CW - 8, y_lo - y_hi, col, stroke="none", sw=0)
    acc += n
R(FX + 4, _fy2(A0S), CW - 8, _fy2(acc) - _fy2(A0S), "url(#unused)", stroke="#c8c2b0", sw=0.8)
T(FX + CW / 2, (_fy2(acc) + _fy2(A0S)) / 2 + 4, "未用 173,024 B", F_TINY, fill="#4a4a4a", anchor="middle")



R(FX, fy(0x150000 + 1280 * 1024), CW, fy(0x150000) - fy(0x150000 + 1280 * 1024), "#fbfbfb", stroke="#ccc")
T(FX + CW / 2, (fy(0x150000) + fy(0x150000 + 1280 * 1024)) / 2 + 4, "app1 空槽", F_BLK,
  fill="#4a4a4a", anchor="middle")

LO, LS = 0x290000, 1408 * 1024
lz1, lz2 = fy(LO), fy(LO + LS)
R(FX, lz2, CW, lz1 - lz2, C_FS, stroke="#5a7a52", sw=1.2)
T(FX + CW / 2, lz2 + 56, "LittleFS", F_MAIN, anchor="middle", weight="bold")
T(FX + CW / 2, lz2 + 74, "已用 77,320 B", F_BLK, anchor="middle")
T(FX + CW / 2, lz2 + 91, "占 1408 KB 的 5.4%", F_TINY, anchor="middle")

R(FX, fy(0x3F0000 + 64 * 1024), CW, fy(0x3F0000) - fy(0x3F0000 + 64 * 1024), C_CD, stroke="#a05a5a")

for a in (0, 0x100000, 0x200000, 0x300000, 0x400000):
    L(FX - 6, fy(a), FX, fy(a), AX_, 1)
    T(FX - 9, fy(a) + 4, f"0x{a:06X}", F_TINY, fill="#444", anchor="end")

# ================= 2. DRAM =================
DX = 450
DLO, DHI = 0x3FFAE000, 0x40000000
ds = (BOT - TOP) / (DHI - DLO)
dy = lambda a: BOT - (a - DLO) * ds

R(DX, TOP, CW, BOT - TOP, "#fcfcfa", stroke=AX_, sw=1.5)
T(DX + CW / 2, TOP - 12, "DRAM 320 KB", F_SEC, anchor="middle", weight="bold")

LBL = DX + CW + 18            # 右侧标签列（与括号留 10px）

# ROM / BT
R(DX, dy(0x3FFBDB5C), CW, dy(DLO) - dy(0x3FFBDB5C), C_ROM, stroke="#8a8478")
_cy = (dy(0x3FFBDB5C) + dy(DLO)) / 2
L(DX + CW, _cy, LBL - 4, _cy, "#8a8478", 0.8)
T(LBL, _cy - 12, "ROM 数据+BT 保留", F_BLK, weight="bold")
T(LBL, _cy + 3, "64,348 B", F_TINY)

# .data
R(DX, dy(0x3FFC3EA5), CW, dy(0x3FFBDB5C) - dy(0x3FFC3EA5), C_DATA, stroke="#5a7a9a")
_cy = (dy(0x3FFC3EA5) + dy(0x3FFBDB5C)) / 2
L(DX + CW, _cy, LBL - 4, _cy, "#5a7a9a", 0.8)
T(LBL, _cy - 5, ".data 25,413 B", F_BLK, weight="bold")
T(LBL, _cy + 10, "已初始化全局", F_TINY)

# .bss 底
R(DX, dy(0x3FFD8818), CW, dy(0x3FFC3EA8) - dy(0x3FFD8818), "#e6ecf3", stroke="#7e93ab")

# g_work
g0, g1 = 0x3FFC3F70, 0x3FFC3F70 + 0xC001
R(DX, dy(g1), CW, dy(g0) - dy(g1), C_OWN, stroke="#12293f", sw=1.1)
T(DX + CW / 2, (dy(g0) + dy(g1)) / 2 - 5, "g_work 49,153 B", F_MAIN, fill="#ffffff",
  anchor="middle", weight="bold")
T(DX + CW / 2, (dy(g0) + dy(g1)) / 2 + 13, "48 KB 工作区", F_TINY, fill="#cfe0f2", anchor="middle")

# g_inflater
i0, i1 = 0x3FFCFF74, 0x3FFCFF74 + 0x2AF0
R(DX, dy(i1), CW, dy(i0) - dy(i1), C_OWN2, stroke="#26507c")
T(DX + CW / 2, (dy(i0) + dy(i1)) / 2 + 5, "g_inflater 10,992 B", F_TINY, fill="#ffffff", anchor="middle")

# .bss 总括：右侧大括号覆盖整段 0x3FFC3EA8..0x3FFD8818，标签放在括号上方空档
_BX = DX + CW + 8          # 柱外，随 DX/CW 走
_btop, _bbot = dy(0x3FFD8818), dy(0x3FFC3EA8)
L(_BX, _btop, _BX, _bbot, "#7e93ab", 1.2)               # 竖括号
L(_BX - 2, _btop, _BX + 2, _btop, "#7e93ab", 1.2)       # 上端点
L(_BX - 2, _bbot, _BX + 2, _bbot, "#7e93ab", 1.2)       # 下端点
T(LBL, 674, ".bss 84,336 B", F_BLK, fill="#2a3a4a", weight="bold")
T(LBL, 690, "未初始化全局", F_TINY, fill="#2a3a4a")

# 库/协议栈区：整段填色（原来只画 3 个符号、其余留白 → 看起来像空内存）
LIB0, LIB1 = 0x3FFD2A64, 0x3FFD8818      # 23,988 B
R(DX, dy(LIB1), CW, dy(LIB0) - dy(LIB1), C_LIB, stroke="#5a6e86", sw=1.0)
T(DX + CW / 2, (dy(LIB0) + dy(LIB1)) / 2 - 3, "库 / 协议栈", F_BLK, fill="#0e1c28", anchor="middle", weight="bold")
T(DX + CW / 2, (dy(LIB0) + dy(LIB1)) / 2 + 14, "23,988 B", F_TINY, fill="#16283a", anchor="middle")
# 标签对齐到库带垂直中心（523.6），引线止于括号左侧，避免跨线
_yc = (dy(LIB0) + dy(LIB1)) / 2
# 色块仅 55.7px 高 → 单列 4 行紧排，总高约 44px，居中于 _yc
L(DX + CW, _yc, _BX - 4, _yc, "#5a7e9e", 0.8)
T(LBL, _yc - 17, "库 / 协议栈 .bss", F_BLK, fill="#24445f", weight="bold")
T(LBL, _yc - 2,  "WiFi+PHY  11,465", F_TINY, fill="#24445f")
T(LBL, _yc + 11, "lwIP       4,701", F_TINY, fill="#24445f")
T(LBL, _yc + 24, "其余 7,029 · 填充 797", F_TINY, fill="#24445f")

# 堆
H0, H1 = 0x3FFD8818, 0x40000000
ht, hb = dy(H1), dy(H0)
R(DX, ht, CW, hb - ht, C_HEAP, stroke="#a08030", sw=1.2)
T(DX + CW / 2, ht + 24, "堆 Heap", F_MAIN, anchor="middle", weight="bold")
T(DX + CW / 2, ht + 42, "161,768 B", F_BLK, anchor="middle")
T(DX + CW / 2, ht + 59, "（158 KB）", F_TINY, anchor="middle")
T(DX + CW / 2, ht + 78, "空闲 155,392 B（96.0%）", F_TINY, anchor="middle")

# 已用部分：按真实比例，只占 4.3%，画在堆底（低地址）。不再编造碎片块
HEAP_TOT, HEAP_FREE = 161768, 155392
HEAP_USED = HEAP_TOT - HEAP_FREE          # 6,376 B（含堆元数据）
_uh = (hb - ht) * HEAP_USED / HEAP_TOT
R(DX, hb - _uh, CW, _uh, C_USED, stroke="#8a6a20", sw=0.8)
_uy = hb - _uh / 2
L(DX + CW, _uy, LBL - 22, _uy, "#8a6a20", 0.8)      # 短引线，不伸到标签列
T(LBL, _uy - 8, "已用 6,376 B", F_TINY, fill="#6a4a10", weight="bold")
T(LBL, _uy + 6, "约 4%（含元数据）", F_TINY, fill="#6a4a10")

# 最大连续块：只标数值，不画臆测位置
L(DX + CW, 345, LBL - 4, 345, "#8a2a2a", 0.8, dash="3,2")
T(LBL, 341, "最大连续块（实测）", F_BLK, fill="#8a2a2a", weight="bold")
T(LBL, 356, "空闲 110,580 B", F_TINY, fill="#8a2a2a")
T(LBL, 371, "跑过 TLS 后 63,476 B", F_TINY, fill="#8a2a2a")
T(LBL, 386, "最坏 45,044 B", F_TINY, fill="#8a2a2a")

for a in (DLO, 0x3FFC0000, 0x3FFD0000, 0x40000000):
    L(DX - 6, dy(a), DX, dy(a), AX_, 1)
    T(DX - 9, dy(a) + 4, f"0x{a:08X}", F_TINY, fill="#444", anchor="end")

# ================= 3. IRAM =================
IX = 810
ILO, IHI = 0x40080000, 0x400A0000
isc = (BOT - TOP) / (IHI - ILO)
iy = lambda a: BOT - (a - ILO) * isc

R(IX, TOP, CW, BOT - TOP, "#fcfcfa", stroke=AX_, sw=1.5)
T(IX + CW / 2, TOP - 12, "IRAM 128 KB", F_SEC, anchor="middle", weight="bold")

R(IX, iy(0x40080404), CW, iy(ILO) - iy(0x40080404), "#aebdd0", stroke="#5a6e86")
_it, _ib2 = iy(0x4009636C), iy(0x40080404)
_mid = (_it + _ib2) / 2
R(IX, _it, CW, _ib2 - _it, C_CODE, stroke="#7e93ab")
T(IX + CW / 2, _mid - 8, ".iram0.text", F_MAIN, anchor="middle", weight="bold")
T(IX + CW / 2, _mid + 9, "89,959 B", F_BLK, anchor="middle")
T(IX + CW / 2, _mid + 26, "中断/调度/TLSF", F_TINY, anchor="middle")

L(IX + CW, iy(ILO), IX + CW + 12, iy(ILO) - 16, "#5a6e86", 0.8)
T(IX + CW + 15, iy(ILO) - 20, "向量表", F_TINY, weight="bold")
T(IX + CW + 15, iy(ILO) - 6, "1,028 B", F_TINY)

R(IX, iy(IHI), CW, iy(0x4009636C) - iy(IHI), "#fbfbfb", stroke="#ccc", dash="4,3")
_fc = (iy(0x4009636C) + iy(IHI)) / 2
T(IX + CW / 2, _fc - 4, "空闲", F_BLK, fill="#4a4a4a", anchor="middle")
T(IX + CW / 2, _fc + 13, "40,084 B", F_BLK, fill="#4a4a4a", anchor="middle")

for a in (ILO, IHI):
    L(IX - 6, iy(a), IX, iy(a), AX_, 1)
    T(IX - 9, iy(a) + 4, f"0x{a:08X}", F_TINY, fill="#444", anchor="end")

# ================= 4. 任务栈 =================
SX, SY, SWd, SH = 1060, 150, 210, 300
R(SX, SY, SWd, SH, "#fffdf6", stroke="#a08050", sw=1.2, rx=6)
T(SX + SWd / 2, SY + 22, "loopTask 栈 8 KB", F_MAIN, anchor="middle", weight="bold")

sx, sw2 = SX + 26, 44
st, sb = SY + 40, SY + SH - 42
R(sx, st, sw2, sb - st, "#f7f0e2", stroke="#a08050")
ym = sb - (sb - st) * (4368 / 8192)
R(sx, ym, sw2, sb - ym, C_STACK)
T(sx + sw2 / 2, (ym + sb) / 2 + 5, "4,368", F_TINY, fill="#3a1200", anchor="middle", weight="bold")
T(sx + sw2 / 2, st + 26, "未用", F_TINY, fill="#4a4a4a", anchor="middle")
L(sx - 5, ym, sx, ym, "#8a3a10", 1.4)
T(sx - 7, ym + 4, "SP", F_TINY, fill="#8a3a10", anchor="end")

tx = SX + 84
T(tx, SY + 56, "已用 4,368 B", F_BLK, fill="#8a3a10", weight="bold")
T(tx, SY + 74, "余量 3,824 B（46.7%）", F_BLK, fill="#1f5a1f")
T(tx, SY + 92, "空闲时只占 2,900 B", F_TINY)
L(tx, SY + 108, SX + SWd - 14, SY + 108, "#e0e0e0")
T(tx, SY + 130, "栈不在 .bss", F_TINY)
T(tx, SY + 147, "任务创建时 malloc", F_TINY)
T(tx, SY + 172, "溢出会踩坏", F_TINY, fill="#a03030")
T(tx, SY + 189, "相邻堆块", F_TINY, fill="#a03030")
T(tx, SY + 214, "峰值取自", F_TINY)
T(tx, SY + 231, "TaskHighWaterMark", F_TINY, fill="#555")
T(tx, SY + 248, "（since-boot）", F_TINY)

# ================= 5. 明细表 =================
TY = 940
R(60, TY, 560, 210, "#fffef9", stroke="#b0a080", sw=1.2, rx=6)
T(76, TY + 26, "Flash 开头 5 个小分区", F_MAIN, weight="bold")
rows = [("1", "bootloader", "24,992 B", "0x001000"),
        ("2", "保留 / 引导参数", "4,096 B", "0x000000"),
        ("3", "分区表", "224 B", "0x008000"),
        ("4", "nvs（凭据/配置）", "20 KB", "0x009000"),
        ("5", "otadata", "8 KB", "0x00E000")]
for k, (num, name, size, addr) in enumerate(rows):
    y = TY + 56 + k * 28
    T(76, y, num, F_TINY, fill="#333")
    T(92, y, name, F_BLK)
    T(430, y, size, F_BLK, anchor="end")
    T(446, y, addr, F_TINY, fill="#555")

R(660, TY, 560, 210, "#fffef9", stroke="#b0a080", sw=1.2, rx=6)
T(676, TY + 26, "固件镜像 6 段", F_MAIN, weight="bold")
# 与柱内 6 段一一对应（自上而下 = 高地址到低地址），色块取自 SEGS
_segcol = {nm: col for nm, _n, col in SEGS}
segs = [("RTC_DATA",     "32 B",      "→ 0x50000200"),
        ("IRAM 段②",     "86,720 B",  "→ 0x400810AC"),
        ("IROM .text",   "854,248 B", "→ 0x400D0020"),
        ("IRAM 段①",     "4,268 B",   "→ 0x40080000"),
        ("DRAM .data",   "25,416 B",  "→ 0x3FFBDB60"),
        ("DROM .rodata", "166,900 B", "→ 0x3F400020")]
for k, (name, size, addr) in enumerate(segs):
    y = TY + 52 + k * 26          # 6 行压进卡片，底部留 15px
    R(678, y - 9, 11, 11, _segcol[name], stroke="#555", sw=0.7)
    T(696, y, name, F_BLK)
    T(1058, y, size, F_BLK, anchor="end")
    T(1206, y, addr, F_TINY, fill="#555", anchor="end")

# ================= 6. 三张卡片 =================
CY = 1180
CWd, CHd = 383, 168
cards = [("① 静态占用", [(".data　25,413 B", ""), (".bss　84,336 B", ""),
                    ("合计 109,749 B", "占 33%")], "#e8ecf2"),
         ("② 剩下给堆", [("DRAM 335,872（328 KB）", ""), ("− 保留 64,348", ""),
                    ("− 静态 109,749 + 对齐 7", ""), ("= 堆 161,768 B", "158 KB"),
                    ("实测空闲 155,392 B", "差 6,376")], "#fdf3dd"),
         ("③ 最大连续块", [("空闲 110,580 B", ""), ("TLS 后 63,476 B", ""),
                     ("最坏 45,044 B", "TLS 发送期")], "#fbe4e4")]
for idx, (title, rs, bg) in enumerate(cards):
    cx = 60 + idx * (CWd + 20)
    R(cx, CY, CWd, CHd, bg, stroke="#aaa", rx=8)
    T(cx + 14, CY + 26, title, F_MAIN, weight="bold")
    for j, (k, v) in enumerate(rs):
        T(cx + 14, CY + 52 + j * 25, k, F_BLK)
        if v:
            T(cx + CWd - 14, CY + 52 + j * 25, v, F_TINY, fill="#555", anchor="end")

# ================= 6b. 运行期峰值 =================
BY = CY + CHd + 32                      # 1380
BW1, BW2, BH = 650, 510, 250

# ④ 每轮请求的临时增量（旧图内容，迁移至此）
R(60, BY, BW1, BH, "#f3f7ee", stroke="#8aa87a", sw=1.2, rx=8)
T(74, BY + 26, "④ 一次请求实测（memprobe 逐阶段埋点）", F_MAIN, weight="bold")
T(74, BY + 46, "串行执行，单轮从堆里拿这些，收尾全部归还", F_TINY, fill="#555")
reqs = [("收请求 + 解析（明文落 48 KB 静态区）", "0 B"),
        ("gzip 上传（压缩体 String）", "2,180 B"),
        ("上游请求期（Ollama）", "2,900 B"),
        ("收尾（chunk 回写 + 序列化）", "6–15 KB"),
        ("单轮峰值：Ollama / gzip+历史", "11,848 / 14,712 B")]
for k, (nm, v) in enumerate(reqs):
    y = BY + 78 + k * 26
    T(74, y, nm, F_BLK)
    T(60 + BW1 - 16, y, v, F_BLK, anchor="end")
L(74, BY + 198, 60 + BW1 - 16, BY + 198, "#c8d8bc")
T(74, BY + 216, "峰值都小于静态工作区，上限由此定死", F_BLK, weight="bold")
T(74, BY + 234, "工作区 48 KB 在 .bss，不随请求涨落、不产生碎片", F_TINY, fill="#3f6a3f")

# ⑤ TLS 叠加（仅 DeepSeek 后端）
R(730, BY, BW2, BH, "#fbe4e4", stroke="#c08080", sw=1.2, rx=8)
T(744, BY + 26, "⑤ 仅 DeepSeek：TLS 叠加（实测）", F_MAIN, weight="bold")
T(744, BY + 46, "openai_client.cpp 用 WiFiClientSecure + setInsecure()", F_TINY, fill="#555")
tls = [("握手一次性跳变（tls_up）", "+46,436 B"),
       ("发送请求体瞬间峰值", "66,004 B"),
       ("峰值时最大连续块", "45,044 B")]
for k, (nm, v) in enumerate(tls):
    y = BY + 78 + k * 26
    T(744, y, nm, F_BLK)
    T(730 + BW2 - 16, y, v, F_BLK, anchor="end")
L(744, BY + 156, 730 + BW2 - 16, BY + 156, "#d8b8b8")
T(744, BY + 176, "峰值 66,004 B", F_BLK, weight="bold", fill="#8a2a2a")
T(744, BY + 196, "TLS 后 blk 110,580 → 63,476，不复原", F_TINY, fill="#8a2a2a")
T(744, BY + 214, "连发 4 次峰值稳定、不累积", F_TINY, fill="#8a2a2a")
T(744, BY + 230, "详见 docs/内存/内存实测-探针-2026-11-01.md", F_TINY, fill="#8a2a2a")

# ================= 6c. 不占 DRAM 的项 =================
BY2 = BY + BH + 30
R(60, BY2, W - 120, 158, "#fffef9", stroke="#b0a080", sw=1.2, rx=8)
T(76, BY2 + 26, "⑥ 容易误算为占 DRAM、实际不占的项", F_MAIN, weight="bold")
nd = [("明文 JSON、解压结果", "在 g_work 静态工作区，不走堆"),
      ("完整回答文本", "只在浏览器，板上只留 2 KB 前缀"),
      ("网页 / marked.js", "Flash 里 LittleFS（1,408 KB 分区）"),
      ("coredump", "Flash 0x3F0000（64 KB，崩溃时写）")]
for k, (a1, a2) in enumerate(nd):
    y = BY2 + 56 + k * 25
    T(76, y, a1, F_BLK)
    T(330, y, a2, F_TINY, fill="#555")

# ================= 7. 图例与脚注 =================
LY = BY2 + 158 + 30
T(60, LY, "图例", F_BLK, weight="bold")
leg = [(C_DATA, "已初始化 .data"), (C_OWN, "本项目工作区"), (C_OWN2, "解压器"),
       (C_LIB, "库/协议栈 .bss"), (C_HEAP, "堆"), (C_USED, "堆已用块"),
       (C_STACK, "任务栈峰值"), (C_CODE, "IRAM 代码"), (C_FLASH, "Flash"),
       (C_FS, "LittleFS"), (C_ROM, "ROM/BT"), (C_CD, "coredump")]
for i, (col, lab) in enumerate(leg):
    x = 60 + (i % 4) * 300
    y = LY + 26 + (i // 4) * 22
    R(x, y - 11, 15, 12, col, stroke="#666", sw=0.7)
    T(x + 21, y, lab, F_TINY)

FY = LY + 100
L(60, FY, W - 60, FY, "#ddd")
T(60, FY + 22, "三段地址轴按真实地址跨度等比缩放，块高与字节数成正比。堆内色块位置为示意；", F_TINY, fill="#555")
T(60, FY + 39, "「空闲 155,392 B」「最大连续块 110,580 / 63,476 / 45,044 B」「栈 4,368 B」为串口实测。", F_TINY, fill="#555")
T(60, FY + 56, "运行期 ESP.getHeapSize()=262,228 B（DRAM 222,144 ＋ IRAM 40,084）；本图「堆 161,768 B」", F_TINY, fill="#555")
T(60, FY + 73, "只是 .bss 之后的 DRAM 段，WiFi/BT 保留窗运行期归 heap_caps（已分配 56.9 KB）。", F_TINY, fill="#555")
T(60, FY + 90, "④⑤ 的数值为 memprobe 实测（docs/内存/内存实测-探针-2026-11-01.md），不再是估算。", F_TINY, fill="#555")
T(60, FY + 107, "收尾阶段 6–15 KB 峰值的具体分配点尚未定位（chunkWrite 本身不分配）。", F_TINY, fill="#555")

add('</svg>')
import os
_out = os.path.dirname(os.path.abspath(__file__))
open(os.path.join(_out, "内存与存储占用图.svg"), "w", encoding="utf-8").write("\n".join(o))
print("ok", len(o))
