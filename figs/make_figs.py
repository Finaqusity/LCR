# -*- coding: utf-8 -*-
"""《RLC 电桥测量装置原理详解》插图生成脚本（matplotlib, 白底, dpi=200）。

运行： python make_figs.py
输出： 同目录下 fig01_block.png ... fig08_dft.png
"""
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import (
    FancyBboxPatch, Rectangle, FancyArrowPatch, Circle, Ellipse, Arc, Polygon,
)
from matplotlib.ticker import LogLocator, NullFormatter
from matplotlib.lines import Line2D
from pathlib import Path

OUT = Path(__file__).resolve().parent

# ---------------------------------------------------------------- 统一风格
BLUE = "#1f5fa8"
ORANGE = "#d97706"
GRAY = "#666666"
DGRAY = "#3f3f3f"
LBLUE = "#9dc3e6"
PALEBLUE = "#eaf1fa"
PALEORANGE = "#fdf3e4"
PALEGRAY = "#f2f2f2"
GREEN = "#d9efd9"

plt.rcParams.update({
    "font.sans-serif": ["Microsoft YaHei", "SimHei", "DejaVu Sans"],
    "font.family": "sans-serif",
    "axes.unicode_minus": False,
    "font.size": 9.5,
    "axes.titlesize": 10.5,
    "axes.labelsize": 9.5,
    "xtick.labelsize": 9,
    "ytick.labelsize": 9,
    "legend.fontsize": 8.2,
    "figure.dpi": 200,
    "savefig.dpi": 200,
    "savefig.facecolor": "white",
    "figure.facecolor": "white",
    "axes.facecolor": "white",
    "axes.linewidth": 1.2,
    "lines.linewidth": 1.5,
    "grid.color": "#d9d9d9",
    "grid.linewidth": 0.8,
    "mathtext.default": "it",
})


def save(fig, name):
    fig.savefig(OUT / name, dpi=200, facecolor="white", bbox_inches="tight",
                pad_inches=0.06)
    plt.close(fig)


def wire(ax, pts, color=BLUE, lw=1.6, ls="-", zorder=3):
    ax.plot([p[0] for p in pts], [p[1] for p in pts], color=color, lw=lw,
            ls=ls, solid_capstyle="round", solid_joinstyle="round",
            zorder=zorder)


def dot(ax, x, y, color=BLUE, s=22, zorder=6):
    ax.scatter([x], [y], s=s, color=color, zorder=zorder, clip_on=False)


def arrow(ax, p0, p1, color=BLUE, lw=1.5, ls="-", ms=11, zorder=5):
    ax.add_patch(FancyArrowPatch(p0, p1, arrowstyle="-|>", mutation_scale=ms,
                                 color=color, lw=lw, linestyle=ls, shrinkA=0,
                                 shrinkB=0, zorder=zorder))


def elbow(ax, pts, color=BLUE, lw=1.5, ls="-", ms=11, zorder=5):
    pts = [tuple(p) for p in pts]
    if len(pts) > 2:
        ax.plot([p[0] for p in pts[:-1]], [p[1] for p in pts[:-1]], color=color,
                lw=lw, ls=ls, solid_capstyle="butt", zorder=zorder)
    ax.add_patch(FancyArrowPatch(pts[-2], pts[-1], arrowstyle="-|>",
                                 mutation_scale=ms, color=color, lw=lw,
                                 linestyle=ls, shrinkA=0, shrinkB=0,
                                 zorder=zorder))


def block(ax, x0, y0, w, h, title, sub=None, ec=BLUE, fc=PALEBLUE, tfs=9.6,
          sfs=8.2, lw=1.5, zorder=2, rounding=0.45):
    ax.add_patch(FancyBboxPatch((x0, y0), w, h,
                                boxstyle=f"round,pad=0,rounding_size={rounding}",
                                facecolor=fc, edgecolor=ec, linewidth=lw,
                                zorder=zorder))
    if sub:
        ax.text(x0 + w / 2, y0 + h * 0.69, title, ha="center", va="center",
                fontsize=tfs, fontweight="bold", color="#171717",
                zorder=zorder + 2)
        ax.text(x0 + w / 2, y0 + h * 0.31, sub, ha="center", va="center",
                fontsize=sfs, color="#333333", zorder=zorder + 2,
                linespacing=1.35)
    else:
        ax.text(x0 + w / 2, y0 + h / 2, title, ha="center", va="center",
                fontsize=tfs, fontweight="bold", color="#171717",
                zorder=zorder + 2)


