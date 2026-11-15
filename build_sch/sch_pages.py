# -*- coding: utf-8 -*-
"""模拟部分原理图集（A4 横向，6 页）：S0 总览 / S1 电源与参考 / S2 激励 /
S3 夹具与 I-V / S4 量程网络 / S5 双通道调理与 ADC。输出 PDF + 每页 PNG。"""
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.patches import Rectangle, FancyArrowPatch

matplotlib.rcParams["font.sans-serif"] = ["Microsoft YaHei"]
matplotlib.rcParams["axes.unicode_minus"] = False

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(HERE, "sch")
os.makedirs(OUT, exist_ok=True)

DARK = "#1F3864"
BLUE = "#1F4E79"
GREEN = "#1E7B34"
RED = "#A33A3A"
GOLD = "#B7791F"
GREY = "#666666"

W, H = 297.0, 210.0          # A4 横向，单位 mm


def new_page():
    fig = plt.figure(figsize=(W / 25.4, H / 25.4))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, W)
    ax.set_ylim(0, H)
    ax.axis("off")
    fig.patch.set_facecolor("white")
    return fig, ax


def frame(ax, title, sub, no, total=6, notes=()):
    ax.add_patch(Rectangle((8, 8), W - 16, H - 16, fill=False, lw=1.2, ec=DARK))
    ax.text(14, H - 13, title, fontsize=14.5, color=DARK, weight="bold", va="top")
    if sub:
        ax.text(14, H - 21, sub, fontsize=8.2, color=GREY, va="top")
    # 标题栏
    ax.add_patch(Rectangle((196, 9), 93, 16, fill=False, lw=0.8, ec=DARK))
    ax.plot([196, 289], [17, 17], lw=0.6, color=DARK)
    ax.plot([252, 252], [9, 25], lw=0.6, color=DARK)
    ax.text(199, 21.5, "RLC 电桥 · 模拟部分原理图集", fontsize=7.0, color=DARK)
    ax.text(199, 12.6, "电赛校队招新「测量方向」", fontsize=6.4, color=GREY)
    ax.text(255, 21.5, "图号 %s"%no, fontsize=7.2, color=DARK)
    ax.text(255, 12.6, "v1.1　2026-11", fontsize=6.4, color=GREY)
    ax.text(W / 2, 11.5, "%d / %d" % (int(no.split("/")[0][1:]) + 1 if no.startswith("S") else 1, total),
            fontsize=6.6, color=GREY)
    # 注释
    y = 40
    for n in notes:
        ax.text(14, y, n, fontsize=7.4, va="top", color="#222222")
        y -= 4.6 * (1 + n.count("\n"))


def box(ax, x, y, w, h, title, lines=(), ec=DARK, fill="#F7F9FC", fs=7.2, tfs=7.8):
    ax.add_patch(Rectangle((x, y), w, h, facecolor=fill, edgecolor=ec, lw=1.1, zorder=3))
    ax.text(x + w / 2, y + h - 2.2, title, fontsize=tfs, ha="center", va="top",
            weight="bold", color=DARK, zorder=4)
    for i, ln in enumerate(lines):
        ax.text(x + 2.0, y + h - 7.4 - i * 3.8, ln, fontsize=fs, va="center", ha="left",
                color="#222222", zorder=4)


def arrow(ax, p1, p2, color=DARK, lw=1.2, style="-|>", rad=0.0):
    ax.add_patch(FancyArrowPatch(p1, p2, arrowstyle=style, mutation_scale=10,
                                 lw=lw, color=color, zorder=2,
                                 connectionstyle="arc3,rad=%.2f" % rad))


def embed(ax, path, y_bottom, max_w=262.0, max_h=118.0):
    img = plt.imread(path)
    ih, iw = img.shape[:2]
    s = min(max_w / iw, max_h / ih)
    w, h = iw * s, ih * s
    x = (W - w) / 2
    ax.imshow(img, extent=(x, x + w, y_bottom, y_bottom + h), zorder=1, aspect="auto")
    return h


