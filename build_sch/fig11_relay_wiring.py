# -*- coding: utf-8 -*-
"""图 11：G6K-2F-Y 带封装的接线图（每一档的接法 + 封装俯视 + 六只对照表）"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Rectangle
import os
from schlib import Sch

BLUE = "#1F4E79"
GREEN = "#1E7B34"
RED = "#A33A3A"
GRAY = "#666666"

s = Sch((0, 37.0), (-1.2, 20.6), w=13.6, fs=9.2)
ax = s.ax

s.text(0.4, 19.8, "G6K-2F-Y-5V 带封装接线图：一档一条支路，只闭合一只", fs=11.5, ha="left", va="center")

# ---------------- ① 每一档的实际接法 ----------------
s.text(0.4, 18.1, "① 每一档怎么接（以 K1 = 10 Ω 档为例）", fs=9.0, ha="left", va="center", color=BLUE)

SUM_X = 2.2
s.wire([(SUM_X, 6.6), (SUM_X, 14.6)])
s.text(SUM_X, 15.15, "SUM", fs=8.2, ha="center", va="bottom")
s.text(SUM_X, 6.05, "虚地 3.3 V", fs=7.0, ha="center", va="top", color=GRAY)

# Rf 支路（A 组：6 = COM，5 = NO）
y1 = 13.0
s.dot(SUM_X, y1)
s.wire([(SUM_X, y1), (3.5, y1)])
s.res((3.5, y1), (6.5, y1), "Rf", loc="top")
s.wire([(6.5, y1), (7.6, y1)])
s.dot(7.6, y1); s.text(7.45, y1 + 0.45, "6", fs=7.6, ha="right", va="bottom", color=BLUE)
ax.add_patch(Circle((9.9, y1), 0.13, fill=False, lw=1.7, zorder=4))
s.wire([(7.6, y1), (9.55, y1 + 0.55)])
s.wire([(9.9, y1), (11.6, y1)])
s.text(11.75, y1, "→ V_o", fs=8.2, ha="left", va="center", color=GREEN)
s.text(9.55, y1 + 0.95, "5（NO）", fs=7.4, ha="center", va="bottom", color=BLUE)
s.text(7.4, y1 - 1.05, "7（NC）：只有默认档 K4 用它接 V_o", fs=6.9, ha="left", va="center", color=GRAY)

# Cc 支路（B 组：3 = COM，4 = NO）
y2 = 9.4
s.dot(SUM_X, y2)
s.wire([(SUM_X, y2), (3.5, y2)])
s.cap((3.5, y2), (5.2, y2), "Cc", loc="bottom", off=0.42)
s.wire([(5.2, y2), (6.4, y2)])
s.dot(6.4, y2); s.text(6.25, y2 + 0.45, "3", fs=7.6, ha="right", va="bottom", color=BLUE)
ax.add_patch(Circle((8.7, y2), 0.13, fill=False, lw=1.7, zorder=4))
s.wire([(6.4, y2), (8.35, y2 + 0.55)])
s.wire([(8.7, y2), (10.4, y2)])
s.text(10.55, y2, "→ V_o", fs=8.2, ha="left", va="center", color=GREEN)
s.text(8.35, y2 + 0.95, "4（NO）", fs=7.4, ha="center", va="bottom", color=BLUE)
s.text(3.4, 7.55, "Cc：10 Ω/100 Ω 档不贴；100 kΩ/1 MΩ 档贴 1–2.2 pF", fs=6.8, ha="left", va="center", color=GRAY)


# ---------------- ② 封装俯视 ----------------
s.text(16.6, 18.1, "② 封装接线（俯视）", fs=9.0, ha="left", va="center", color=BLUE)

bx0, bx1, by0, by1 = 18.2, 23.6, 7.2, 15.4
ax.add_patch(Rectangle((bx0, by0), bx1 - bx0, by1 - by0, fill=False, lw=1.8, zorder=3))
s.text((bx0 + bx1) / 2, by0 - 0.55, "U17  G6K-2F-Y-5V", fs=8.2, ha="center", va="top")
ax.add_patch(Circle((bx0 + 0.55, by1 - 0.55), 0.16, fill=True, color="k", zorder=5))
s.text(bx0 + 1.5, by1 - 0.55, "方向标记", fs=6.6, ha="left", va="center", color=GRAY)

left_pins = [("5", 14.4, "→ V_o", BLUE),
             ("6", 12.4, "← Rf", BLUE),
             ("7", 10.4, "→ V_o（K4）", RED),
             ("8", 8.4, "← ULN", GREEN)]
right_pins = [("4", 14.4, "→ V_o", BLUE),
              ("3", 12.4, "← Cc", BLUE),
              ("2", 10.4, "悬空", GRAY),
              ("1", 8.4, "← +5 V", GREEN)]

for num, y, label, col in left_pins:
    s.wire([(bx0 - 1.3, y), (bx0, y)])
    s.text(bx0 - 0.25, y, num, fs=8.0, ha="right", va="center")
    s.text(bx0 - 1.45, y, label, fs=6.9, ha="right", va="center", color=col)
for num, y, label, col in right_pins:
    s.wire([(bx1, y), (bx1 + 1.3, y)])
    s.text(bx1 + 0.25, y, num, fs=8.0, ha="left", va="center")
    s.text(bx1 + 1.45, y, label, fs=6.9, ha="left", va="center", color=col)

# 内部：线圈与两刀
ax.add_patch(Rectangle((20.1, 8.0), 1.6, 0.9, fill=False, lw=1.2))
s.text(20.9, 7.55, "线圈 237 Ω", fs=6.6, ha="center", va="top", color=GRAY)
s.wire([(19.6, 8.4), (20.1, 8.4)])
s.wire([(21.7, 8.4), (22.2, 8.4)])
s.text(20.9, 9.25, "+", fs=7.0, ha="center", va="bottom", color=RED)
# 两刀
s.wire([(bx0, 12.4), (19.6, 12.4)])
s.wire([(19.6, 12.4), (21.0, 11.7)])
s.wire([(bx0, 14.4), (19.6, 14.4)])
s.dot(19.6, 12.4)

s.wire([(22.2, 12.4), (bx1, 12.4)])
s.wire([(22.2, 12.4), (20.8, 11.7)])
s.wire([(22.2, 14.4), (bx1, 14.4)])
s.dot(22.2, 12.4)
s.text((bx0 + bx1) / 2, 6.3, "第一刀 = 5/6/7（切 Rf）　第二刀 = 2/3/4（切 Cc）", fs=6.8, ha="center", va="top", color=GRAY)

# ---------------- ③ 六只对照表 ----------------
s.text(27.2, 18.1, "③ 六只怎么分配", fs=9.0, ha="left", va="center", color=BLUE)
hdr = ["位号", "Rf", "控制", "第一刀", "第二刀"]
cols = [27.2, 28.6, 30.0, 31.4, 33.2]
for x, h in zip(cols, hdr):
    s.text(x, 16.7, h, fs=7.2, ha="left", va="center", color=GRAY)
rows = [
    ("K1", "10 Ω",   "PD0", "NO(5)", "—"),
    ("K2", "100 Ω",  "PD1", "NO(5)", "—"),
    ("K3", "1 kΩ",   "PD2", "NO(5)", "—"),
    ("K4", "10 kΩ",  "PD3", "NC(7)", "—"),
    ("K5", "100 kΩ", "PD4", "NO(5)", "Cc"),
    ("K6", "1 MΩ",   "PD5", "NO(5)", "Cc"),
]
y = 15.8
for r in rows:
    for i, (x, t) in enumerate(zip(cols, r)):
        col = RED if (r[0] == "K4" and i in (3, 4)) else "k"
        s.text(x, y, t, fs=7.0, ha="left", va="center", color=col)
    y -= 1.0
s.text(27.2, 9.5, "K4 不吸合 → 10 kΩ 在位（默认档）", fs=6.9, ha="left", va="center", color=RED)
s.text(27.2, 8.8, "测别的档：先吸 K4 断开 10 kΩ，", fs=6.9, ha="left", va="center", color=GRAY)
s.text(27.2, 8.2, "再吸目标档（先断后吸，各等 5–10 ms）", fs=6.9, ha="left", va="center", color=GRAY)

# ---------------- 注释 ----------------
s.text(0.4, 3.4,
       "① 线圈：1 = +5 V，8 = ULN2803 输出（灌电流）；1N4148 就近跨 1–8、阴极接 1；每只 21.1 mA，6 只同时吸合约 127 mA。\n"
       "② 顺序：全断 → 等 5 ms → 吸目标档 → 等 5 ms → 采样（继电器动作 ≤3 ms，触点在毫秒级弹跳）。\n"
       "③ 封装：-Y 版焊盘 3.2 / 5.4 / 7.6 mm（普通 2F 是 2.54 / 5.08 / 7.62，不可混）；PCB 1 脚焊盘对准实物方向标记。\n"
       "④ 触点电阻 ≤100 mΩ 串在 Rf 里是乘性误差（10 Ω 档即 1%），靠每档 0.1% 已知电阻做负载校准吸收；短路校准消不掉。\n"
       "⑤ 两刀共用一个线圈（机械联动），所以 Rf 与 Cc 一定同时接入/断开，不存在时序问题。",
       fs=7.2, ha="left", va="top")

s.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig11_relay_wiring.png"), dpi=200)