# ================================================================ fig01
def fig01():
    fig = plt.figure(figsize=(14, 6.5))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 118)
    ax.set_ylim(12.5, 55.4)
    ax.axis("off")

    # ---- 模拟主链（y 33..44，中心 38.5）
    block(ax, 2, 33, 13, 11, "正弦发生器",
          "(DDS 或 DAC+DMA)\n100 Hz / 1 kHz\n10 kHz / 100 kHz", sfs=8.0)
    block(ax, 17.5, 33, 11, 11, "驱动缓冲放大器", "输出 0.5 Vrms\n低输出阻抗",
          tfs=9.0)
    block(ax, 58.5, 33, 15, 11, "I-V 变换器",
          "反相输入端为虚地\n$V_G=2.5$ V；自动平衡", sfs=8.0)
    block(ax, 77.5, 33, 11, 11, "程控增益", "量程网络\n（自动选档）", tfs=9.4)
    block(ax, 91, 30.5, 9.5, 14, "双通道\n同步 ADC", "CH1：$V_{in}$\nCH2：$V_o$",
          tfs=9.2, sfs=8.4)
    block(ax, 46, 47.2, 24, 4.8, "主控 MCU", "（DDS 控制、量程、触发、DFT）",
          ec=ORANGE, fc=PALEORANGE, tfs=9.6, sfs=8.0)
    block(ax, 105, 34.5, 13, 8, "幅相计算", "$|Z|$、$\\varphi$\n（DFT / 解调）",
          ec=GRAY, fc=PALEGRAY, tfs=9.4)
    block(ax, 105, 17.5, 13, 8, "显示 + 按键", "（OLED / 串口）", ec=GRAY,
          fc=PALEGRAY, tfs=9.4)
    block(ax, 36, 21.5, 14, 7, "参考通道缓冲", "高输入阻抗", tfs=9.4)

    # ---- 节点 A：力线与感线在此汇合（四端开尔文）
    arrow(ax, (28.5, 38.5), (33.8, 38.5), ms=10)
    dot(ax, 34.0, 38.5)

    # ---- 测试夹具（四端开尔文）：力线直通，感线绕上后汇入
    ax.add_patch(FancyBboxPatch((38.5, 32.5), 15.3, 11.5,
                                boxstyle="round,pad=0,rounding_size=0.45",
                                facecolor=PALEBLUE, edgecolor=BLUE,
                                linewidth=1.5, zorder=2))
    ax.text(46.15, 43.0, "测试夹具（四端开尔文）", ha="center", va="center",
            fontsize=9.2, fontweight="bold", color="#171717", zorder=4)
    ax.text(46.15, 41.7, "Force / Sense 四线分离", ha="center", va="center",
            fontsize=8.0, color="#333333", zorder=4)
    ax.add_patch(Rectangle((45.3, 36.8), 4.0, 3.4, facecolor="white",
                           edgecolor=BLUE, lw=1.5, zorder=4))
    ax.text(47.3, 38.5, "$Z_x$", ha="center", va="center", fontsize=10.5,
            zorder=5)
    # 力线：节点 A 直通 Z_x 左端、Z_x 右端直通 I-V
    wire(ax, [(34.0, 38.5), (45.3, 38.5)])
    wire(ax, [(49.3, 38.5), (55.5, 38.5)])
    # 感线：自节点 A 绕上方，到器件端子处汇入力线
    wire(ax, [(34.0, 38.5), (34.0, 40.9), (44.1, 40.9), (44.1, 38.5)])
    wire(ax, [(50.5, 38.5), (50.5, 40.9), (55.5, 40.9), (55.5, 38.5)])
    dot(ax, 44.1, 38.5)
    dot(ax, 50.5, 38.5)
    dot(ax, 55.5, 38.5)
    arrow(ax, (55.5, 38.5), (58.5, 38.5), ms=10)

    # 节点标注
    ax.text(31.4, 41.6, "A（TP1）", ha="center", va="center", fontsize=8.0,
            color=DGRAY, zorder=6)
    ax.text(31.4, 40.05, "$V_{in}$", ha="center", va="center", fontsize=8.8,
            color=DGRAY, zorder=6)
    # 参考通道：A -> 缓冲 -> ADC CH1
    elbow(ax, [(34.0, 38.5), (34.0, 25), (36, 25)], ms=10)
    elbow(ax, [(50, 25), (94.0, 25), (94.0, 30.45)], ms=10)
    ax.text(57, 25.8, "CH1：$V_{in}$（参考通道）", ha="left", va="center",
            fontsize=8.4, color=BLUE)

    # 主链其余连线
    arrow(ax, (15, 38.5), (17.5, 38.5), ms=10)
    arrow(ax, (73.5, 38.5), (77.5, 38.5), ms=10)
    dot(ax, 75.5, 38.5)
    ax.text(75.5, 40.3, "B（$V_o$）", ha="center", va="center", fontsize=8.2,
            color=DGRAY, zorder=6)
    arrow(ax, (88.5, 38.5), (91, 38.5), ms=10)
    # ADC -> 幅相计算
    arrow(ax, (100.5, 38.5), (105, 38.5), ms=10)
    ax.text(102.75, 40.2, "采样序列", ha="center", va="center", fontsize=8.0,
            color=DGRAY)

    # ---- MCU 控制（灰虚线）与数据（灰实线）
    elbow(ax, [(46, 49.5), (9, 49.5), (9, 44.3)], color=GRAY,
          ls=(0, (4, 2.6)), ms=11)
    ax.text(24, 50.15, "频率控制", ha="center", va="center", fontsize=8.4,
            color=DGRAY,
            bbox=dict(boxstyle="round,pad=0.18", fc="white", ec="none"),
            zorder=7)
    elbow(ax, [(58.5, 47.2), (58.5, 45.6), (83, 45.6), (83, 44.3)], color=GRAY,
          ls=(0, (4, 2.6)), ms=11)
    ax.text(70.5, 46.15, "量程控制", ha="center", va="center", fontsize=8.4,
            color=DGRAY,
            bbox=dict(boxstyle="round,pad=0.15", fc="white", ec="none"),
            zorder=7)
    elbow(ax, [(70, 50.5), (95.75, 50.5), (95.75, 44.55)], color=GRAY,
          ls=(0, (4, 2.6)), ms=11)
    ax.text(83, 51.2, "触发 / 采集", ha="center", va="center", fontsize=8.4,
            color=DGRAY,
            bbox=dict(boxstyle="round,pad=0.18", fc="white", ec="none"),
            zorder=7)
    # ADC 采样数据 -> MCU
    elbow(ax, [(93.5, 44.5), (93.5, 48.7), (70, 48.7)], color=DGRAY, lw=1.3, ms=10)
    ax.text(80, 48.15, "采样数据", ha="center", va="top", fontsize=8.4,
            color=DGRAY,
            bbox=dict(boxstyle="round,pad=0.15", fc="white", ec="none"),
            zorder=7)
    # 幅相计算 -> MCU
    elbow(ax, [(111.5, 42.5), (111.5, 53.4), (68, 53.4), (68, 52.1)],
          color=DGRAY, lw=1.3, ms=10)
    ax.text(89, 54.15, "$|Z|$、$\\varphi$ 结果", ha="center", va="center",
            fontsize=8.4, color=DGRAY,
            bbox=dict(boxstyle="round,pad=0.15", fc="white", ec="none"),
            zorder=7)
    # 幅相计算 -> 显示
    arrow(ax, (111.5, 34.5), (111.5, 25.8), color=DGRAY, lw=1.3, ms=10)
    ax.text(110.2, 30.2, "幅相结果", ha="center", va="center", fontsize=8.2,
            color=DGRAY, rotation=90)

    # 公式
    ax.text(56, 14.6,
            "CH1 测 $V_{in}$，CH2 测 $V_o$，因此   "
            "$Z_x=-Z_f\\cdot\\dfrac{V_{in}}{V_o}$（复数比）",
            ha="center", va="center", fontsize=10.5, color="#171717")
    save(fig, "fig01_block.png")


