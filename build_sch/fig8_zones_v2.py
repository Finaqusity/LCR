# -*- coding: utf-8 -*-
"""图 8  按 LQFP100 实际引脚位置的 AFE 分区（不旋转芯片）"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyArrowPatch
from matplotlib import rcParams

rcParams["font.sans-serif"] = ["Microsoft YaHei"]
rcParams["axes.unicode_minus"] = False

fig, ax = plt.subplots(figsize=(11.6, 8.8), dpi=200)
ax.set_xlim(-2, 134)
ax.set_ylim(-26, 88)
ax.set_aspect("equal")
ax.axis("off")

ax.add_patch(Rectangle((0, 0), 128, 84, fc="#FBFDFF", ec="#1F3864", lw=2.0))

# —— MCU（0° 放置：pin1 左上），标注关键引脚所在边 ——
ax.add_patch(Rectangle((44, 46), 30, 30, fc="#EEF3EE", ec="#3E7A46", lw=1.8))
ax.text(59, 73.4, "MCU（不旋转，pin1 左上）", ha="center", fontsize=10, weight="bold")
ax.text(59, 60, "F407VET6\nLQFP100", ha="center", va="center", fontsize=8.4, color="#333333", linespacing=1.5)

# 引脚边标注
ax.plot([44], [48.0], marker="o", ms=9, color="#A33A3A")
ax.text(42.2, 44.4, "PA0(23) PA1(24)", ha="right", va="center", fontsize=8.2, color="#A33A3A", weight="bold")
ax.plot([53], [46], marker="o", ms=7, color="#B7791F")
ax.text(58.5, 43.2, "PA4–PA7(29–32) SPI1", ha="center", va="center", fontsize=7.8, color="#B7791F")
ax.plot([74], [58.0], marker="o", ms=7, color="#6B4E9B")
ax.text(75.8, 58.0, "PA8(67) MCO", ha="left", va="center", fontsize=7.8, color="#6B4E9B")
ax.text(126.5, 81.0, "顶边 81–88：PD0–PD7 → ULN2803（继电器）", ha="right", va="bottom", fontsize=7.4, color="#666666")
ax.text(126.5, 78.0, "顶边 92/93：PB6/PB7 → OLED（I²C1）　右边 68/69：PA9/PA10 → CH340", ha="right", va="bottom", fontsize=7.4, color="#666666")

# —— 模拟区（下 + 右 L 形）——
ax.add_patch(Rectangle((6, 4), 86, 34, fc="#E8F1FA", ec="#2E5496", lw=1.6))
ax.add_patch(Rectangle((92, 4), 32, 62, fc="#E8F1FA", ec="#2E5496", lw=1.6))
ax.text(9, 35.4, "模拟区（AFE）", ha="left", fontsize=10, weight="bold", color="#2E5496")

# —— 数字侧（左上/上方）——
ax.add_patch(Rectangle((4, 50), 34, 30, fc="#F2F2F2", ec="#777777", lw=1.3))
ax.text(21, 76.0, "数字侧（不属模拟区）", ha="center", fontsize=9, color="#555555", weight="bold")
ax.text(21, 61, "电源入口 / LDO\nCH340 + Type-C\nULN2803 + 继电器\n排针", ha="center", va="center",
        fontsize=7.8, color="#555555", linespacing=1.5)


def zone(x, y, w, h, title, body, fc, ec, fs=9.6, bs=7.8):
    ax.add_patch(Rectangle((x, y), w, h, fc=fc, ec=ec, lw=1.4))
    ax.text(x + w / 2, y + h - 2.3, title, ha="center", va="center", fontsize=fs, weight="bold")
    ax.text(x + w / 2, y + h / 2 - 1.0, body, ha="center", va="center", fontsize=bs,
            color="#333333", linespacing=1.5)


zone(38, 6, 26, 26, "① 调理 → ADC", "AC 耦合 + 1.65 V\n跟随 + 100 Ω/1 nF\n钳位 → PA0/PA1", "#EEF3EE", "#3E7A46")
zone(66, 6, 22, 26, "② I-V 与量程", "U4 + BUF634A\nRf 六档 + 继电器\nΣ 保护环", "#E8F1FA", "#2E5496")
zone(93, 6, 29, 22, "③ 夹具", "4 × BNC\nF+/S+/F−/S−", "#FDE8E8", "#A33A3A")
zone(93, 34, 29, 30, "④ 激励", "AD9833 + 低通\n放大 + BUF634A\nTP1（F+）", "#FFF4E5", "#B7791F")
zone(8, 6, 24, 26, "⑤ 虚地与基准", "2.5 V 虚地\n1.65 V 中点\n（电源，不是信号）", "#F3EEFB", "#6B4E9B")


def flow(x1, y1, x2, y2, color="#3E7A46", lw=2.1):
    ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>", mutation_scale=16,
                                 lw=lw, color=color, shrinkA=0, shrinkB=0))

flow(107, 33.6, 107, 28.4, color="#B7791F")   # 激励 → 夹具（向下）
flow(92.5, 17, 88.5, 17, color="#A33A3A")  # 夹具 → I-V
flow(65.5, 19, 64.5, 19)                   # I-V → 调理
flow(51, 32.5, 46, 45.5, color="#A33A3A")  # 调理 → PA0
ax.text(53.0, 38.6, "Vin / Vo ≤ 20 mm", fontsize=7.8, color="#A33A3A", ha="left", va="center")

notes = [
    "① 不要为 PA0 去旋转芯片：0° 放置时 PA0/PA1(23/24) 就在左下角，正对模拟区，Vin/Vo 只有 10–20 mm。",
    "② 唯一规则：调理/ADC 区贴着 PA0/PA1，其余按“离 PA0 由近到远”排 —— I-V → 夹具 → 激励。",
    "③ 夹具贴右侧板边（BNC），激励放右上角：PA8(67) MCO 在芯片右边中部 → 到 AD9833 最短。",
    "④ SPI1 在底边(29–32)、PD0–PD7 在顶边(81–88) → 继电器驱动放上方数字侧，控制线短且不跨模拟区。",
    "⑤ 虚地与 1.65 V 是“电源”，放模拟区最左下角；不要夹在 I-V 与夹具之间。",
    "⑥ 数字侧（LDO、CH340、ULN2803、排针）整体压在左上/上方，模拟区只保留 AFE，中间打一圈地过孔。",
]
for i, t in enumerate(notes):
    ax.text(1.5, -6.5 - i * 3.1, t, fontsize=7.9, color="#333333", va="center")

fig.savefig(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig8_afe_zones_v2.png"),
            bbox_inches="tight", facecolor="white")
print("saved fig8_afe_zones_v2.png")