# ---------------------------------------------------------------- S0
def s0(pdf):
    fig, ax = new_page()
    frame(ax, "S0  模拟部分总览：一条信号链、四个关键数字", "被测件串在「激励」与「虚地」之间；测的是 Vin 与 Vo 的复数比 → Zx = −Zf·Vin/Vo", "S0/6",
          notes=["① 量程：切 Rf（10Ω–1MΩ 六档），让 |Vo| 始终落在 0.05–0.5 Vrms（ratio 0.1–1）。",
                 "② 精度：比率测量消掉激励幅度/增益/ADC 基准漂移；残余误差靠 开路/短路/负载 校准与四端夹具。",
                 "③ 相位：类型识别（0°→R，−90°→C，+90°→L）；双通道必须同一 ADC 同步触发（1µs=36°@100kHz）。",
                 "④ 采样：fs = 16·f0，N=1024，M=64 周期相干；DFT 只取 f0 一个 bin（等效带宽≈1.5 kHz）。"])
    y = 146
    seq = [("AD9833", ["0.6 Vpp", "100Hz–100k"]),
           ("隔直+放大", ["U2A ×2.36", "→1.414 Vpp"]),
           ("缓冲", ["BUF634A", "Zo<0.1Ω"]),
           ("TP1 = F+", ["夹具四端", "0.5Vrms±1%"]),
           ("DUT", ["Zx", "F+/S+/F−/S−"]),
           ("I-V 变换", ["U4A+虚地", "I=Vin/Zx"]),
           ("Rf 六档", ["10Ω–1MΩ", "Vo=VREF−I·Zf"]),
           ("双通道", ["AC→1.65V", "U5A/U5B"]),
           ("ADC×2", ["TIM2 TRGO", "N=1024/12bit"]),
           ("DFT→Zx", ["显示 R/L/C", "误差 RSS"])]
    bw, gap = 24.2, 3.0
    x = 16
    for i, (t, lines) in enumerate(seq):
        box(ax, x, y, bw, 26, t, lines, fs=6.2, tfs=7.0)
        if i < len(seq) - 1:
            arrow(ax, (x + bw, y + 13), (x + bw + gap, y + 13))
        x += bw + gap
    # 中部：核心公式
    box(ax, 16, 62, 265, 24, "核心公式与三个关键问题",
        ["Vo − VREF = −(Zf / Zx)·(Vin − VREF)　→　Zx = −Zf·(Vin − VREF)/(Vo − VREF)；取模：|Zx| = |Zf|·|Vin|/|Vo|，相位 φ = φVo − φVin + 180°",
         "为什么成立：I-V 的虚地把 DUT 电流全部逼进 Rf；而 Vin、Vo 都是「测出来的」，激励幅度/放大器增益/ADC 基准的漂移在比值里同时抵消。",
         "为什么分档：切 Rf 让 |Vo| 落在 0.05–0.5 Vrms；为什么不割地：整片地平面让回流路径最短，分的是布局与电源域（详见 S3/S4）。"], fs=6.8)

    # 底部三块
    box(ax, 16, 96, 82, 32, "参考与电源（S1）",
        ["3.3VA → 10Ω → 10µF∥100nF → U2B 跟随 = VREF_AFE 3.3V",
         "5V → MT3608 → 9VA（U2/U3/U4 供电）",
         "3V3D → 10k/10k → U6B 跟随 = VBIAS 1.65V",
         "VDDA = 3.3VA 经 10Ω + 1µF/100nF/10nF"], ec=GOLD)
    box(ax, 108, 96, 82, 32, "校准（S5 的 K7 + 软件）",
        ["K7 把两路同时切到同一 CAL 信号 → α（增益/相位失配）",
         "开路：夹具空置 → 并联杂散 Cp（1–2pF）",
         "短路：粗铜线 → 串联残余 Rs（0.1Ω 级）",
         "负载：1kΩ 0.1% → 残余乘性误差"], ec=RED)
    box(ax, 200, 96, 81, 32, "误差预算（RSS）",
        ["R 1%：未校准 0.59% → 校准后 0.06%",
         "C/L 2%：未校准 1.18% → 校准后 0.22%",
         "R 0.5%：未校准不达标 → 校准后 0.06%",
         "主导项：ADC 通道失配 0.58%、夹具杂散"], ec=GREEN)
    pdf.savefig(fig); plt.close(fig)