# ================================================================ fig02
def fig02():
    fig = plt.figure(figsize=(9, 5.5))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 90)
    ax.set_ylim(1.8, 56.8)
    ax.axis("off")

    # 交流源 V_s：一端接 V_REF，一端接节点 A
    ax.add_patch(Circle((7, 44), 2.6, facecolor="white", edgecolor=BLUE,
                        lw=1.6, zorder=4))
    tt = np.linspace(-1.7, 1.7, 100)
    ax.plot(7 + tt, 44 + 0.75 * np.sin(tt / 1.7 * np.pi), color=BLUE, lw=1.4,
            zorder=5)
    ax.text(7, 47.9, "$V_s$", ha="center", va="center", fontsize=10.5)
    ax.text(7, 39.9, "0.5 Vrms，$f$", ha="center", va="center", fontsize=8.6,
            color=DGRAY)
    wire(ax, [(9.6, 44), (12.6, 44)])
    wire(ax, [(4.4, 44), (3.2, 44), (3.2, 24)])
    dot(ax, 12.6, 44)
    ax.text(10.0, 45.6, "节点 A", ha="left", va="center", fontsize=8.8,
            color=DGRAY)

    # 被测阻抗 Z_x
    wire(ax, [(12.6, 44), (16.5, 44)])
    ax.add_patch(Rectangle((16.5, 41.6), 8.0, 4.8, facecolor="white",
                           edgecolor=BLUE, lw=1.6, zorder=4))
    ax.text(20.5, 44, "$Z_x$", ha="center", va="center", fontsize=10.5,
            zorder=5)
    arrow(ax, (17.2, 47.4), (23.8, 47.4), ms=10)
    ax.text(20.5, 48.35, "$i_x$", ha="center", va="center", fontsize=10,
            color=BLUE)
    wire(ax, [(24.5, 44), (31, 44)])
    dot(ax, 31, 44)
    ax.text(30.8, 42.1, "B（虚地）", ha="right", va="center", fontsize=8.6,
            color=DGRAY)

    # 运放
    ax.add_patch(Polygon([[38, 32], [38, 48], [52, 40]], closed=True,
                         facecolor="white", edgecolor=BLUE, lw=1.7, zorder=4))
    ax.text(40.2, 44.4, "$-$", ha="center", va="center", fontsize=13, zorder=5)
    ax.text(40.2, 35.6, "$+$", ha="center", va="center", fontsize=11.5,
            zorder=5)
    wire(ax, [(31, 44), (38, 44)])
    wire(ax, [(38, 36), (33, 36), (33, 24)])
    # 反馈 Z_f
    wire(ax, [(31, 44), (31, 51), (36, 51)])
    ax.add_patch(Rectangle((36, 49.0), 8.0, 4.0, facecolor="white",
                           edgecolor=BLUE, lw=1.6, zorder=4))
    ax.text(40, 51, "$Z_f$", ha="center", va="center", fontsize=10.5, zorder=5)
    wire(ax, [(44, 51), (58, 51), (58, 40)])
    wire(ax, [(52, 40), (66, 40)])
    dot(ax, 58, 40)
    ax.text(61.0, 42.1, "$V_o$", ha="left", va="center", fontsize=10.5)
    ax.add_patch(Circle((66, 40), 0.6, facecolor="white", edgecolor=BLUE,
                        lw=1.5, zorder=5))
    ax.text(67.3, 40, "→ 程控增益 / ADC", ha="left", va="center",
            fontsize=8.6, color=DGRAY)

    # V_REF 参考轨
    wire(ax, [(3.2, 24), (33, 24)])
    ax.text(8.0, 22.2, "$V_{REF}=2.5$ V（单电源中点，AC 参考地）", ha="left",
            va="center", fontsize=8.6, color=DGRAY)

    # 单电源 +5 V
    wire(ax, [(40, 33.2), (40, 31.4)])
    ax.plot([38.3, 41.7], [31.4, 31.4], color=BLUE, lw=2.0)
    ax.text(40, 30.0, "+5 V", ha="center", va="center", fontsize=9.0)

    # 自动平衡虚线框
    ax.add_patch(FancyBboxPatch((34.5, 26.0), 30.0, 30.2,
                                boxstyle="round,pad=0,rounding_size=1.2",
                                facecolor="none", edgecolor=GRAY, lw=1.3,
                                linestyle=(0, (5, 3)), zorder=2))
    ax.text(49.5, 55.2, "自动平衡（虚地维持）", ha="center", va="center",
            fontsize=9.0, color=DGRAY, zorder=6,
            bbox=dict(boxstyle="round,pad=0.18", fc="white", ec="none"))

    # 公式（两列排布）
    fs = 9.6
    ax.text(4, 16.8, r"$V_B\approx V_{REF}$（虚地，$V_{REF}=2.5$ V）",
            ha="left", va="center", fontsize=fs)
    ax.text(52, 16.8, r"$V_o-V_{REF}=-i_x\,Z_f$", ha="left", va="center",
            fontsize=fs)
    ax.text(4, 10.2, r"$i_x=(V_s-V_{REF})/Z_x$", ha="left", va="center",
            fontsize=fs)
    ax.text(52, 9.2,
            r"$\Rightarrow\ \ Z_x=-Z_f\cdot\dfrac{V_s-V_{REF}}{V_o-V_{REF}}$",
            ha="left", va="center", fontsize=fs)
    ax.text(4, 3.2, "注：式中 $V_s$ 取节点 A 的复电压，$V_{REF}$ 为 2.5 V 参考。",
            ha="left", va="center", fontsize=8.2, color=DGRAY)
    save(fig, "fig02_abb.png")


