# -*- coding: utf-8 -*-
"""图 7  模拟区（AFE）内部分区：沿信号流排布 + 保护环 + 单点接地"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyArrowPatch
from matplotlib import rcParams

rcParams["font.sans-serif"] = ["Microsoft YaHei"]
rcParams["axes.unicode_minus"] = False

fig, ax = plt.subplots(figsize=(11.4, 8.2), dpi=200)
ax.set_xlim(-2, 122)
ax.set_ylim(-20, 84)
ax.set_aspect("equal")
ax.axis("off")

ax.add_patch(Rectangle((0, 0), 120, 78, fc="#F7FBFF", ec="#2E5496", lw=2.0))
ax.text(2, 74, "模拟区内部（AFE）", fontsize=12, weight="bold", color="#2E5496", va="center")


def zone(x, y, w, h, title, body, fc, ec, fs=10.2, bs=8.0):
    ax.add_patch(Rectangle((x, y), w, h, fc=fc, ec=ec, lw=1.5))
    ax.text(x + w / 2, y + h - 2.6, title, ha="center", va="center", fontsize=fs, weight="bold")
    ax.text(x + w / 2, y + h / 2 - 1.4, body, ha="center", va="center", fontsize=bs,
            color="#333333", linespacing=1.55)


# ① 激励
zone(3, 46, 30, 26, "① 激励", "AD9833 + 低通\n前置放大 + BUF634A\nTP1（F+）", "#FFF4E5", "#B7791F")
# ② 夹具
zone(38, 46, 30, 26, "② 夹具", "4 × BNC\nF+ / S+ / F− / S−\nDUT 夹口（四线汇合）", "#FDE8E8", "#A33A3A")
# ③ I-V + 量程
zone(73, 46, 44, 26, "③ I-V 与量程", "U4 OPA2810 + BUF634A（环内）\nRf 六档 + 6 只继电器（Σ = 求和节点）", "#E8F1FA", "#2E5496")
# ④ 调理
zone(73, 8, 44, 30, "④ 双通道调理 → ADC", "AC 耦合 → 1.65 V 重偏置\n跟随 → 100 Ω + 1 nF → 钳位\nADC1_IN0(PA0) / ADC2_IN1(PA1)", "#EEF3EE", "#3E7A46")
# ⑤ 虚地与基准
zone(3, 8, 30, 30, "⑤ 虚地与基准", "2.5 V 虚地（分压 + 缓冲）\n1.65 V ADC 中点\n每颗 LDO 就近去耦", "#F3EEFB", "#6B4E9B")
# ⑥ 继电器驱动（数字侧边缘）
zone(38, 8, 30, 30, "⑥ 继电器驱动", "ULN2803 + 续流二极管\n控制线来自 MCU\n线圈电流本地回流", "#F2F2F2", "#777777")

# 信号流
def flow(x1, y1, x2, y2, color="#3E7A46", lw=2.2):
    ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>", mutation_scale=18,
                                 lw=lw, color=color, shrinkA=0, shrinkB=0))

flow(33.6, 59, 37.4, 59)
flow(68.6, 59, 72.4, 59)
ax.annotate("", xy=(91, 44.5), xytext=(91, 46.2), arrowprops=dict(arrowstyle="-|>", lw=2.0, color="#3E7A46"))
ax.text(92.3, 45.2, "Vo → 调理", fontsize=8, color="#3E7A46", va="center")

# 保护环
ax.add_patch(Rectangle((75.5, 47.6), 10, 5.2, fill=False, ec="#A33A3A", lw=1.6, ls=(0, (5, 3))))
ax.text(86.2, 50.2, "保护环：包住 Σ 与 Rf 落点", fontsize=7.8, color="#A33A3A", va="center")

# 单点接地
ax.plot([60], [3.6], marker="v", ms=13, color="#B7791F")
ax.text(60, 1.0, "GND_STAR（模拟地单点，紧邻 ADC 与调理区）", ha="center", fontsize=8.2, color="#B7791F")

notes = [
    "① 沿信号流排：激励 → 夹具 → I-V/量程 → 调理/ADC，一条直线，不要来回折；夹具贴板边，AFE 到 BNC ≤ 30 mm。",
    "② 激励的回流（BUF634A 输出电流）与测量回流（DUT 电流）在夹具处分开，各走各的地铜，最后到 GND_STAR 汇合。",
    "③ 求和节点是全板最脆弱的一点：四周围保护环（由 2.5 V 经 100 Ω 驱动），下面 L2 地完整，附近不放任何数字走线。",
    "④ AD9833 是数字器件（SPI + 8 MHz MCLK），放在激励区最外侧，SPI 线串 33 Ω，MCLK 走线包地。",
    "⑤ 继电器线圈电流（6 × 40 mA 脉冲）只在数字侧本地回流，驱动芯片别放进模拟区。",
    "⑥ 每组 LDO 就近去耦：VDDA 用 10 Ω + 1 µF + 100 nF + 10 nF，不要用磁珠（会谐振）。",
]
for i, t in enumerate(notes):
    ax.text(1.5, -3.2 - i * 3.0, t, fontsize=7.9, color="#333333", va="center")

fig.savefig(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig7_afe_zones.png"),
            bbox_inches="tight", facecolor="white")
print("saved fig7_afe_zones.png")


