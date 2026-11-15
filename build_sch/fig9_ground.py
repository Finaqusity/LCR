# -*- coding: utf-8 -*-
"""图 9  地平面怎么处理：割地（错） vs 整片地 + 分区（对）"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyArrowPatch, Circle
from matplotlib import rcParams

rcParams["font.sans-serif"] = ["Microsoft YaHei"]
rcParams["axes.unicode_minus"] = False

fig, axes = plt.subplots(1, 2, figsize=(13.0, 6.4), dpi=200)
for ax in axes:
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 62)
    ax.set_aspect("equal")
    ax.axis("off")


def plane(ax, fc="#D9D9D9"):
    ax.add_patch(Rectangle((4, 4), 92, 46, fc=fc, ec="#888888", lw=1.2))


def zone(ax, x, y, w, h, label, fc, ec):
    ax.add_patch(Rectangle((x, y), w, h, fc=fc, ec=ec, lw=1.4, alpha=0.85))
    ax.text(x + w / 2, y + h - 3.2, label, ha="center", va="center", fontsize=9.2,
            weight="bold", color=ec)


def via_line(ax, x, y0, y1, n=9, color="#444444"):
    for i in range(n):
        y = y0 + (y1 - y0) * i / (n - 1)
        ax.add_patch(Circle((x, y), 0.75, fc="white", ec=color, lw=1.1, zorder=5))


# ================= 左：割地（错误） =================
ax = axes[0]
ax.set_title("错误做法：把地平面切成两块", fontsize=11.5, color="#A33A3A", weight="bold", pad=8)
plane(ax)
zone(ax, 6, 6, 40, 42, "模拟区", "#DCE9F7", "#2E5496")
zone(ax, 54, 6, 40, 42, "数字区", "#E6F0E6", "#3E7A46")
# 分割缝
ax.add_patch(Rectangle((46.6, 4), 6.8, 46, fc="#FFFFFF", ec="none", zorder=3))
ax.plot([46.6, 46.6], [4, 50], color="#A33A3A", lw=1.6, ls=(0, (4, 3)))
ax.plot([53.4, 53.4], [4, 50], color="#A33A3A", lw=1.6, ls=(0, (4, 3)))
ax.text(50, 52.4, "分割缝（无铜）", ha="center", fontsize=8.4, color="#A33A3A")
ax.text(50, 2.0, "唯一连接点：一根 0 Ω 桥", ha="center", fontsize=7.8, color="#A33A3A")
ax.add_patch(Rectangle((46.6, 24.4), 6.8, 1.8, fc="#C9A227", ec="none", zorder=4))

# 信号跨缝
ax.add_patch(FancyArrowPatch((30, 34), (72, 34), arrowstyle="-|>", mutation_scale=14,
                             lw=1.8, color="#B7791F", zorder=6))
ax.text(51, 36.2, "信号被迫跨缝", ha="center", fontsize=8.2, color="#B7791F", zorder=7)

# 回流绕行（大环路）
ax.add_patch(FancyArrowPatch((70, 30), (55, 25.2), arrowstyle="-|>", mutation_scale=12,
                             lw=1.5, color="#A33A3A", ls="--"))
ax.add_patch(FancyArrowPatch((45, 25.2), (30, 30), arrowstyle="-|>", mutation_scale=12,
                             lw=1.5, color="#A33A3A", ls="--"))
ax.text(50, 19.0, "回流只能绕到唯一连接点", ha="center", fontsize=8.2, color="#A33A3A")
ax.text(50, 14.4, "环路面积变大 → 辐射 / 敏感度 / 共阻抗耦合都变差", ha="center", fontsize=8.0, color="#A33A3A")
ax.text(50, 8.6, "跨缝的信号相当于挂了一根天线", ha="center", fontsize=7.9, color="#666666")

# ================= 右：整片地（正确） =================
ax = axes[1]
ax.set_title("正确做法：整片地 + 布局分区", fontsize=11.5, color="#2F7D32", weight="bold", pad=8)
plane(ax, "#CFCFCF")
zone(ax, 6, 6, 40, 42, "模拟区", "#DCE9F7", "#2E5496")
zone(ax, 54, 6, 40, 42, "数字区", "#E6F0E6", "#3E7A46")
# 分区边界：地过孔缝合（不挖槽）
via_line(ax, 50, 7.5, 48.5, n=10)
ax.text(50, 52.4, "边界打一排地过孔（缝合），铜不断开", ha="center", fontsize=8.4, color="#2F7D32")

# 信号穿边界，下方地完整
ax.add_patch(FancyArrowPatch((30, 34), (72, 34), arrowstyle="-|>", mutation_scale=14,
                             lw=1.8, color="#B7791F", zorder=6))

ax.text(52, 36.2, "回流紧贴走线正下方", ha="center", fontsize=8.2, color="#2F7D32", zorder=7)
ax.add_patch(FancyArrowPatch((66, 31.4), (36, 31.4), arrowstyle="-|>", mutation_scale=11,
                             lw=1.4, color="#2F7D32", ls=(0, (4, 3)), zorder=4))
ax.text(26, 27.0, "地平面完整 → 回流不用绕", ha="center", fontsize=7.9, color="#2F7D32")
ax.text(26, 21.0, "数字回流在数字区下方", ha="center", fontsize=7.9, color="#2F7D32")
ax.text(26, 17.0, "模拟回流在模拟区下方", ha="center", fontsize=7.9, color="#2F7D32")
ax.text(26, 13.0, "两者天然分开，不必切铜", ha="center", fontsize=7.9, color="#2F7D32")
ax.text(26, 8.8, "“单点”靠布局与过孔实现", ha="center", fontsize=7.9, color="#666666")

fig.text(0.5, 0.015, "本设计（4 层）：L2 整片 GND，不挖槽；模拟/数字只做“布局分区 + 电源分区 + 过孔缝合”",
         ha="center", fontsize=8.6, color="#333333")

fig.savefig(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig9_ground_split.png"),
            bbox_inches="tight", facecolor="white")
print("saved fig9_ground_split.png")