# ================================================================ fig03
def fig03():
    fig, axes = plt.subplots(1, 3, figsize=(13, 4.6))
    fig.subplots_adjust(left=0.02, right=0.98, top=0.92, bottom=0.04,
                        wspace=0.10)
    y0 = -1.10          # 复平面原点
    rx, ry = 1.48, 1.12  # 用于把圆弧按显示比例修正

    def draw_axes(ax):
        ax.set_xlim(-1.45, 1.45)
        ax.set_ylim(-2.12, 1.16)
        ax.axis("off")
        arrow(ax, (-0.62, y0), (1.33, y0), color="#555555", lw=1.15, ms=9)
        arrow(ax, (0, -2.02), (0, 0.02), color="#555555", lw=1.15, ms=9)
        ax.text(1.36, y0 - 0.12, "Re", ha="left", va="center", fontsize=9,
                color="#333333")
        ax.text(-0.06, 0.02, "Im", ha="right", va="bottom", fontsize=9,
                color="#333333")
        dot(ax, 0, y0, color="#333333", s=13)

    # ---------- R
    ax = axes[0]
    draw_axes(ax)
    ax.set_title("电阻 R", fontsize=11, fontweight="bold", pad=4)
    zig = [(x, 0.86 + (0.12 if i % 2 == 0 else -0.12))
           for i, x in enumerate(np.linspace(-0.36, 0.36, 8))]
    ax.plot([-0.66, -0.36], [0.86, 0.86], color=BLUE, lw=1.7)
    ax.plot([p[0] for p in zig], [p[1] for p in zig], color=BLUE, lw=1.7,
            solid_joinstyle="miter")
    ax.plot([0.36, 0.66], [0.86, 0.86], color=BLUE, lw=1.7)
    ax.text(0, 0.48, "$Z=R$", ha="center", va="center", fontsize=10.5)
    ax.text(0, 0.28, "$|Z|=R$，$\\varphi=0^\\circ$（V 与 I 同相）",
            ha="center", va="center", fontsize=9.2)
    arrow(ax, (0, y0), (1.06, y0), color=BLUE, lw=2.4, ms=12)
    ax.text(1.10, y0 + 0.12, "$Z$", ha="left", va="center", fontsize=10.5,
            color=BLUE)
    arrow(ax, (0, y0 - 0.26), (0.62, y0 - 0.26), color=ORANGE, lw=1.6, ms=10)
    ax.text(0.66, y0 - 0.26, "$I$", ha="left", va="center", fontsize=10,
            color=ORANGE)

    # ---------- L
    ax = axes[1]
    draw_axes(ax)
    ax.set_title("电感 L", fontsize=11, fontweight="bold", pad=4)
    ax.plot([-0.66, -0.36], [0.86, 0.86], color=BLUE, lw=1.7)
    for i in range(4):
        ax.add_patch(Arc((-0.36 + 0.18 * i + 0.09, 0.86), 0.18, 0.20,
                         theta1=0, theta2=180, color=BLUE, lw=1.7))
    ax.plot([0.36, 0.66], [0.86, 0.86], color=BLUE, lw=1.7)
    ax.text(0, 0.48, "$Z=j\\omega L$", ha="center", va="center", fontsize=10.5)
    ax.text(0, 0.28, "$|Z|=\\omega L$，$\\varphi=+90^\\circ$（V 超前 I）",
            ha="center", va="center", fontsize=9.2)
    arrow(ax, (0, y0), (0, -0.14), color=BLUE, lw=2.4, ms=12)
    ax.text(-0.12, -0.62, "$Z$", ha="right", va="center", fontsize=10.5,
            color=BLUE)
    arrow(ax, (0, y0), (0.62, y0), color=ORANGE, lw=1.6, ms=10)
    ax.text(0.66, y0 - 0.10, "$I$", ha="left", va="center", fontsize=10,
            color=ORANGE)
    ax.add_patch(Arc((0, y0), 0.68, 0.68 * ry / rx, theta1=0, theta2=90,
                     color=GRAY, lw=1.2, linestyle=(0, (3, 2))))
    ax.text(0.30, -0.60, "$\\varphi$", ha="center", va="center", fontsize=10,
            color=DGRAY)

    # ---------- C
    ax = axes[2]
    draw_axes(ax)
    ax.set_title("电容 C", fontsize=11, fontweight="bold", pad=4)
    ax.plot([-0.66, -0.10], [0.86, 0.86], color=BLUE, lw=1.7)
    ax.plot([0.10, 0.66], [0.86, 0.86], color=BLUE, lw=1.7)
    ax.plot([-0.10, -0.10], [0.68, 1.04], color=BLUE, lw=2.3)
    ax.plot([0.10, 0.10], [0.68, 1.04], color=BLUE, lw=2.3)
    ax.text(0, 0.48, "$Z=-j/(\\omega C)$", ha="center", va="center",
            fontsize=10.5)
    ax.text(0, 0.28, "$|Z|=1/(\\omega C)$，$\\varphi=-90^\\circ$（I 超前 V）",
            ha="center", va="center", fontsize=9.2)
    arrow(ax, (0, y0), (0, -1.94), color=BLUE, lw=2.4, ms=12)
    ax.text(-0.12, -1.80, "$Z$", ha="right", va="center", fontsize=10.5,
            color=BLUE)
    arrow(ax, (0, y0), (0.62, y0), color=ORANGE, lw=1.6, ms=10)
    ax.text(0.66, y0 - 0.10, "$I$", ha="left", va="center", fontsize=10,
            color=ORANGE)
    ax.add_patch(Arc((0, y0), 0.68, 0.68 * ry / rx, theta1=-90, theta2=0,
                     color=GRAY, lw=1.2, linestyle=(0, (3, 2))))
    ax.text(0.30, -1.58, "$\\varphi$", ha="center", va="center", fontsize=10,
            color=DGRAY)
    save(fig, "fig03_phasor.png")


# ================================================================ fig04
def fig04():
    fig, ax = plt.subplots(figsize=(11, 7))
    fig.subplots_adjust(left=0.085, right=0.80, top=0.895, bottom=0.10)
    f = np.logspace(1, 6, 500)

    caps = [("100 pF", 100e-12), ("1 nF", 1e-9), ("10 nF", 10e-9),
            ("100 nF", 100e-9), ("1 $\\mu$F", 1e-6), ("10 $\\mu$F", 10e-6)]
    inds = [("10 $\\mu$H", 10e-6), ("100 $\\mu$H", 100e-6), ("1 mH", 1e-3),
            ("10 mH", 10e-3), ("100 mH", 100e-3)]
    cap_colors = ["#0b3d70", "#1f5fa8", "#3f7fbf", "#6aa3d2", "#95bde0",
                  "#b9d3ea"]
    ind_colors = ["#7c3a03", "#a04a05", "#c05f08", "#d97706", "#e89a2b"]
    cap_lx = [1.3e5] * 5 + [6.0e4]
    ind_lx = 1.15e5

    ax.axhspan(100, 1e5, color=GREEN, alpha=0.85, zorder=0, lw=0)
    ax.text(12, 2.2e4, "适合测量窗口\n100 $\\Omega$ – 100 k$\\Omega$",
            ha="left", va="center", fontsize=8.8, color="#2f6b2f", zorder=4,
            bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="none",
                      alpha=0.85))

    for (lab, C), col, lx in zip(caps, cap_colors, cap_lx):
        ax.loglog(f, 1.0 / (2 * np.pi * f * C), color=col, lw=1.6, zorder=3)
    for (lab, C), col, lx in zip(caps, cap_colors, cap_lx):
        zx = 1.0 / (2 * np.pi * lx * C)
        ax.text(lx, zx * 1.45, lab, ha="left", va="bottom", fontsize=8.4,
                color=col, zorder=5)
    for (lab, L), col in zip(inds, ind_colors):
        ax.loglog(f, 2 * np.pi * f * L, color=col, lw=1.6, zorder=3)
    for (lab, L), col in zip(inds, ind_colors):
        zx = 2 * np.pi * ind_lx * L
        ax.text(ind_lx, zx / 1.45, lab, ha="left", va="top", fontsize=8.4,
                color=col, zorder=5)

    for ff, name in [(100, "100 Hz"), (1e3, "1 kHz"), (1e4, "10 kHz"),
                     (1e5, "100 kHz")]:
        ax.axvline(ff, color=GRAY, lw=1.2, ls=(0, (5, 3)), zorder=2)
        ax.text(ff, 4.5e6, name + "\n档位", ha="center", va="center",
                fontsize=8.4, color=DGRAY, zorder=6,
                bbox=dict(boxstyle="round,pad=0.18", fc="white", ec="none",
                          alpha=0.9))

    ax.set_xlim(1e1, 1e6)
    ax.set_ylim(1e-1, 1e7)
    ax.set_xlabel("频率 $f$（Hz）")
    ax.set_ylabel("阻抗模 $|Z|$（$\\Omega$）")
    ax.set_xticks([1e1, 1e2, 1e3, 1e4, 1e5, 1e6])
    ax.set_xticklabels(["10", "100", "1k", "10k", "100k", "1M"])
    ax.set_yticks([1e-1, 1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7])
    ax.set_yticklabels(["0.1", "1", "10", "100", "1k", "10k", "100k", "1M",
                        "10M"])
    for axis in (ax.xaxis, ax.yaxis):
        axis.set_minor_locator(LogLocator(base=10,
                                          subs=np.arange(2, 10) * 0.1,
                                          numticks=100))
        axis.set_minor_formatter(NullFormatter())
    ax.grid(True, which="major", color="#dddddd", lw=0.8, zorder=1)
    ax.grid(True, which="minor", color="#eeeeee", lw=0.5, zorder=0)

    handles, labels = [], []
    for (lab, C), col in zip(caps, cap_colors):
        handles.append(Line2D([], [], color=col, lw=1.8))
        labels.append("C = " + lab)
    for (lab, L), col in zip(inds, ind_colors):
        handles.append(Line2D([], [], color=col, lw=1.8))
        labels.append("L = " + lab)
    leg = ax.legend(handles, labels, loc="upper left",
                    bbox_to_anchor=(1.015, 1.0), frameon=True, fontsize=8.2,
                    handlelength=2.0, labelspacing=0.55, borderpad=0.6,
                    title="元件取值")
    leg.get_title().set_fontsize(8.6)

    ax.set_title("元件的 $|Z|$ 随频率变化 → 频率要按元件“自动挑”",
                 fontsize=11.5, pad=8)
    save(fig, "fig04_freq.png")


