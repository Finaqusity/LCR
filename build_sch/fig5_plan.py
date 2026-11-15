# -*- coding: utf-8 -*-
"""图 5  单板方案平面示意（120 × 100 mm，四层板）"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyArrowPatch
from matplotlib import rcParams

rcParams["font.sans-serif"] = ["Microsoft YaHei"]
rcParams["axes.unicode_minus"] = False

fig, ax = plt.subplots(figsize=(11.2, 8.0), dpi=200)
ax.set_xlim(-8, 132)
ax.set_ylim(-14, 112)
ax.set_aspect("equal")
ax.axis("off")

BOARD = dict(fc="#F7FBFF", ec="#1F3864", lw=2.2)
ANALOG = dict(fc="#E8F1FA", ec="#2E5496", lw=1.4)
POWER = dict(fc="#FFF4E5", ec="#B7791F", lw=1.4)
DIGITAL = dict(fc="#EEF3EE", ec="#3E7A46", lw=1.4)
FIXT = dict(fc="#FDE8E8", ec="#A33A3A", lw=1.6)

ax.add_patch(Rectangle((0, 0), 120, 100, **BOARD))


def zone(x, y, w, h, title, sub, st, fs=10.5):
    ax.add_patch(Rectangle((x, y), w, h, **st))
    ax.text(x + w / 2, y + h - 3.2, title, ha="center", va="center",
            fontsize=fs, weight="bold")
    if sub:
        ax.text(x + w / 2, y + h / 2 - 1.5, sub, ha="center", va="center",
                fontsize=8.2, color="#333333", linespacing=1.6)


# 电源区
zone(4, 56, 30, 40, "电源区", "5V 入口(KF301/Type-C)\nORing → LDO 3.3VD\nLDO 3.3VA + RC\n2.5V 虚地 / 1.65V",
     POWER)
# 模拟区
zone(37, 40, 40, 56, "模拟区（AFE）", "AD9833 + 低通\n反相放大 + BUF634A（TP1）\nI-V 变换（U4 + BUF634A）\nRf 六档 + 继电器\n保护环 / guard", ANALOG)
# 夹具区
zone(82, 58, 34, 38, "夹具区", "四端开尔文\nF+ S+ | F− S−\nM3 固定孔 ×2\n屏蔽罩焊盘", FIXT)
# 核心板座
zone(4, 6, 30, 28, "MCU 区（直焊）", "STM32F407VET6 · LQFP100\n8 MHz 晶振 + 去耦 / VCAP\nNRST / BOOT0 / SWD 排针", DIGITAL)
# 数字/继电器驱动
zone(37, 6, 38, 28, "数字与驱动", "ULN2803 ×1\n继电器驱动 + 续流\nSPI1 / MCO / SWD\nBOOT0 跳线", DIGITAL)
# 测试点
zone(78, 6, 38, 28, "测试点与接口", "TP1 (F+)  TP_VO\n2.5V / 5V / 3V3 / GND\nSWD 5 脚  UART 4 脚", dict(fc="#FFFFFF", ec="#555555", lw=1.2))

# 模拟/数字分界
ax.plot([0, 120], [37.0, 37.0], "--", color="#A33A3A", lw=1.6, dashes=(6, 4))
ax.text(0.8, 35.6, "模拟 / 数字分界：只在布局上分区，L2 地平面保持完整（不割地）",
        ha="left", va="center", fontsize=8.2, color="#A33A3A")
ax.text(60, 38.9, "Vin / Vo → PA0/PA1：RC 贴在 MCU 脚旁，走线 ≤30 mm 且贴地",
        ha="center", va="center", fontsize=8.2, color="#3E7A46")

# 关键连线提示
ax.annotate("", xy=(37, 46), xytext=(34, 46),
            arrowprops=dict(arrowstyle="->", lw=1.6, color="#2E5496"))
ax.text(35.5, 49.6, "激励", ha="center", fontsize=8, color="#2E5496")
ax.annotate("", xy=(82, 44), xytext=(77, 44),
            arrowprops=dict(arrowstyle="->", lw=1.6, color="#2E5496"))
ax.text(79.5, 46.4, "F+/F−", ha="center", fontsize=8, color="#2E5496")

ax.annotate("", xy=(34, 60), xytext=(34, 40),
            arrowprops=dict(arrowstyle="->", lw=1.3, color="#B7791F"))
ax.text(33.2, 52, "5V/3V3", ha="right", va="center", fontsize=8, color="#B7791F")

# 尺寸标注
ax.annotate("", xy=(0, -6), xytext=(120, -6),
            arrowprops=dict(arrowstyle="<->", lw=1.2, color="#444444"))
ax.text(60, -10, "120 mm", ha="center", fontsize=9, color="#444444")
ax.annotate("", xy=(-5, 0), xytext=(-5, 100),
            arrowprops=dict(arrowstyle="<->", lw=1.2, color="#444444"))
ax.text(-7.5, 50, "100 mm", ha="center", va="center", rotation=90, fontsize=9, color="#444444")

ax.text(60, 107, "单板方案：一块 4 层测量主板，F407VET6（LQFP100）直接焊在数字侧角落",
        ha="center", fontsize=12.5, weight="bold", color="#1F3864")
ax.text(60, 102.5, "夹具用 M3 螺栓直接固定在板边；模拟区与数字区只靠布局分区，L2 地平面完整不分割",
        ha="center", fontsize=8.8, color="#444444")

# 图例/叠层小表
ax.text(0, -13.2, "叠层：L1 器件 + 模拟走线 ｜ L2 完整 GND ｜ L3 电源分区(+5V/+3.3VD/+3.3VA/+2.5VG/+1.65V) ｜ L4 数字走线与继电器驱动",
        ha="left", fontsize=8.4, color="#333333")

fig.savefig(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig5_board_plan.png"),
            bbox_inches="tight", facecolor="white")
print("saved fig5_board_plan.png")




