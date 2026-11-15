# -*- coding: utf-8 -*-
"""图 10：Rf 六档量程网络 —— 6 只电阻 + 6 只 G6K 继电器的具体接法"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle
import os
from schlib import Sch

BLUE = "#1F4E79"
GREEN = "#1E7B34"
RED = "#A33A3A"

s = Sch((0, 31.0), (-4.6, 19.4), w=11.6, fs=9.6)
ax = s.ax

# ---------- 两条母线 ----------
SUM_X, VO_X = 4.6, 26.2
Y_TOP, Y_BOT = 17.4, 4.9
s.wire([(SUM_X, Y_BOT), (SUM_X, Y_TOP)])
s.wire([(VO_X, Y_BOT), (VO_X, Y_TOP)])
s.text(SUM_X, Y_TOP + 0.55, "求和节点 SUM", fs=8.8, va="bottom", ha="center")
s.text(SUM_X, Y_TOP + 0.10, "U4 反相输入 · 虚地 3.3 V", fs=7.4, va="bottom", ha="center", color=BLUE)
s.text(VO_X, Y_TOP + 0.55, "V_o", fs=8.8, va="bottom", ha="center")
s.text(VO_X, Y_TOP + 0.10, "U4 / BUF634A 输出", fs=7.4, va="bottom", ha="center", color=BLUE)

ROWS = [
    ("K1", "10 Ω",   "10 Ω – 100 Ω",   False),
    ("K2", "100 Ω",  "100 Ω – 1 kΩ",   False),
    ("K3", "1 kΩ",   "1 k – 10 kΩ",    False),
    ("K4", "10 kΩ",  "10 k – 100 kΩ",  True),   # 默认档：用常闭
    ("K5", "100 kΩ", "100 k – 1 MΩ",   False),
    ("K6", "1 MΩ",   "1 M – 10 MΩ",    False),
]

y = 16.2
for name, val, cover, nc_default in ROWS:
    s.dot(SUM_X, y)
    s.wire([(SUM_X, y), (5.9, y)])
    s.res((5.9, y), (9.1, y), f"Rf = {val}", loc="top")
    s.wire([(9.1, y), (10.4, y)])
    s.dot(10.4, y)                                   # COM
    tx = 12.2                                        # 触点端子
    ax.add_patch(Circle((tx, y), 0.12, fill=False, lw=1.7, zorder=4))
    if nc_default:                                   # 常闭：刀片压在端子上
        s.wire([(10.4, y), (tx - 0.14, y)])
        s.wire([(tx, y), (VO_X, y)])
        s.text((10.4 + tx) / 2, y + 0.55, f"{name}（常闭 NC，默认档）", fs=7.8, va="bottom")
    else:                                            # 常开：刀片抬起
        s.wire([(10.4, y), (tx - 0.34, y + 0.52)])
        s.wire([(tx, y), (VO_X, y)])
        s.text((10.4 + tx) / 2, y + 0.62, f"{name}（常开 NO，吸合接入）", fs=7.8, va="bottom")
    s.text(19.3, y + 0.34, f"覆盖 {cover}", fs=7.4, color=GREEN, ha="left", va="center")
    s.dot(VO_X, y)
    y -= 2.15

# ---------- 第二刀：补偿电容 ----------
s.text(2.6, 3.95, "每档的第二刀（可选）：补偿电容与 Rf 并联、同步接入", fs=7.8, ha="left", va="bottom", color=BLUE)
s.wire([(3.0, 2.55), (4.6, 2.55)])
s.cap((4.6, 2.55), (6.4, 2.55), "Cc 1–2.2 pF", loc="bottom", off=0.55)
s.wire([(6.4, 2.55), (7.4, 2.55)])
s.dot(7.4, 2.55)
s.wire([(7.4, 2.55), (8.6, 2.55)])
ax.add_patch(Circle((9.0, 2.55), 0.12, fill=False, lw=1.7, zorder=4))
s.wire([(8.78, 2.55), (9.0, 2.55)])
s.wire([(9.0, 2.55), (10.4, 2.55)])
s.text(3.0, 2.05, "SUM", fs=7.6, ha="left", va="top", color=BLUE)
s.text(10.6, 2.05, "V_o", fs=7.6, ha="left", va="top", color=BLUE)
s.text(8.2, 3.25, "COM(6) → NO(5)", fs=7.2, ha="center", va="bottom", color=BLUE)
s.text(3.0, 1.42, "10 Ω / 100 Ω 档不贴；100 kΩ / 1 MΩ 档贴 1–2.2 pF（抵消 Rf 与触点的并联杂散）",
       fs=7.2, ha="left", va="top")

# ---------- 线圈驱动 ----------
s.text(17.4, 3.95, "线圈（每只继电器）", fs=8.2, ha="left", va="bottom", color=BLUE)
s.text(17.4, 3.45, "+5 V → pin 1 ──[线圈 237 Ω]── pin 8 → ULN2803 输出（灌电流）", fs=7.4, ha="left", va="top")
s.text(17.4, 2.88, "1N4148 就近并在线圈两端（阴极接 +5 V）", fs=7.4, ha="left", va="top")
s.text(17.4, 2.31, "PD0–PD5 选档，PD6 给 K7 校准；先断后吸、各等 5–10 ms", fs=7.4, ha="left", va="top")
s.text(17.4, 1.74, "上电默认 K4 不吸合 → 10 kΩ 在位，求和节点不会悬空", fs=7.4, ha="left", va="top", color=RED)

# ---------- 注释 ----------
s.text(0.4, -0.35,
       "① 触点在 Rf 串联回路里，触点电阻（≤100 mΩ）是乘性误差：10 Ω 档即 1%，短路校准消不掉 → 每档用 0.1% 已知电阻做负载校准。\n"
       "② 别的档要吸合 K4 先断开 10 kΩ，再吸目标档（软件先断后吸）；Form C 本身 break-before-make，但继电器之间无互锁。\n"
       "③ 继电器 G6K-2F-Y：线圈 1/8、公共端 3/6、常开/常闭 4-5 / 2-7（用万用表确认）；焊盘按 -Y 版 3.2/5.4/7.6 mm。\n"
       "④ 走线：6 只 Rf 到 SUM 与 V_o 的支路尽量短、等长；1 MΩ 档那一整块四周加保护环（VREF_AFE 经 100 Ω）。\n"
       "⑤ Rf 用 0.1%、≤25 ppm 的金属膜/薄膜（0805/1206）；10 Ω 档注意 50 mA rms 的电流与自热（0.025 W 没问题，但别用 0402）。",
       fs=7.3, ha="left", va="top")

s.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig10_ranges.png"), dpi=200)