# ================================================================ fig05
def fig05():
    fig = plt.figure(figsize=(13, 6.5))
    gs = fig.add_gridspec(2, 3, height_ratios=[1.0, 1.05], hspace=0.34,
                          wspace=0.28, left=0.055, right=0.985, top=0.975,
                          bottom=0.10)

    # ---------------- 上排：同步解调框图
    ax = fig.add_subplot(gs[0, :])
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 30)
    ax.axis("off")

    ax.text(1.0, 28.7, "输入 $v(t)=A\\cos(\\omega t+\\varphi)$", ha="left",
            va="center", fontsize=9.5)
    ax.plot([1.0, 8.5], [15, 15], color=BLUE, lw=1.6)
    dot(ax, 8.5, 15)
    ax.text(3.6, 16.4, "$v(t)$", ha="center", va="center", fontsize=8.8)
    ax.plot([8.5, 8.5], [8, 22], color=BLUE, lw=1.6)
    ax.plot([8.5, 15], [22, 22], color=BLUE, lw=1.6)
    ax.plot([8.5, 15], [8, 8], color=BLUE, lw=1.6)

    def mult(xc, yc, ref, lab_y):
        ax.add_patch(Ellipse((xc, yc), 4.0, 5.2, facecolor="white",
                             edgecolor=BLUE, lw=1.6, zorder=4))
        ax.text(xc, yc, "$\\times$", ha="center", va="center", fontsize=13,
                zorder=5)
        arrow(ax, (xc, lab_y), (xc, yc - 2.6), color=ORANGE, lw=1.3, ms=9)
        ax.text(xc + 1.4, lab_y - 0.9, "参考 " + ref, ha="left", va="center",
                fontsize=8.4, color=DGRAY)

    mult(17, 22, "$\\cos\\omega t$", 17.2)
    mult(17, 8, "$\\sin\\omega t$", 3.2)

    arrow(ax, (19.1, 22), (24, 22), ms=10)
    arrow(ax, (19.1, 8), (24, 8), ms=10)

    for yy, txt in ((22, "低通滤波\n（LPF）"), (8, "低通滤波\n（LPF）")):
        ax.add_patch(FancyBboxPatch((24, yy - 2.5), 11, 5.0,
                                    boxstyle="round,pad=0,rounding_size=0.4",
                                    facecolor=PALEBLUE, edgecolor=BLUE,
                                    lw=1.5, zorder=4))
        ax.text(29.5, yy, txt, ha="center", va="center", fontsize=9.0,
                zorder=5, linespacing=1.35)

    arrow(ax, (35, 22), (45, 22), ms=10)
    arrow(ax, (35, 8), (45, 8), ms=10)
    ax.text(37, 23.2, "$I$（同相）", ha="left", va="center", fontsize=9.2,
            color=BLUE)
    ax.text(37, 9.2, "$Q$（正交）", ha="left", va="center", fontsize=9.2,
            color=BLUE)

    ax.add_patch(FancyBboxPatch((45, 1.0), 28, 26.0,
                                boxstyle="round,pad=0,rounding_size=0.6",
                                facecolor=PALEGRAY, edgecolor=GRAY, lw=1.5,
                                zorder=3))
    ax.text(59, 24.3, "幅相计算", ha="center", va="center", fontsize=10.2,
            fontweight="bold", zorder=5)
    ax.text(59, 20.6, r"$I=\frac{A}{2}\cos\varphi$", ha="center", va="center",
            fontsize=10.2, zorder=5)
    ax.text(59, 17.5, r"$Q=\frac{A}{2}\sin\varphi$", ha="center", va="center",
            fontsize=10.2, zorder=5)
    ax.text(59, 13.5, r"$|V|=\sqrt{I^2+Q^2}$", ha="center", va="center",
            fontsize=10.2, zorder=5)
    ax.text(59, 10.1, r"$\varphi=\mathrm{atan2}(Q,I)$", ha="center",
            va="center", fontsize=10.2, zorder=5)
    ax.text(59, 4.6, "（对 CH1、CH2 各算一次）", ha="center", va="center",
            fontsize=8.2, color=DGRAY, zorder=5)

    arrow(ax, (73, 14), (84, 14), ms=10)
    ax.text(74.5, 15.6, "$|V|$、$\\varphi$", ha="left", va="center",
            fontsize=9.6)
    ax.text(74.5, 12.0, "（两路相除得 $Z_x$）", ha="left", va="center",
            fontsize=8.4, color=DGRAY)

    # ---------------- 下排：(a) 乘法波形
    w = np.linspace(0, 4 * np.pi, 1200)
    phi = np.pi / 4
    ref = np.cos(w)
    v = np.cos(w + phi)
    prod = v * ref
    dc = 0.5 * np.cos(phi)

    ax1 = fig.add_subplot(gs[1, 0])
    ax1.plot(w, ref, color=GRAY, lw=1.1, ls=(0, (4, 2)),
             label="参考 $\\cos\\omega t$")
    ax1.plot(w, v, color=BLUE, lw=1.5,
             label="$v(t)=\\cos(\\omega t+\\varphi)$")
    ax1.plot(w, prod, color=ORANGE, lw=1.3,
             label="乘积 $v\\cdot\\cos\\omega t$")
    ax1.axhline(dc, color=ORANGE, lw=1.2, ls=(0, (1.5, 1.5)),
                label="直流 $A/2\\cos\\varphi$")
    ax1.set_xlim(0, 4 * np.pi)
    ax1.set_ylim(-1.18, 1.30)
    ax1.set_xticks([0, np.pi, 2 * np.pi, 3 * np.pi, 4 * np.pi])
    ax1.set_xticklabels(["0", "$\\pi$", "$2\\pi$", "$3\\pi$", "$4\\pi$"])
    ax1.set_xlabel("$\\omega t$（rad）", labelpad=1.5)
    ax1.set_ylabel("幅度", labelpad=1.5)
    ax1.grid(True, color="#e2e2e2", lw=0.7)
    ax1.set_title("① 乘法：2$\\omega$ 纹波 + 直流分量", fontsize=9.6, pad=4)
    ax1.legend(loc="upper right", fontsize=8.0, frameon=True, borderpad=0.35,
               handlelength=1.6, labelspacing=0.3, framealpha=0.95)

    # (b) 低通后的直流
    ax2 = fig.add_subplot(gs[1, 1])
    filt = dc * (1 - np.exp(-w / 1.1))
    ax2.plot(w, prod, color=ORANGE, lw=0.9, alpha=0.45,
             label="乘积（含 2$\\omega$ 纹波）")
    ax2.plot(w, filt, color=BLUE, lw=2.0, label="低通输出 → 直流 $I$")
    ax2.axhline(dc, color=BLUE, lw=1.0, ls=(0, (4, 3)))
    ax2.set_xlim(0, 4 * np.pi)
    ax2.set_ylim(-1.18, 1.30)
    ax2.set_xticks([0, np.pi, 2 * np.pi, 3 * np.pi, 4 * np.pi])
    ax2.set_xticklabels(["0", "$\\pi$", "$2\\pi$", "$3\\pi$", "$4\\pi$"])
    ax2.set_xlabel("$\\omega t$（rad）", labelpad=1.5)
    ax2.grid(True, color="#e2e2e2", lw=0.7)
    ax2.set_title("② 低通后只剩 $I=\\frac{A}{2}\\cos\\varphi$", fontsize=9.6,
                  pad=4)
    ax2.legend(loc="upper left", fontsize=8.0, frameon=True, borderpad=0.35,
               handlelength=1.6, labelspacing=0.3, framealpha=0.95)

    # (c) 复平面
    ax3 = fig.add_subplot(gs[1, 2])
    ax3.set_xlim(-0.16, 1.20)
    ax3.set_ylim(-0.20, 1.18)
    ax3.set_aspect("equal")
    ax3.axis("off")
    arrow(ax3, (-0.06, 0), (1.14, 0), color="#555555", lw=1.1, ms=9)
    arrow(ax3, (0, -0.08), (0, 1.14), color="#555555", lw=1.1, ms=9)
    ax3.text(1.06, -0.10, "Re（同相）", ha="right", va="top", fontsize=8.4,
             color="#333333")
    ax3.text(-0.05, 1.10, "Im（正交）", ha="left", va="top", fontsize=8.4,
             color="#333333")
    ax3.annotate("", xy=(1.0 * np.cos(phi), 1.0 * np.sin(phi)), xytext=(0, 0),
                 arrowprops=dict(arrowstyle="-|>", color=BLUE, lw=2.4,
                                 shrinkA=0, shrinkB=0, mutation_scale=12))
    ax3.text(1.0 * np.cos(phi) + 0.04, 1.0 * np.sin(phi) + 0.04, "$A$",
             fontsize=10.5, color=BLUE)
    ax3.plot([np.cos(phi), np.cos(phi)], [0, np.sin(phi)], color=GRAY, lw=1.1,
             ls=(0, (3, 2)))
    ax3.plot([0, np.cos(phi)], [np.sin(phi), np.sin(phi)], color=GRAY, lw=1.1,
             ls=(0, (3, 2)))
    ax3.annotate("", xy=(np.cos(phi), 0), xytext=(0, 0),
                 arrowprops=dict(arrowstyle="-|>", color=ORANGE, lw=2.2,
                                 shrinkA=0, shrinkB=0, mutation_scale=11))
    ax3.annotate("", xy=(0, np.sin(phi)), xytext=(0, 0),
                 arrowprops=dict(arrowstyle="-|>", color=ORANGE, lw=2.2,
                                 shrinkA=0, shrinkB=0, mutation_scale=11))
    ax3.text(np.cos(phi) / 2, -0.10, "$I=A\\cos\\varphi$", ha="center",
             va="top", fontsize=8.6, color=ORANGE)
    ax3.text(0.045, np.sin(phi) / 2, "$Q=A\\sin\\varphi$", ha="left",
             va="center", fontsize=8.6, color=ORANGE, rotation=90)
    ax3.add_patch(Arc((0, 0), 0.44, 0.44, theta1=0, theta2=45, color=GRAY,
                      lw=1.1))
    ax3.text(0.27, 0.12, "$\\varphi$", fontsize=9.4, color=DGRAY)
    ax3.set_title("③ 复平面：I、Q 是 $A$ 的投影", fontsize=9.6, pad=4)
    save(fig, "fig05_demod.png")


