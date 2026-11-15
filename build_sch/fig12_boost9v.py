# -*- coding: utf-8 -*-
"""图 12：5 V → 9 V 升压（MT3608）原理图 + 9 V 分配与滤波"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Rectangle, Polygon, Arc
import numpy as np
import os
from schlib import Sch

BLUE = "#1F4E79"
GREEN = "#1E7B34"
RED = "#A33A3A"
GRAY = "#666666"

s = Sch((0, 28.0), (-0.8, 17.8), w=11.6, fs=9.4)
ax = s.ax

s.text(0.4, 16.9, "5 V → 9 V 升压（MT3608，1.2 MHz）与 9 V 分配", fs=11.0, ha="left", va="center")


def diode(x1, x2, y, label="D1"):
    """阳极在左、阴极在右"""
    mid = (x1 + x2) / 2.0
    ax.add_patch(Polygon([(mid - 0.30, y - 0.30), (mid - 0.30, y + 0.30), (mid + 0.22, y)],
                         closed=True, fill=False, lw=1.6, zorder=4))
    ax.plot([mid + 0.22, mid + 0.22], [y - 0.30, y + 0.30], color="k", lw=1.8, zorder=4)
    s.text(mid - 0.05, y - 0.52, label, fs=7.6, ha="center", va="top")


def inductor(x1, x2, y, label="L1"):
    n = 4
    w = (x2 - x1) / n
    for i in range(n):
        ax.add_patch(Arc(((x1 + w * (i + 0.5)), y), w, 0.62, theta1=0, theta2=180, lw=1.6, zorder=4))
    s.text((x1 + x2) / 2, y + 0.62, label, fs=7.6, ha="center", va="bottom")


# ---------------- 输入 ----------------
s.text(0.4, 12.25, "5 V 输入（ORing 之后）", fs=7.4, ha="left", va="bottom", color=GREEN)
s.wire([(1.1, 11.0), (6.4, 11.0)])
s.dot(1.1, 11.0)
s.cap((1.1, 11.0), (1.1, 9.9), "10 µF", loc="left", off=0.42)
s.gnd(1.1, 9.9)
s.cap((2.7, 11.0), (2.7, 10.2), "100 nF", loc="right", off=0.45)
s.gnd(2.7, 10.2)

# ---------------- MT3608 ----------------
bx0, bx1, by0, by1 = 6.4, 10.4, 5.6, 11.6
ax.add_patch(Rectangle((bx0, by0), bx1 - bx0, by1 - by0, fill=False, lw=1.8, zorder=3))
s.text((bx0 + bx1) / 2, (by0 + by1) / 2 + 0.75, "MT3608", fs=9.0, ha="center", va="center")
s.text((bx0 + bx1) / 2, (by0 + by1) / 2 + 0.05, "1.2 MHz 升压", fs=7.0, ha="center", va="center", color=GRAY)

# 引脚：左 IN(5)、EN(4)；右 SW(1)、FB(3)；下 GND(2)
s.text(bx0 - 0.3, 10.4, "5", fs=7.6, ha="right", va="center")
s.text(bx0 - 0.3, 7.0, "4", fs=7.6, ha="right", va="center")
s.text(bx1 + 0.3, 10.4, "1", fs=7.6, ha="left", va="center")
s.text(bx1 + 0.3, 7.0, "3", fs=7.6, ha="left", va="center")
s.text((bx0 + bx1) / 2 + 0.35, by0 - 0.35, "2", fs=7.6, ha="center", va="top")
s.text(bx0 - 0.45, 10.75, "IN", fs=6.6, ha="right", va="bottom", color=BLUE)
s.text(bx0 - 0.45, 6.65, "EN", fs=6.6, ha="right", va="top", color=BLUE)
s.text(bx1 + 0.45, 10.75, "SW", fs=6.6, ha="left", va="bottom", color=BLUE)
s.text(bx1 + 0.45, 6.65, "FB", fs=6.6, ha="left", va="top", color=BLUE)
s.text((bx0 + bx1) / 2 + 0.35, by0 + 0.30, "GND", fs=6.6, ha="center", va="bottom", color=BLUE)
s.wire([((bx0 + bx1) / 2 + 0.35, by0), ((bx0 + bx1) / 2 + 0.35, by0 - 1.1)])
s.gnd((bx0 + bx1) / 2 + 0.35, by0 - 1.1)

# VIN 进 IC
s.wire([(2.7, 11.0), (bx0, 11.0)])
s.wire([(bx0, 11.0), (bx0, 10.4)])
# EN 经 100 kΩ 接 VIN
s.wire([(bx0, 7.0), (4.2, 7.0)])
s.res((4.2, 7.0), (4.2, 8.4), "100 kΩ", loc="left")
s.wire([(4.2, 8.4), (4.2, 11.0)])
s.dot(4.2, 11.0)

# 电感：VIN → SW 节点
s.dot(3.4, 11.0)
s.wire([(3.4, 11.0), (3.4, 14.2)])
s.wire([(3.4, 14.2), (5.0, 14.2)])
inductor(5.0, 9.2, 14.2, "L1  10 µH")
s.wire([(9.2, 14.2), (12.2, 14.2)])
s.wire([(12.2, 14.2), (12.2, 10.4)])
s.wire([(bx1, 10.4), (12.2, 10.4)])
s.dot(12.2, 10.4)

# 肖特基：SW → VOUT
diode(12.2, 14.6, 10.4, "D1  SS34")
s.wire([(14.6, 10.4), (18.2, 10.4)])
s.dot(14.6, 10.4)

# 输出电容
s.wire([(14.6, 10.4), (14.6, 9.7)])
s.cap((14.6, 9.7), (14.6, 8.8), "22 µF", loc="left", off=0.45)
s.gnd(14.6, 8.8)
s.cap((16.0, 10.4), (16.0, 9.7), "100 nF", loc="right", off=0.46)
s.gnd(16.0, 9.7)

# 反馈分压：VOUT → R1 → FB → R2 → GND
s.wire([(18.2, 10.4), (18.2, 9.4)])
s.res((18.2, 9.4), (18.2, 7.6), "R1 140 kΩ", loc="right")
s.wire([(18.2, 7.6), (18.2, 6.8)])
s.dot(18.2, 6.8)
s.wire([(18.2, 6.8), (bx1, 6.8)])
s.wire([(bx1, 6.8), (bx1, 7.0)])
s.res((18.2, 6.8), (18.2, 5.4), "R2 10 kΩ", loc="right")
s.gnd(18.2, 5.4)
s.text(18.9, 6.55, "Vout = 0.6×(1+R1/R2) = 9.0 V", fs=7.2, ha="left", va="center", color=GRAY)

# 9 V 输出 → RC 滤波 → 9VA
s.wire([(18.2, 10.4), (19.0, 10.4)])
s.dot(19.0, 10.4)
s.res((19.0, 10.4), (21.0, 10.4), "10 Ω", loc="top")
s.wire([(21.0, 10.4), (23.6, 10.4)])
s.dot(23.6, 10.4)
s.cap((23.6, 10.4), (23.6, 9.5), "22 µF", loc="right", off=0.42)
s.gnd(23.6, 9.5)
s.wire([(23.6, 10.4), (26.2, 10.4)])
s.text(26.5, 10.4, "9VA", fs=9.0, ha="left", va="center", color=RED)
s.text(22.6, 11.1, "→ U2 / U3 / U4 供电", fs=7.6, ha="left", va="center", color=GREEN)
# 去耦
s.dot(25.6, 10.4)
s.wire([(25.6, 10.4), (25.6, 10.4)])
s.text(22.6, 9.0, "每个运放电源脚再各 10 µF + 100 nF", fs=7.2, ha="left", va="center", color=GRAY)

# 输入处标 5V 来源


# ---------------- 注释 ----------------
s.text(0.4, 3.4,
       "① 电感 L1 取 10–22 µH（饱和电流 ≥1 A）；D1 用 SS34/SK34（3 A 肖特基）；Cin 10 µF + 100 nF，Cout 22 µF + 100 nF，全部用陶瓷。\n"
       "② 反馈电阻要 1%：R1 = 140 kΩ、R2 = 10 kΩ → 0.6 × 15 = 9.0 V；想微调就把 R2 换成 10 kΩ + 多圈电位器。\n"
       "③ 开关回路（L1 → SW → D1 → Cout → GND）面积最小化，输入电容紧贴 IN/GND，反馈线从 VOUT 单独拉、不要从开关节点走。\n"
       "④ 布局：整个 boost 只放数字区（离 AFE ≥10 mm），L3 在 AFE 下方不铺 +5V/+9V 大电流铜皮；9 V 输出到 AFE 前串 10 Ω + 22 µF 做二次滤波（拐点 723 Hz，1.2 MHz 处衰减 60 dB 以上）。\n"
       "⑤ 电流预算（9 V 侧）：OPA2810 ×3 通道 ≈ 10.8 mA + BUF634A ×2（低功耗模式）≈ 3 mA + 负载（10 Ω 档峰值 70 mA），平均 <40 mA、峰值 ~85 mA → MT3608 余量充足。",
       fs=7.1, ha="left", va="top")

s.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig12_boost9v.png"), dpi=200)



