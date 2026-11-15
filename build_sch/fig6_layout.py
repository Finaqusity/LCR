# -*- coding: utf-8 -*-
"""图 6  按你的习惯排的单板布局（含风险标注）"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
from matplotlib import rcParams

rcParams["font.sans-serif"] = ["Microsoft YaHei"]
rcParams["axes.unicode_minus"] = False

fig, ax = plt.subplots(figsize=(11.6, 8.6), dpi=200)
ax.set_xlim(-6, 138)
ax.set_ylim(-16, 134)
ax.set_aspect("equal")
ax.axis("off")

ax.add_patch(Rectangle((0, 0), 120, 100, fc="#FBFDFF", ec="#1F3864", lw=2.2))


def z(x, y, w, h, title, sub, fc, ec, fs=10.5, subs=8.0):
    ax.add_patch(Rectangle((x, y), w, h, fc=fc, ec=ec, lw=1.5))
    ax.text(x + w / 2, y + h - 3.0, title, ha="center", va="center", fontsize=fs, weight="bold")
    if sub:
        ax.text(x + w / 2, y + h / 2 - 1.6, sub, ha="center", va="center",
                fontsize=subs, color="#333333", linespacing=1.5)


# —— 用户定的相对位置 ——
z(4, 60, 32, 36, "电源", "5V 入口 / ORing\nLDO 3.3VD / 3.3VA\n2.5V 虚地 / 1.65V", "#FFF4E5", "#B7791F")
z(4, 38, 32, 18, "Type-C + SWD", "USB 受电+串口\nSWD 5 脚排针", "#EAF3FB", "#2E5496")
z(46, 44, 28, 30, "MCU（居中）", "STM32F407VET6\nLQFP100\n晶振/去耦/VCAP 就近", "#EEF3EE", "#3E7A46")
z(46, 80, 28, 15, "OLED", "SPI/I²C 排线\n注意电荷泵噪声", "#F3EEFB", "#6B4E9B")
z(80, 68, 34, 27, "排针区（右上）", "扩展 / 逻辑分析\n继电器控制", "#F7F7F7", "#777777")
z(4, 8, 24, 22, "排针区（左下）", "调试 / 电源输出\n继电器驱动", "#F7F7F7", "#777777")
# 模拟区（正下方 + 右下，L 形）
ax.add_patch(Rectangle((32, 4), 52, 34, fc="#E8F1FA", ec="#2E5496", lw=1.6))
ax.add_patch(Rectangle((84, 4), 32, 58, fc="#E8F1FA", ec="#2E5496", lw=1.6))
ax.text(58, 34.5, "模拟区（AFE）", ha="center", va="center", fontsize=10.5, weight="bold")
ax.text(58, 26.5, "AD9833 → 低通 → 放大 + BUF634A\nI-V + Rf 六档 + 继电器\n保护环 / guard", ha="center",
        va="center", fontsize=8.2, color="#333333", linespacing=1.5)
# BNC 夹具
ax.add_patch(Rectangle((88, 8), 24, 18, fc="#FDE8E8", ec="#A33A3A", lw=1.8))
ax.text(100, 22.5, "夹具 BNC ×4", ha="center", va="center", fontsize=10, weight="bold")
ax.text(100, 14.5, "F+ / S+ / F− / S−\n外壳单点接 AGND\nM3 固定", ha="center", va="center",
        fontsize=8, color="#333333", linespacing=1.5)

# —— 标注：连线方向 ——
ax.annotate("", xy=(32, 66), xytext=(36, 66), arrowprops=dict(arrowstyle="->", lw=1.4, color="#B7791F"))
ax.annotate("", xy=(46, 52), xytext=(36, 52), arrowprops=dict(arrowstyle="->", lw=1.4, color="#2E5496"))
ax.annotate("", xy=(46, 86), xytext=(46, 80), arrowprops=dict(arrowstyle="->", lw=1.4, color="#6B4E9B"))
ax.annotate("", xy=(58, 44), xytext=(58, 38), arrowprops=dict(arrowstyle="->", lw=1.5, color="#3E7A46"))
ax.annotate("", xy=(86, 30), xytext=(84, 30), arrowprops=dict(arrowstyle="->", lw=1.5, color="#2E5496"))

# —— 风险提示 ——
anno = dict(fontsize=7.8, color="#A33A3A", linespacing=1.4)
ax.text(1.5, 124, "① MCU 转个方向：让 PA0(23)/PA1(24)/PA8 朝下、朝右，Vin/Vo/MCO 直接进模拟区，别绕半圈",
        ha="left", va="center", **anno)
ax.text(1.5, 119.5, "② OLED 的电荷泵（~13 V）放在屏背板、板端加 RC，远离右下模拟区；排线不要从模拟区上方过",
        ha="left", va="center", **anno)
ax.text(1.5, 115, "③ 模拟区做成连续的 L 形（下 + 右下），中间不要被数字排针切断；L2 地平面完整",
        ha="left", va="center", **anno)
ax.text(1.5, -3.5, "④ Type-C 的 USB 差分线沿左边缘走，USB 5 V 从左边上去，两条线都不要横穿模拟区",
        ha="left", va="center", **anno)
ax.text(1.5, -8.0, "⑤ MCU 与模拟区之间打一排地过孔（缝合）；ULN2803 与继电器线圈电流只在数字侧本地回流",
        ha="left", va="center", **anno)
ax.text(1.5, -12.5, "⑥ BNC 放板边、离 AFE ≤30 mm；四个外壳在夹具附近单点接 AGND，不要形成地环路",
        ha="left", va="center", **anno)

ax.text(60, 129.5, "按你的习惯排：电源左上、MCU 居中、OLED 在上、模拟区在下 + 右下、夹具用 4 个 BNC",
        ha="center", fontsize=12.5, weight="bold", color="#1F3864")

fig.savefig(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig6_layout.png"),
            bbox_inches="tight", facecolor="white")
print("saved fig6_layout.png")