# ================================================================ fig06
def fig06():
    fig, (axl, axr) = plt.subplots(1, 2, figsize=(12, 5.5),
                                   gridspec_kw=dict(width_ratios=[1.0, 1.05],
                                                    wspace=0.16))
    fig.subplots_adjust(left=0.025, right=0.985, top=0.855, bottom=0.075)
    fig.suptitle("高频小电容为什么必须校准：几 pF 杂散就带来 10% 量级的误差",
                 fontsize=11.5, y=0.985)

    # ---------------- (a) 误差模型
    ax = axl
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 12.6)
    ax.axis("off")
    ax.set_title("（a）误差模型：$Z_m=Z_s+(Z_x\\parallel Z_p)$",
                 fontsize=10.2, pad=6)

    ax.add_patch(Circle((5, 11.65), 0.15, facecolor="white", edgecolor=BLUE,
                        lw=1.4, zorder=5))
    ax.text(5.35, 11.65, "端口 1", ha="left", va="center", fontsize=8.4,
            color=DGRAY)
    wire(ax, [(5, 11.5), (5, 10.65)])
    ax.add_patch(Rectangle((4.1, 9.55), 1.8, 1.1, facecolor="white",
                           edgecolor=BLUE, lw=1.5, zorder=4))
    ax.text(6.15, 10.1, "$Z_s$", ha="left", va="center", fontsize=9.8)
    ax.text(6.15, 9.35, "引线、接触电阻", ha="left", va="center", fontsize=8.0,
            color=DGRAY)
    wire(ax, [(5, 9.55), (5, 8.75)])
    dot(ax, 5, 8.75)
    wire(ax, [(5, 8.75), (3.5, 8.75), (3.5, 7.45)])
    wire(ax, [(5, 8.75), (6.7, 8.75), (6.7, 7.45)])

    ax.add_patch(Rectangle((2.85, 6.25), 1.7, 1.2, facecolor="white",
                           edgecolor=BLUE, lw=1.5, zorder=4))
    ax.text(2.7, 6.9, "$Z_x$", ha="right", va="center", fontsize=9.8)
    ax.text(2.7, 5.95, "真实被测件", ha="right", va="center", fontsize=8.0,
            color=DGRAY)
    ax.add_patch(Rectangle((5.85, 6.25), 1.7, 1.2, facecolor="white",
                           edgecolor=BLUE, lw=1.5, zorder=4))
    ax.text(7.75, 6.9, "$Z_p$", ha="left", va="center", fontsize=9.8)
    ax.text(7.75, 5.95, "夹具杂散、输入电容", ha="left", va="center",
            fontsize=8.0, color=DGRAY)

    wire(ax, [(3.5, 6.25), (3.5, 5.25), (5, 5.25)])
    wire(ax, [(6.7, 6.25), (6.7, 5.25), (5, 5.25)])
    dot(ax, 5, 5.25)
    wire(ax, [(5, 5.25), (5, 4.85)])
    ax.add_patch(Circle((5, 4.7), 0.15, facecolor="white", edgecolor=BLUE,
                        lw=1.4, zorder=5))
    ax.text(5.35, 4.7, "端口 2", ha="left", va="center", fontsize=8.4,
            color=DGRAY)

    ax.annotate("", xy=(9.55, 11.55), xytext=(9.55, 4.85),
                arrowprops=dict(arrowstyle="<|-|>", color=GRAY, lw=1.3,
                                mutation_scale=10))
    ax.text(9.95, 8.2, "$Z_m$（V/I，仪表读数）", rotation=90, ha="center",
            va="center", fontsize=8.2, color=DGRAY)

    ax.text(0.25, 3.55, r"$Z_m=Z_s+(Z_x\parallel Z_p)$", ha="left",
            va="center", fontsize=9.4)
    ax.text(0.25, 2.62, r"短路（$Z_x=0$）：$Z_s=Z_{short}$", ha="left",
            va="center", fontsize=9.2)
    ax.text(0.25, 1.72, r"开路（$Z_x\to\infty$）：$Z_p=Z_{open}$", ha="left",
            va="center", fontsize=9.2)
    ax.text(0.25, 0.66,
            r"$\Rightarrow\ Z_x=\dfrac{(Z_m-Z_s)\,Z_p}{Z_p-(Z_m-Z_s)}$",
            ha="left", va="center", fontsize=9.6)

    # ---------------- (b) 100 kHz 测 10 pF
    ax = axr
    cs = np.linspace(0.5, 3.0, 26)
    err_un = 10.0 * cs
    err_cal = 0.52 + 0.055 * cs + 0.03 * np.sin(cs * 4)
    ax.plot(cs, err_un, color=ORANGE, lw=2.0, marker="o", ms=3.4,
            label="未校准（误差 ≈ $C_{stray}/C_x$）")
    ax.plot(cs, err_cal, color=BLUE, lw=2.2, marker="s", ms=3.2,
            label="开路 + 短路校准后")
    ax.annotate("1 pF 杂散 → 误差约 10%", xy=(1.02, 10.2), xytext=(1.30, 21.0),
                fontsize=8.6, color=DGRAY,
                arrowprops=dict(arrowstyle="->", color=GRAY, lw=1.2,
                                shrinkA=2, shrinkB=2))
    ax.text(2.12, 3.0, "校准后残余 < 1%", ha="left", va="center",
            fontsize=8.4, color=BLUE)
    ax.set_xlim(0.45, 3.05)
    ax.set_ylim(0, 32)
    ax.set_xlabel("夹具杂散电容 $C_{stray}$（pF）")
    ax.set_ylabel("相对误差（%）")
    ax.set_title("（b）100 kHz 下测 10 pF 电容", fontsize=10.2, pad=6)
    ax.grid(True, color="#e3e3e3", lw=0.8)
    ax.set_axisbelow(True)
    ax.legend(loc="upper left", fontsize=8.0, frameon=True, framealpha=0.95)
    save(fig, "fig06_calib.png")