# ---------------------------------------------------------------- S1
def s1(pdf):
    fig, ax = new_page()
    frame(ax, "S1  电源与参考：5 V 输入 → 5 条电源域 + 2 个基准",
          "开关回路（MT3608）与继电器电流只在数字区；AFE 正下方的 L3 只放 3.3VA / VREF_AFE / 1.65V", "S1/6",
          notes=["上电顺序：5V → 9V → 3V3D/3V3A → VDDA → VREF_AFE → VBIAS；每步量电压与纹波（<1–2 mVrms）。",
                 "判据：9V 8.8–9.2 V；VREF_AFE 3.300±0.05 V；VBIAS 1.650±0.01 V；纹波 VREF<1 mVpp。",
                 "硬约束：VREF+（21）≤ VDDA（22），留 100–200 mV；初版 VREF+ 与 VDDA 同点，预留 REF3030 焊盘。",
                 "去耦：每 VDD 脚 100nF（6 颗）；VCAP_1/2 各 2.2µF；每个运放电源脚 10µF+100nF。"])
    # 入口
    box(ax, 16, 156, 120, 24, "① 5 V 入口与反接/浪涌保护",
        ["KF301-5.0-2P → PTC 500mA → SMAJ5.0A → 双 SS34 ORing（外部 5V / USB VBUS）",
         "470µF + 100nF 储能；D1/D2 阳极接各自电源，阴极并到 5V 公共轨",
         "首次上电用限流电源 0.3–0.5A，先量 5V 对地不短路"], fs=7.0)
    arrow(ax, (136, 168), (152, 168))
    box(ax, 152, 156, 60, 24, "⑤ 5V 轨",
        ["继电器线圈（6×40–70mA）",
         "CH340、OLED（经 LDO）",
         "LDO 输入：U10/U12、MT3608"],
        ec=RED, fs=7.0)
    # 三条支路
    y2 = 116
    box(ax, 16, y2, 78, 30, "② 3V3D（数字）",
        ["U10 = ME6211A33，SOT-89（500mW）",
         "输出 10µF + 100nF 就近",
         "负载：MCU / CH340 / OLED / 数字逻辑",
         "5V→3.3V、150mA 时 PD≈0.2W，铺铜散热"], fs=6.9)
    box(ax, 108, y2, 78, 30, "③ 3V3A（模拟）",
        ["U12 = ME6211A33，SOT-23-5（≤150mA）",
         "输出 10µF + 100nF",
         "负载：AD9833、OPA2320×2、VREF 分压",
         "与 3V3D 不合并，只在电源侧各自回地"], fs=6.9)
    box(ax, 200, y2, 81, 30, "④ 9VA（仅 AFE）",
        ["U13 = MT3608 升压（1.2MHz）",
         "输出 10µF + 10Ω + 22µF（RC 削纹波）",
         "负载：U2/U3/U4/U2B（BUF634A 需余量 1.8V）",
         "开关回路（L/D/Cin）只走数字区，离 AFE≥10mm"], ec=GOLD, fs=6.9)
    # 5V 分配母线
    ax.plot([30, 280], [150, 150], lw=1.2, color=RED, zorder=2)
    ax.plot([182, 182], [156, 150], lw=1.2, color=RED, zorder=2)
    for xx in (55, 147, 240):
        arrow(ax, (xx, 150), (xx, 146), color=RED, lw=1.0)
    ax.text(24, 152, "5V 分三路", fontsize=7.0, color=RED, ha="left")    # 第二排
    y3 = 74
    box(ax, 16, y3, 88, 28, "VDDA 滤波（从 3V3A）",
        ["3V3A → 10Ω → 1µF + 100nF + 10nF → VDDA(22)",
         "VSSA(20)、VREF−(无此脚) 直接接地平面",
         "不要用磁珠（会与陶瓷电容谐振）"], fs=6.9, ec=GREEN)
    box(ax, 112, y3, 82, 28, "VREF_AFE = 3.3 V 参考",
        ["3.3VA → 10Ω → 10µF∥100nF → U2B(OPA2810) 跟随",
         "OPA2320 只能 5.5V，不能做 9V 域的缓冲",
         "每个使用点 100nF；保护环经 100Ω 由此驱动"], fs=6.9, ec=GREEN)
    box(ax, 202, y3, 79, 28, "VBIAS = 1.65 V（ADC 中点）",
        ["3V3D → 10k/10k → U6B(OPA2320) 跟随 → 10µF",
         "与 VREF_AFE（3.3V）不是同一个东西",
         "只给两路 ADC 入口的 100k 偏置电阻"], fs=6.9, ec=GREEN)
    # 底部：测试点
    ax.text(16, 62, "⑦ 测试点（每个都就近配 GND 弹簧）：TP5V　TP9V　TP3V3D　TP3V3A　TPVDDA　TPVREF　TPVBIAS",
            fontsize=7.4, color=DARK)
    pdf.savefig(fig); plt.close(fig)


