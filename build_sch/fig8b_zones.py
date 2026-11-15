# -*- coding: utf-8 -*-
"""图 8b  按你的封装朝向（pin1 左下、逆时针）：PA0 在芯片右下角"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyArrowPatch
from matplotlib import rcParams

rcParams["font.sans-serif"] = ["Microsoft YaHei"]
rcParams["axes.unicode_minus"] = False

fig, ax = plt.subplots(figsize=(11.8, 9.0), dpi=200)
ax.set_xlim(-2, 136)
ax.set_ylim(-27, 92)
ax.set_aspect("equal")
ax.axis("off")

ax.add_patch(Rectangle((0, 0), 130, 88, fc="#FBFDFF", ec="#1F3864", lw=2.0))

# ==== MCU：pin1 左下、逆时针 ====
cx0, cy0, cw, ch = 46, 48, 30, 30
ax.add_patch(Rectangle((cx0, cy0), cw, ch, fc="#EEF3EE", ec="#3E7A46", lw=1.8))
ax.text(cx0 + cw / 2, cy0 + ch - 2.4, "MCU（pin1 左下，逆时针）", ha="center", fontsize=9.8, weight="bold")
ax.text(cx0 + cw / 2, cy0 + ch / 2 - 2.0, "F407VET6 · LQFP100", ha="center", va="center", fontsize=8.2, color="#333333")

# 引脚边说明





# 关键点
def pin(x, y, color, label, dx=0, dy=0, ha="left", fs=7.6):
    ax.plot([x], [y], marker="o", ms=8, color=color)
    ax.text(x + dx, y + dy, label, ha=ha, va="center", fontsize=fs, color=color, weight="bold")


pin(cx0 + cw - 1.0, cy0, "#A33A3A", "PA0(23) PA1(24)", dx=-5.0, dy=1.8, ha="center")
pin(cx0 + cw, cy0 + 5.0, "#B7791F", "PA4–PA7 (SPI1)", dx=1.2, ha="left")
pin(cx0 + cw * 0.34, cy0 + ch, "#6B4E9B", "PA8(MCO)", dx=0.9, dy=1.5, ha="left")
pin(cx0 + cw * 0.44, cy0 + ch, "#7A7A22", "PA9/PA10 → CH340", dx=0.8, dy=4.6, ha="left")
pin(cx0 + 1.0, cy0 + ch - 5.0, "#555555", "PD0–PD7 → 继电器", dx=0.9, dy=1.2, ha="left")
pin(cx0 + 1.0, cy0 + 7.0, "#555555", "PB6/PB7 → OLED", dx=0.9, dy=1.2, ha="left")

# ==== 模拟区：右下 ====
ax.add_patch(Rectangle((44, 4), 84, 34, fc="#E8F1FA", ec="#2E5496", lw=1.6))
ax.text(47, 35.2, "模拟区（AFE）· 右下", ha="left", fontsize=10, weight="bold", color="#2E5496")

def zone(x, y, w, h, title, body, fc, ec, fs=9.6, bs=7.9):
    ax.add_patch(Rectangle((x, y), w, h, fc=fc, ec=ec, lw=1.4))
    ax.text(x + w / 2, y + h - 2.3, title, ha="center", va="center", fontsize=fs, weight="bold")
    ax.text(x + w / 2, y + h / 2 - 1.0, body, ha="center", va="center", fontsize=bs,
            color="#333333", linespacing=1.5)


zone(62, 6, 26, 26, "① 调理 → ADC", "AC 耦合 + 1.65 V\n跟随 + 100 Ω/1 nF\n钳位 → PA0/PA1", "#EEF3EE", "#3E7A46")
zone(92, 6, 32, 26, "② I-V 与量程", "U4 + BUF634A\nRf 六档 + 继电器\nΣ 保护环", "#E8F1FA", "#2E5496")
zone(92, 36, 32, 14, "③ 夹具", "4 × BNC（板边）", "#FDE8E8", "#A33A3A")
zone(92, 54, 32, 30, "④ 激励", "AD9833 + 低通\n放大 + BUF634A\nTP1（F+）", "#FFF4E5", "#B7791F")
zone(6, 6, 26, 28, "⑤ 虚地与基准", "2.5 V 虚地\n1.65 V 中点\n（电源，不是信号）", "#F3EEFB", "#6B4E9B")
ax.add_patch(Rectangle((4, 50), 38, 34, fc="#F2F2F2", ec="#777777", lw=1.3))
ax.text(23, 80.4, "数字侧：电源 / CH340 / ULN2803 / 排针", ha="center", fontsize=8.6, color="#555555", weight="bold")
ax.text(23, 64, "电源入口 + LDO\nType-C + CH340\nULN2803 + 继电器\nSWD / 排针", ha="center", va="center",
        fontsize=7.8, color="#555555", linespacing=1.5)


def flow(x1, y1, x2, y2, color="#3E7A46", lw=2.1):
    ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>", mutation_scale=16,
                                 lw=lw, color=color, shrinkA=0, shrinkB=0))

flow(108, 53.4, 108, 50.4, color="#B7791F")   # 激励 → 夹具
flow(108, 35.6, 108, 32.4, color="#A33A3A")   # 夹具 → I-V
flow(91.4, 19, 88.6, 19, color="#A33A3A")
flow(69.5, 32, 69.5, cy0 - 0.4, color="#A33A3A")   # 调理 → PA0（直上）
ax.text(70.6, 41.2, "Vin / Vo ≈ 10 mm", fontsize=8, color="#A33A3A", ha="left", va="center")

notes = [
    "① 你这版封装是 pin1 在左下、逆时针编号 → PA0(23)/PA1(24) 落在芯片右下角，正对右下模拟区，Vin/Vo 只有约 10 mm。",
    "② 唯一规则：调理/ADC 区贴着 PA0，其余按“离 PA0 由近到远”排：I-V → 夹具 → 激励。激励靠夹具（F+ 驱动线短），MCO 从 PA8 绕一段过来即可。",
    "③ PA4–PA7 在右边下部 → SPI1 直接向右进 AD9833；串 33 Ω，走线包地。",
    "④ PA8 在上边偏左(1/3 处)、PA9/PA10 在上边中部 → MCO 沿板顶绕到右上（串 33–100 Ω、包地）；UART 就近上左去 CH340。",
    "⑤ PD0–PD7 在左边上段、PB6/PB7 在左边中段 → 继电器驱动与 OLED 都在左侧；这也正好对上你的“左侧排针/下方排针”习惯。",
    "⑥ 虚地/1.65 V 是电源，放模拟区最左下角；数字侧整体压左上，模拟区内只留 AFE。",
]
for i, t in enumerate(notes):
    ax.text(1.5, -7.0 - i * 3.1, t, fontsize=7.9, color="#333333", va="center")

fig.savefig(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig8b_afe_zones_pin1bl.png"),
            bbox_inches="tight", facecolor="white")
print("saved fig8b_afe_zones_pin1bl.png")