# ================================================================ fig07
def fig07():
    fig = plt.figure(figsize=(12, 5.2))
    ax = fig.add_axes([0.115, 0.105, 0.865, 0.795])
    ax.set_xlim(0, 135)
    ax.set_ylim(0, 10.6)

    C_SW, C_STB, C_SMP = "#b0b0b0", LBLUE, BLUE
    C_DFT, C_UI = ORANGE, "#8c8c8c"

    y1, y2, bh = 6.6, 2.6, 0.85
    row1 = [(0, 1, C_SW), (1, 6, C_STB), (6, 106, C_SMP), (106, 116, C_DFT),
            (116, 121, C_UI)]
    row2 = [(0, 1, C_SW), (1, 6, C_STB), (6, 6.1, C_SMP), (6.1, 16.1, C_DFT),
            (16.1, 21.1, C_UI)]
    for y0, segs in ((y1, row1), (y2, row2)):
        for x0, x1, col in segs:
            ax.add_patch(Rectangle((x0, y0), x1 - x0, bh, facecolor=col,
                                   edgecolor="white", lw=0.8, zorder=3))

    ax.text(56, y1 + bh / 2, "采样：10 个周期 = 100 ms", ha="center",
            va="center", fontsize=9.0, color="white", zorder=4)
    ax.text(111, y1 + bh / 2, "DFT\n10 ms", ha="center", va="center",
            fontsize=8.0, color="white", zorder=4, linespacing=1.25)
    ax.text(11.1, y2 + bh / 2, "DFT ≈ 10 ms", ha="center", va="center",
            fontsize=8.0, color="white", zorder=4)

    def note(xy, xytext, text, ha="left", fs=8.2):
        ax.annotate(text, xy=xy, xytext=xytext, ha=ha, va="center",
                    fontsize=fs, color=DGRAY,
                    arrowprops=dict(arrowstyle="-|>", color=GRAY, lw=0.9,
                                    shrinkA=2, shrinkB=1))

    # 100 Hz 档（标注在条上方）
    note((0.6, y1 + bh + 0.03), (0.2, 9.55), "频率/量程切换 1 ms")
    note((2.6, y1 + bh + 0.03), (1.5, 8.85), "激励稳定 + 滤波器建立 5 ms")
    note((118.5, y1 + bh + 0.03), (114.0, 8.85), "显示 / 通信 5 ms")
    # 100 kHz 档（标注在两条之间）
    note((0.6, y2 + bh + 0.03), (0.2, 6.30), "频率/量程切换 1 ms")
    note((3.3, y2 + bh + 0.03), (1.5, 5.65), "激励稳定 + 滤波器建立 5 ms")
    note((6.1, y2 + bh + 0.03), (9.2, 4.95), "采样 10 个周期 = 0.1 ms")
    note((21.3, y2 + bh / 2), (23.0, y2 + bh / 2), "显示 / 通信 5 ms")

    # 总时长
    ax.annotate("", xy=(0, 10.15), xytext=(121, 10.15),
                arrowprops=dict(arrowstyle="<|-|>", color=BLUE, lw=1.4,
                                mutation_scale=10))
    ax.text(60.5, 10.28,
            "100 Hz 档合计 121 ms ≈ 0.12 s，远小于 1 s 采集上限（1 s 在画面外）",
            ha="center", va="bottom", fontsize=8.8, color=BLUE)
    ax.annotate("", xy=(0, 1.95), xytext=(21.1, 1.95),
                arrowprops=dict(arrowstyle="<|-|>", color=BLUE, lw=1.3,
                                mutation_scale=9))
    ax.text(23.2, 1.95, "100 kHz 档合计 ≈ 21 ms", ha="left", va="center",
            fontsize=8.6, color=BLUE)

    ax.set_yticks([y1 + bh / 2, y2 + bh / 2])
    ax.set_yticklabels(["100 Hz 档", "100 kHz 档"], fontsize=9.5)
    ax.tick_params(axis="y", length=0, pad=6)
    ax.set_xticks([0, 20, 40, 60, 80, 100, 120])
    ax.set_xticklabels(["0", "20", "40", "60", "80", "100", "120"])
    ax.set_xlabel("时间（ms）", labelpad=2)
    for sp in ("top", "right", "left"):
        ax.spines[sp].set_visible(False)
    ax.spines["bottom"].set_linewidth(1.0)
    ax.set_title("单次测量时序：100 Hz 档 ≈ 0.12 s，100 kHz 档 ≈ 21 ms",
                 fontsize=11.0, pad=10)
    save(fig, "fig07_timing.png")


