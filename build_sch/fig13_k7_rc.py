# -*- coding: utf-8 -*-
"""图 13：K7 校准继电器 + 两路 RC 前端（CH1=Vin/S+，CH2=Vo）接线图"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import matplotlib
matplotlib.use("Agg")
from matplotlib.patches import Rectangle, FancyArrowPatch

from schlib import Sch

BLUE = "#1F4E79"
GRAY = "#666666"
GREEN = "#1E7B34"
RED = "#A33A3A"
ORANGE = "#B7791F"
BOX = "#B4C6E7"

s = Sch((0, 30.0), (-2.6, 22.6), w=11.0)


def block(x, y, w, h, lines, fs=6.9, ec=RED, fc="#FFFFFF"):
    ax = s.ax
    ax.add_patch(Rectangle((x, y - h / 2), w, h, fill=True, fc=fc, ec=ec, lw=1.35, zorder=3))
    n = len(lines)
    for i, ln in enumerate(lines):
        yy = y + (n - 1) * 0.44 - i * 0.88
        ax.text(x + w / 2, yy, ln, ha="center", va="center", fontsize=fs, zorder=4)


def arrow(x1, y1, x2, y2, color=BLUE, lw=1.2):
    s.ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>",
                                   mutation_scale=9, lw=lw, color=color, zorder=2))


def channel(y, res_label, cap_label, clamp_label, tag):
    block(2.5, y, 3.0, 2.2, [f"K7{tag}", "NC → COM"])
    s.wire([(5.5, y), (6.3, y)])
    s.res((6.3, y), (7.9, y), res_label, loc="top")
    s.wire([(7.9, y), (13.4, y)])
    s.dot(8.4, y)
    # 100 pF 下地
    s.wire([(8.4, y), (8.4, y - 0.55)])
    s.cap((8.4, y - 0.55), (8.4, y - 1.75), cap_label, loc="left")
    s.gnd(8.4, y - 1.75)
    # BAT54S 钳位
    s.dot(10.3, y)
    s.wire([(10.3, y), (10.3, y - 0.6)])
    block(9.0, y - 1.6, 2.6, 1.5, [clamp_label, "GND / +9VA"], fs=6.3, ec=GRAY, fc="#F7F7F7")
    # 1 µF 串联
    s.cap((11.7, y), (12.7, y), "1 µF", loc="top")
    arrow(13.5, y, 13.9, y)
    s.text(14.0, y, "→ ADC", fs=7.0, ha="left", va="center", color=GRAY)


# ── 面板 ① 正常测量 ─────────────────────────────────────────
s.ax.add_patch(Rectangle((0.5, 11.4), 14.9, 10.0, fill=False, ec=BOX, lw=1.2))
s.text(1.0, 20.75, "① 正常测量：K7 不吸合（COM–NC 通）", fs=8.4, ha="left", color=BLUE)
s.text(1.2, 20.0, "S+（BNC 中心）", fs=7.0, ha="left", color=GREEN)
s.wire([(1.2, 18.4), (2.5, 18.4)])
s.text(1.2, 16.3, "VO（母线）", fs=7.0, ha="left", color=GREEN)
s.wire([(1.2, 14.7), (2.5, 14.7)])
channel(18.4, "R21 1 kΩ", "C33\n100 pF", "D4 BAT54S", "A")
channel(14.7, "R40 1 kΩ", "C47\n100 pF", "D9 BAT54S", "B")
s.text(1.2, 12.3, "两路各自走完 1 kΩ + 100 pF + 钳位 + 1 µF → ADC", fs=6.4, ha="left", color=GRAY)

# ── 面板 ② 校准 ─────────────────────────────────────────────
OX = 15.6
s.ax.add_patch(Rectangle((OX, 11.4), 14.2, 10.0, fill=False, ec=BOX, lw=1.2))
s.text(OX + 0.5, 20.75, "② 校准：K7 吸合（COM–NO 通，两路同接 CAL）", fs=8.4, ha="left", color=BLUE)


def channel_cal(y, res_label, tag):
    s.wire([(OX + 0.7, y), (OX + 2.1, y)])
    block(OX + 2.1, y, 3.0, 2.2, [f"K7{tag}", "NO → COM"])
    s.wire([(OX + 5.1, y), (OX + 5.9, y)])
    s.res((OX + 5.9, y), (OX + 7.5, y), res_label, loc="top")
    s.wire([(OX + 7.5, y), (OX + 8.4, y)])
    s.dot(OX + 8.0, y)
    block(OX + 8.4, y - 1.35, 2.4, 1.3, ["同一套 RC", "不变"], fs=6.2, ec=GRAY, fc="#F7F7F7")
    arrow(OX + 11.0, y, OX + 11.4, y)
    s.text(OX + 11.5, y, "→ ADC", fs=7.0, ha="left", va="center", color=GRAY)


channel_cal(18.4, "R21 1 kΩ", "A")
channel_cal(14.7, "R40 1 kΩ", "B")
s.wire([(OX + 0.7, 13.4), (OX + 0.7, 19.2)])
s.dot(OX + 0.7, 18.4)
s.dot(OX + 0.7, 14.7)
s.text(OX + 0.7, 19.6, "CAL", fs=7.6, ha="center", va="bottom", color=ORANGE)
s.text(OX + 0.7, 12.9, "同一根线", fs=6.4, ha="center", va="top", color=ORANGE)
s.text(OX + 1.2, 12.0, "吸合后两路输入都是 CAL → α = CH1 / CH2", fs=6.4, ha="left", color=GRAY)

# ── 面板 ③ CAL 来源 / 线圈 / 改板说明 ────────────────────────
s.ax.add_patch(Rectangle((0.5, -2.4), 29.3, 12.6, fill=False, ec=BOX, lw=1.2))
s.text(1.0, 9.95, "③ CAL 信号从哪来、线圈怎么驱动、网表要改哪两处", fs=8.4, ha="left", color=BLUE)

# U17B 跟随器（+ 输入接 TP1，输出反馈到 −）
in_m, in_p, out = s.opamp(5.2, 6.9, w=1.9, h=1.6)
s.wire([(1.4, in_p[1]), in_p])
s.text(1.4, in_p[1] + 0.62, "TP1", fs=7.4, ha="left", color=GREEN)
s.text(1.4, 5.62, "U17B = OPA2810 空闲通道（跟随 TP1）", fs=7.0, ha="left", color=BLUE)
s.wire([out, (7.6, 6.9)])
s.wire([(7.6, 6.9), (7.6, 8.55)])
s.wire([(7.6, 8.55), (in_m[0], 8.55)])
s.wire([(in_m[0], 8.55), in_m])
s.dot(7.6, 6.9)
s.res((7.6, 6.9), (8.9, 6.9), "100 Ω", loc="bottom")
arrow(9.0, 6.9, 9.4, 6.9, color=ORANGE)
s.text(9.5, 6.9, "CAL", fs=7.6, ha="left", va="center", color=ORANGE)

# 线圈
s.text(1.4, 4.6, "线圈：+5 V → K7.1（＋）；K7.8 → U18.7（ULN2803 第 7 路，PD6）", fs=7.0, ha="left")
s.text(1.4, 3.95, "ULN2803 内部已有续流二极管；K7 型号 G6K-2F-Y-5V（DPDT，双刀同时动作）", fs=7.0, ha="left", color=GRAY)

# 改板两处
s.text(1.4, 2.95, "改板 ①（CH1）：剪开 R21 与 S+ 之间那根线 —— R21.1（网络 $1N239，只连 S+.1）", fs=7.0, ha="left", color=RED)
s.text(2.2, 2.4, "R21.1 → K7A.COM；S+.1 → K7A.NC；R21.2（Vin 网络，连 C33/C38/D4）不动", fs=7.0, ha="left", color=RED)
s.text(1.4, 1.7, "改板 ②（CH2）：新增 R40。VO 母线 → K7B.NC；R40 的 VO 侧 → K7B.COM；R40 另一端接 C47/D9/C40 不动", fs=7.0, ha="left", color=RED)
s.text(1.4, 1.0, "两刀 NO 都接 CAL；不吸合 = 正常测量，吸合 = 校准（失效安全方向）", fs=7.0, ha="left", color=GRAY)
s.text(1.4, 0.35, "固件：每个频率先吸合 K7 采一组 → α = m1/m2（复数）；测量时 Zx = −Zf · α · (Vin / Vo)", fs=7.0, ha="left", color=GRAY)
s.text(1.4, -0.35, "元件：R21/R40 = 1 kΩ 1% 0603；C33/C47 = 100 pF C0G 0603；D4/D9 = BAT54S；K7 线圈 5 V / 21.1 mA", fs=7.0, ha="left", color=GRAY)
s.text(1.4, -1.05, "注意：1 kΩ 只在“通道”这条支路上，绝不能串进 VO 主母线（那条线流过 Rf 的反馈电流）", fs=7.0, ha="left", color=RED)

# 状态表（右侧空位）
TB_X, TB_Y = 18.6, 6.9
block(TB_X, TB_Y, 10.4, 3.6, [], fs=6.6, ec=GRAY, fc="#F7F7F7")
s.text(TB_X + 0.3, TB_Y + 1.35, "K7 状态", fs=7.0, ha="left", color=BLUE)
s.text(TB_X + 0.3, TB_Y + 0.45, "不吸合（默认）：NC–COM 通 → S+ / VO 进各自 RC → 正常测量", fs=6.3, ha="left", color=GREEN)
s.text(TB_X + 0.3, TB_Y - 0.25, "吸合（校准）：NO–COM 通 → 两路都吃 CAL → 测 α", fs=6.3, ha="left", color=ORANGE)
s.text(TB_X + 0.3, TB_Y - 1.05, "线圈断线/驱动失效时自然回到“正常测量”，不会卡在校准态", fs=6.3, ha="left", color=GRAY)

s.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig13_k7_rc.png"))