# ---------------------------------------------------------------- S2..S5
def image_page(pdf, title, sub, no, png, notes):
    fig, ax = new_page()
    frame(ax, title, sub, no, notes=notes)
    embed(ax, png, 62, max_w=262, max_h=115)
    pdf.savefig(fig); plt.close(fig)


def main():
    p = os.path.join(ROOT, "模拟部分原理图集.pdf")
    with PdfPages(p) as pdf:
        s0(pdf)
        s1(pdf)
        image_page(pdf, "S2  激励链：AD9833 → 隔直 → 反相放大 → BUF634A → TP1",
                   "TP1 上 0.5 Vrms ±1%；U2/U3 由 9VA 供电，输出摆幅 2.59–4.01 V（BUF634A 余量 ≥1.8 V 已满足）",
                   "S2/6", os.path.join(HERE, "fig1_source.png"),
                   ["增益 |A| = (Rf+VR1)/R1；图中 VR1 按 10kΩ 画（2.2–3.2），推荐改用 2kΩ 多圈（2.2–2.4，调得更细，目标 2.36）。",
                    "Cf 22pF 与反馈并联：f−3dB ≈ 306 kHz，100 kHz 处相移 −18°（比率测量里抵消）。",
                    "C1 10µF + R1 10kΩ：高通拐点 1.59 Hz，100 Hz 处衰减 0.013%、相移 0.9°。",
                    "固件：FREQREG=f·2^28/8e6（1 kHz→0x8312）；SPI mode2；FSYNC 上拉 10kΩ。"])
        image_page(pdf, "S3  夹具与 I-V：四端开尔文 + 虚地 + 自动平衡",
                   "DUT 串在 F+ 与 F− 之间；F− 被 I-V 钳在 VREF_AFE（虚地），电流全部流进 Rf",
                   "S3/6", os.path.join(HERE, "fig2_afe.png"),
                   ["四端：F+ 送电流、S+ 只取电压 → 引线与接触电阻不进结果（10Ω 档 20mΩ 就是 0.2%）。",
                    "本版 S− 在夹具处与 F− 短接（二线 + 开路/短路校准）；完全四线留 v2。",
                    "求和节点是最高阻节点：走线 ≤5 mm、四周 guard 环（由 VREF_AFE 经 100Ω 驱动）、下方 L2 完整。",
                    "U4B（OPA2810 另一半）可做 guard 驱动缓冲：5(+)=VREF_AFE、6(−)=7 短接、7→100Ω→环。"])
        image_page(pdf, "S4  Rf 六档量程：用继电器把 |Vo| 关进 0.05–0.5 Vrms",
                   "六只继电器同一时刻只吸合一只；触点电阻在 10Ω 档就是 1%，靠短路校准消掉",
                   "S4/6", os.path.join(HERE, "fig3_rf.png"),
                   ["选档规则：先粗测（K3/1kHz）得 |Zx|；C 取能测的最高频、L 取最低频、R 固定 1 kHz。",
                    "端例外：10µH@100kHz 只有 6.28Ω，即使 10Ω 档 ratio=1.59 也会削顶 → 该档激励降到 0.35Vrms。",
                    "1MΩ 档只在 1 kHz 用（Rf∥1pF 极点 159 kHz，100kHz 下幅值偏 18%、相位 32°）。",
                    "驱动：ULN2803（PD0–PD5 六路 + PD6 校准开关）；上电默认全断，软件须「先选档再采集」。"])
        image_page(pdf, "S5  双通道调理与 ADC：AC 耦合 + 1.65 V 重偏置 + 钳位",
                   "两路必须由 TIM2 TRGO 同时触发（双重同步模式）；1 µs 时差在 100 kHz 就是 36°",
                   "S5/6", os.path.join(HERE, "fig4_adc.png"),
                   ["AC 耦合 + 1.65V 重偏置：把 |Vo| 可用上限从 566 mVrms 提到 1.17 Vrms（3.3V ADC、1.65V 中点）。",
                    "1µF + 100kΩ 高通 1.6 Hz（100 Hz 处衰减 0.02%）；两通道必须同型号同批次。",
                    "ADC 前 100Ω + 1nF：滤采样电荷冲击；BAT54S 把输入钳在 −0.3…3.6 V。",
                    "K7（DPDT）把两路同时切到 CAL 信号 → 读两路复数比得 α，之后 Zx = −Zf·α·Vin/Vo。"])
    print("saved", p)


if __name__ == "__main__":
    main()