# ================================================================ fig08
def fig08():
    fig, (axl, axr) = plt.subplots(1, 2, figsize=(12, 5),
                                   gridspec_kw=dict(wspace=0.22))
    fig.subplots_adjust(left=0.07, right=0.985, top=0.88, bottom=0.19)

    # ---------------- (a) 相干采样示意
    M, N = 3, 24
    t = np.linspace(0, M, 2000)
    ts = M * np.arange(N) / N
    axl.plot(t, np.sin(2 * np.pi * t), color=GRAY, lw=1.2)
    axl.plot(ts, np.sin(2 * np.pi * ts), "o", color=BLUE, ms=4.4)
    for xv in (0, M):
        axl.axvline(xv, color=GRAY, lw=1.1, ls=(0, (4, 3)), zorder=2)
    axl.annotate("", xy=(0, 1.38), xytext=(M, 1.38),
                 arrowprops=dict(arrowstyle="<|-|>", color=BLUE, lw=1.2,
                                 mutation_scale=9))
    axl.text(M / 2, 1.44, "窗口：$M=3$ 个整周期", ha="center", va="bottom",
             fontsize=8.8, color=BLUE)
    axl.plot([0.05, 0.20], [1.16, 1.16], color=GRAY, lw=1.2)
    axl.text(0.25, 1.16, "连续信号 $v(t)$", va="center", fontsize=8.2,
             color=DGRAY)
    axl.plot([0.98], [1.16], "o", color=BLUE, ms=4.4)
    axl.text(1.04, 1.16, "采样点（$N=24$）", va="center", fontsize=8.2,
             color=DGRAY)
    axl.text(0.05, -1.30,
             "N = M·($f_s/f$) = 3×8 = 24 点；窗口内整数个周期 → "
             "相干采样无频谱泄漏", ha="left", va="center", fontsize=8.2,
             color=DGRAY)
    axl.set_xlim(0, M)
    axl.set_ylim(-1.42, 1.62)
    axl.set_xticks([0, 1, 2, 3])
    axl.set_xticklabels(["0", "$T$", "$2T$", "$3T$"])
    axl.set_xlabel("时间")
    axl.set_ylabel("幅度")
    axl.set_yticks([-1, 0, 1])
    axl.grid(True, color="#e8e8e8", lw=0.7)
    axl.set_axisbelow(True)
    axl.set_title("（a）相干采样：整数个周期", fontsize=10.2, pad=6)

    # ---------------- (b) N 点 DFT 频谱
    k = np.arange(N)
    X = np.zeros(N)
    X[3] = 1.0
    X[21] = 1.0
    axr.vlines(k, 0, X, color=BLUE, lw=1.5, zorder=3)
    axr.plot(k, X, "o", color=BLUE, ms=4.0, zorder=4)
    axr.vlines(3, 0, 1.0, color=ORANGE, lw=2.4, zorder=5)
    axr.plot([3], [1.0], "o", color=ORANGE, ms=6.5, zorder=6)
    axr.annotate("第 $M=3$ 个 bin：\n复数 $I+jQ$（实部→I，虚部→Q）",
                 xy=(3.05, 0.985), xytext=(5.2, 0.86), ha="left",
                 va="center", fontsize=8.4, color=DGRAY,
                 arrowprops=dict(arrowstyle="->", color=GRAY, lw=1.1,
                                 shrinkA=2, shrinkB=3))
    axr.annotate("$N-M=21$：共轭镜像", xy=(20.95, 0.985), xytext=(22.9, 0.62),
                 ha="right", va="center", fontsize=8.2, color=DGRAY,
                 arrowprops=dict(arrowstyle="->", color=GRAY, lw=1.0,
                                 shrinkA=2, shrinkB=3))
    axr.text(12.0, 0.15, "其余 bin ≈ 0（整周期采样无泄漏）", ha="center",
             va="center", fontsize=8.2, color=DGRAY)
    axr.set_xlim(-0.8, 23.8)
    axr.set_ylim(0, 1.22)
    axr.set_xticks([0, 3, 6, 9, 12, 15, 18, 21])
    axr.set_xlabel("离散频率 bin $k$（$f_k=k\\,f_s/N$）")
    axr.set_ylabel("归一化 $|X[k]|$")
    axr.grid(True, axis="y", color="#e8e8e8", lw=0.7)
    axr.set_axisbelow(True)
    axr.set_title("（b）只取第 $M$ 个 bin 就得到 I、Q", fontsize=10.2, pad=6)

    fig.text(0.5, 0.035,
             "$Z_x=-Z_f\\cdot\\dfrac{I_{in}+jQ_{in}}{I_o+jQ_o}$",
             ha="center", va="bottom", fontsize=12)
    save(fig, "fig08_dft.png")


def main():
    fig01()
    fig02()
    fig03()
    fig04()
    fig05()
    fig06()
    fig07()
    fig08()
    from matplotlib.image import imread
    for p in sorted(OUT.glob("fig0*.png")):
        h, w = imread(p).shape[:2]
        print(f"{p.name}: {w}x{h}px")


if __name__ == "__main__":
    main()
