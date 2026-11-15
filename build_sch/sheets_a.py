# -*- coding: utf-8 -*-
"""模拟部分整机原理图（5 张图）：S1 电源与参考 / S2 激励 / S3 夹具与 I-V /
S4 量程网络 / S5 双通道调理与 ADC 接口。输出到 build_sch/sch/*.png。"""
import os
import sys
import math

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from schlib import Sch, BLUE, GREEN, LW  # noqa: E402

OUT = os.path.join(HERE, "sch")
os.makedirs(OUT, exist_ok=True)

W, H = 148.0, 105.0          # 图纸坐标（A4 横向比例）
DARK = "#1F3864"
RED = "#A33A3A"
GOLD = "#B7791F"
GREY = "#666666"


def new_sheet():
    return Sch((-2, W + 2), (-2, H + 2), w=11.8, fs=8.6)


def frame(s, title, no, note=""):
    s.ax.add_patch(__import__("matplotlib.patches", fromlist=["Rectangle"]).Rectangle(
        (3, 3), W - 6, H - 6, fill=False, lw=1.6, ec=DARK, zorder=1))
    s.text(6, H - 5.5, title, ha="left", va="top", fs=14, weight="bold", color=DARK)
    if note:
        s.text(6, H - 10.0, note, ha="left", va="top", fs=7.8, color=GREY)
    # 标题栏
    s.ax.add_patch(__import__("matplotlib.patches", fromlist=["Rectangle"]).Rectangle(
        (86, 3.6), 58.4, 10.4, fill=False, lw=1.0, ec=DARK, zorder=2))
    s.wire([(86, 9.0), (144.4, 9.0)], lw=0.8)
    s.wire([(118, 3.6), (118, 13.4)], lw=0.8)
    s.text(88, 11.4, "RLC 电桥测量装置 · 模拟部分原理图", ha="left", va="center", fs=8.0, color=DARK)
    s.text(88, 6.5, "电赛校队招新「测量方向」", ha="left", va="center", fs=7.2, color=GREY)
    s.text(120, 11.4, "图号  " + no, ha="left", va="center", fs=8.0, color=DARK)
    s.text(120, 6.5, "v1.1  2026-11", ha="left", va="center", fs=7.2, color=GREY)


def port(s, x, y, name, d="r", color=BLUE, fs=7.8):
    L = 3.4
    if d == "r":
        s.wire([(x, y), (x + L, y)])
        s.text(x + L + 0.4, y, name, ha="left", fs=fs, color=color)
        return (x + L, y)
    if d == "l":
        s.wire([(x - L, y), (x, y)])
        s.text(x - L - 0.4, y, name, ha="right", fs=fs, color=color)
        return (x - L, y)
    if d == "u":
        s.wire([(x, y), (x, y + L)])
        s.text(x, y + L + 0.5, name, ha="center", va="bottom", fs=fs, color=color)
        return (x, y + L)
    s.wire([(x, y - L), (x, y)])
    s.text(x, y - L - 0.5, name, ha="center", va="top", fs=fs, color=color)
    return (x, y - L)


def pwr(s, x, y, name, color=GOLD):
    """电源符号：向上短柱 + 横杠。"""
    s.wire([(x, y), (x, y + 1.4)])
    s.wire([(x - 0.9, y + 1.4), (x + 0.9, y + 1.4)])
    s.text(x, y + 1.9, name, ha="center", va="bottom", fs=7.6, color=color)


def relay_contact(s, x1, x2, y, label=""):
    """常开触点：两空心圆 + 斜杆。"""
    s.ax.plot([x1], [y], "o", ms=4.4, mfc="white", mec="k", mew=1.2, zorder=5)
    s.ax.plot([x2], [y], "o", ms=4.4, mfc="white", mec="k", mew=1.2, zorder=5)
    s.wire([(x1 + 0.55, y + 0.4), (x2 - 0.75, y + 1.35)], lw=1.4)
    if label:
        s.text((x1 + x2) / 2, y + 2.0, label, fs=7.4, color=GOLD)


def bnc(s, x, y, label, nets="F+", d="r"):
    """BNC 母座示意：方框 + 中心针 + 外壳。"""
    s.ax.add_patch(__import__("matplotlib.patches", fromlist=["Rectangle"]).Rectangle(
        (x, y - 1.6), 8.0, 3.2, fill=False, lw=1.4, zorder=3))
    s.text(x + 4.0, y + 0.35, "BNC", fs=6.8, color=GREY)
    s.text(x + 4.0, y - 0.85, label, fs=7.6, weight="bold")
    s.wire([(x - 3.0, y), (x, y)])                 # 中心针
    s.text(x - 3.0, y + 0.55, nets, fs=7.2, ha="left", color=BLUE)
    # 外壳接地
    s.wire([(x + 4.0, y + 1.6), (x + 4.0, y + 3.0)])
    s.text(x + 4.0, y + 3.3, "壳→AGND/9V?", fs=6.2, ha="center", va="bottom", color=GREY)


def sw_spdt(s, x, y, common_from, label=""):
    """单刀双掷：common 在左，两个投掷在右（上=正常，下=CAL）。"""
    s.wire(common_from)
    s.ax.plot([x], [y], "o", ms=4.0, mfc="white", mec="k", mew=1.1, zorder=5)
    s.wire([(x, y), (x + 4.6, y + 2.4)], lw=1.3)
    s.ax.plot([x + 4.6], [y + 2.4], "o", ms=4.0, mfc="white", mec="k", mew=1.1, zorder=5)
    s.ax.plot([x + 4.6], [y - 2.4], "o", ms=4.0, mfc="white", mec="k", mew=1.1, zorder=5)
    if label:
        s.text(x + 2.0, y - 3.6, label, fs=7.2, color=GOLD)


# =====================================================================
# S1 电源与参考
# =====================================================================
def sheet1():
    s = new_sheet()
    frame(s, "S1  电源与参考（5 V 输入 → 5 V / 3.3VD / 3.3VA / VDDA / 9 V / VREF_AFE / 1.65 V）", "S1/5",
          "规则：开关回路（MT3608）与继电器电流只在数字区；AEF 正下方的 L3 只放 3.3VA / VREF_AFE / 1.65V。")

    # ---- 5V 入口 ----
    y0 = 80
    s.text(8, y0 + 6.0, "① 5 V 入口与 ORing", ha="left", fs=9, weight="bold", color=DARK)
    s.text(8, y0 + 3.4, "KF301-5.0-2P", ha="left", fs=7.4)
    s.wire([(8, y0), (11, y0)])
    s.res((11, y0), (16, y0), "PTC 500mA", loc="top")
    s.wire([(16, y0), (19, y0)])
    s.dot(19, y0)
    s.cap((19, y0), (19, y0 - 3.0), "SMAJ5.0A", loc="right")
    s.gnd(19, y0 - 3.0)
    s.wire([(19, y0), (23, y0)])
    s.text(23, y0 + 0.9, "5Vin", fs=7.4, color=BLUE)

    # ORing 二极管
    s.text(9, y0 - 4.0, "USB VBUS", ha="left", fs=7.2)
    s.wire([(9, y0 - 5.4), (13, y0 - 5.4)])
    s.ax.add_patch(__import__("matplotlib.patches", fromlist=["Polygon"]).Polygon(
        [(13, y0 - 5.8), (13, y0 - 5.0), (15.4, y0 - 5.4)], closed=True, fill=False, lw=1.4))
    s.wire([(15.4, y0 - 5.8), (15.4, y0 - 5.0)])
    s.wire([(15.4, y0 - 5.4), (19, y0 - 5.4), (19, y0 - 2.6)])
    s.text(16.2, y0 - 4.6, "D2 SS34", fs=6.8, ha="left")
    s.wire([(23, y0), (23, y0 - 1.6)])
    s.ax.add_patch(__import__("matplotlib.patches", fromlist=["Polygon"]).Polygon(
        [(22.6, y0 - 1.6), (23.4, y0 - 1.6), (23.0, y0 - 3.6)], closed=True, fill=False, lw=1.4))
    s.wire([(23, y0 - 3.6), (23, y0 - 6.0)])
    s.text(24.0, y0 - 2.4, "D1 SS34", fs=6.8, ha="left")
    s.text(24.0, y0 - 4.4, "（外部 5V 优先）", fs=6.4, ha="left", color=GREY)

    # 5V 母线
    yb = y0 - 6.0
    s.wire([(23, yb), (140, yb)])
    s.dot(23, yb)
    for x in (46, 74, 104, 126):
        s.dot(x, yb)
    s.text(30, yb + 1.5, "5V", fs=10, weight="bold", color=RED)
    s.cap((33, yb), (33, yb - 3.4), "470µF + 100nF", loc="right")
    s.gnd(33, yb - 3.4)

    # ---- 3.3VD ----
    s.text(40, 60.0, "② 3.3VD（数字）", ha="left", fs=8.6, weight="bold", color=DARK)
    y = 55
    s.wire([(46, yb), (46, y)])
    s.ax.add_patch(__import__("matplotlib.patches", fromlist=["Rectangle"]).Rectangle(
        (42, y - 6.4), 9, 6.4, fill=False, lw=1.5, zorder=3))
    s.text(46.5, y - 1.6, "U10", fs=7.6)
    s.text(46.5, y - 3.2, "ME6211A33", fs=6.6)
    s.text(46.5, y - 4.6, "SOT-89", fs=6.2, color=GREY)
    s.wire([(51, y - 1.2), (57, y - 1.2)])
    s.dot(54, y - 1.2)
    s.cap((54, y - 1.2), (54, y - 4.8), "10µF+100nF", loc="right")
    s.gnd(54, y - 4.8)
    port(s, 57, y - 1.2, "3V3D（MCU / CH340 / OLED）", d="r")
    s.text(40, y - 8.6, "每个 VDD 脚 100nF，6 颗；\nVCAP_1/2 各 2.2µF。", ha="left", fs=6.8, color=GREY)

    # ---- 3.3VA + VDDA ----
    s.text(70, 60.0, "③ 3.3VA / VDDA（模拟）", ha="left", fs=8.6, weight="bold", color=DARK)
    y = 55
    s.wire([(74, yb), (74, y)])
    s.ax.add_patch(__import__("matplotlib.patches", fromlist=["Rectangle"]).Rectangle(
        (70, y - 6.4), 9, 6.4, fill=False, lw=1.5, zorder=3))
    s.text(74.5, y - 1.6, "U12", fs=7.6)
    s.text(74.5, y - 3.2, "ME6211A33", fs=6.6)
    s.text(74.5, y - 4.6, "SOT-23-5", fs=6.2, color=GREY)
    s.wire([(79, y - 1.2), (86, y - 1.2)])
    s.dot(82, y - 1.2)
    s.cap((82, y - 1.2), (82, y - 4.8), "10µF+100nF", loc="right")
    s.gnd(82, y - 4.8)
    port(s, 86, y - 1.2, "3V3A", d="r")
    # VDDA
    yy = y - 10.5
    s.wire([(82, y - 1.2), (82, yy)])
    s.res((82, yy), (88, yy), "10Ω", loc="top")
    s.dot(90, yy)
    s.cap((90, yy), (90, yy - 3.0), "1µF+100nF+10nF", loc="right")
    s.gnd(90, yy - 3.0)
    s.wire([(88, yy), (96, yy)])
    port(s, 96, yy, "VDDA（22）/ VREF+（21）同点", d="r")

    # ---- 9V ----
    s.text(104, 60.0, "④ 9 V（仅 AFE）", ha="left", fs=8.6, weight="bold", color=DARK)
    s.wire([(104, yb), (104, 54)])
    s.ax.add_patch(__import__("matplotlib.patches", fromlist=["Rectangle"]).Rectangle(
        (100, 40), 11, 14, fill=False, lw=1.5, zorder=3))
    s.text(105.5, 52.0, "U13", fs=7.6)
    s.text(105.5, 50.2, "MT3608", fs=6.8)
    s.text(105.5, 48.4, "升压 1.2MHz", fs=6.2, color=GREY)
    s.text(105.5, 46.2, "IN  SW  FB", fs=6.2, color=GREY)
    s.text(105.5, 44.4, "GND EN", fs=6.2, color=GREY)
    # 输出
    s.wire([(111, 45.0), (118, 45.0)])
    s.dot(115, 45.0)
    s.cap((115, 45.0), (115, 41.6), "10µF", loc="right")
    s.gnd(115, 41.6)
    s.res((118, 45.0), (124, 45.0), "10Ω", loc="top")
    s.dot(126, 45.0)
    s.cap((126, 45.0), (126, 41.6), "22µF", loc="right")
    s.gnd(126, 41.6)
    port(s, 126, 45.0, "9VA（U2/U3/U4/U2B）", d="r")
    s.text(100, 38.0, "开关回路（L、D、Cin）只在数字区，\n离 AFE ≥10mm。", ha="left", fs=6.8, color=GREY)

    # ---- VREF_AFE ----
    s.text(8, 30.0, "⑤ VREF_AFE = 3.3 V 参考（缓冲，9 V 供电）", ha="left", fs=8.6, weight="bold", color=DARK)
    y = 24
    s.wire([(8, y), (12, y)])
    s.res((12, y), (17, y), "10Ω", loc="top")
    s.dot(19.5, y)
    s.cap((19.5, y), (19.5, y - 3.2), "10µF∥100nF", loc="right")
    s.gnd(19.5, y - 3.2)
    s.wire([(17, y), (23, y)])
    in1, in2, out = s.opamp(30, y, label="U2B  OPA2810", fs=7.8)
    s.wire([(23, y), (in2[0], in2[1]), in2])
    s.wire([out, (35, y), (35, y + 2.2), (in1[0] - 0.3, y + 2.2), (in1[0] - 0.3, in1[1]), in1])
    s.wire([out, (40, y)])
    port(s, 40, y, "VREF_AFE（I-V 同相端 / 激励参考 / 保护环）", d="r")
    s.text(8, y - 6.0, "用 OPA2810 B 通道（耐 9 V）；OPA2320 只有 5.5 V，不能用。\n每个使用点就近 100nF；保护环经 100Ω 由本网络驱动。",
           ha="left", fs=6.8, color=GREY)

    # ---- VBIAS ----
    s.text(78, 30.0, "⑥ VBIAS = 1.65 V（ADC 中点）", ha="left", fs=8.6, weight="bold", color=DARK)
    y = 24
    s.wire([(78, y + 5), (78, y + 3.0)])
    s.res((78, y + 3.0), (78, y), "10kΩ", loc="left")
    s.dot(78, y)
    s.res((78, y), (78, y - 3.0), "10kΩ", loc="left")
    s.gnd(78, y - 3.0)
    s.wire([(78, y), (82, y)])
    in1, in2, out = s.opamp(89, y, label="U6B  OPA2320", fs=7.8)
    s.wire([(82, y), (in2[0], in2[1]), in2])
    s.wire([out, (94, y), (94, y + 2.0), (in1[0] - 0.3, y + 2.0), (in1[0] - 0.3, in1[1]), in1])
    s.wire([out, (99, y)])
    s.cap((99, y), (99, y - 3.0), "10µF", loc="right")
    s.gnd(99, y - 3.0)
    port(s, 99, y, "VBIAS", d="r")
    s.text(78, y - 6.0, "3.3VD 分压 10k/10k + 跟随；\n与 VREF_AFE（3.3 VA 域）不是同一个东西。", ha="left", fs=6.8, color=GREY)

    # ---- 测试点 ----
    s.text(108, 30.0, "⑦ 测试点（就近配 GND 弹簧）", ha="left", fs=8.6, weight="bold", color=DARK)
    s.text(108, 25.6, "TP5V  TP9V  TP3V3D  TP3V3A\nTPVDDA  TPVREF  TPVBIAS  GND", ha="left", fs=7.4)
    s.text(108, 16.0, "上电顺序：5V → 9V → 3V3D/3V3A → VDDA → VREF_AFE → VBIAS\n判据：9V 8.8–9.2V；VREF 3.300±0.05V；纹波 <1mVpp。",
           ha="left", fs=6.8, color=GREY)
    s.save(os.path.join(OUT, "S1_power.png"))


# =====================================================================
# S2 激励链
# =====================================================================
def sheet2():
    s = new_sheet()
    frame(s, "S2  激励链（AD9833 → 隔直 → 反相放大 → BUF634A → TP1 = F+）", "S2/5",
          "目标：TP1 上 0.5 Vrms ±1%，四个频点波形干净；U2/U3 由 9VA 供电。")

    # AD9833
    s.icbox(6, 52, 30, 40, "U11  AD9833BRMZ",
            ["1 COMP      → 10nF → AGND",
             "2 VDD       → 3V3A + 100nF",
             "3 CAP/2.5V  → 100nF → AGND",
             "4 DGND      → 地平面",
             "5 MCLK      ← PA8（MCO 8MHz）",
             "6 SDATA     ← PA7（串 33Ω）",
             "7 SCLK      ← PA5（串 33Ω）",
             "8 FSYNC     ← PA4 + 10k 上拉",
             "9 AGND      → 地平面",
             "10 VOUT     → 0.6Vpp，含 ~1.65V 直流"], fs=7.4)
    s.wire([(36, 66), (42, 66)])
    s.text(38.5, 67.4, "VOUT", fs=6.6, color=GREY)
    # C1 + R1
    s.cap((42, 66), (47, 66), "C1 10µF", loc="top")
    s.res((47, 66), (53, 66), "R1 10kΩ", loc="top")
    s.wire([(53, 66), (57, 66)])
    s.dot(57, 66)

    # U2A
    in1, in2, out = s.opamp(64, 62, w=3.4, h=2.8, label="U2A  OPA2810", fs=8.4)
    s.wire([(57, 66), (in1[0], in1[1]), in1])
    s.wire([(57, 66), (57, 62.0)])
    s.dot(57, 62.0)
    port(s, 40, 57.5, "VREF_AFE", d="r")
    s.wire([(40 + 3.4, 57.5), (50.5, 57.5), (50.5, in2[1]), in2])
    pwr(s, 60.0, 66.0, "9VA")
    s.wire([(60.0, 66.0), (60.0, 64.6)])
    s.text(61.6, 66.2, "V+", fs=6.4, color=GREY)
    s.gnd(60.0, 55.0)
    s.text(61.6, 54.6, "V−=GND", fs=6.4, color=GREY)

    # 反馈：Rf 22k + VR1 2k 串联，Cf 22pF 并联
    yA = 82.0
    s.wire([(57, 66), (57, yA)])
    s.dot(57, 66)
    s.wire([(57, yA), (60, yA)])
    s.res((60, yA), (67, yA), "Rf 22kΩ 0.1%", loc="top")
    s.wire([(67, yA), (69, yA)])
    s.pot((69, yA), (75, yA), "", frac=0.55)
    s.text(72.0, yA + 2.6, "VR1 2kΩ 多圈", fs=7.2)
    s.text(72.0, yA + 1.5, "（校 TP1 幅度）", fs=6.4, color=GREY)
    s.wire([(75, yA), (80, yA)])
    # 输出侧节点
    s.wire([(80, yA), (80, 66)])
    s.dot(80, 66)
    # Cf
    yC = 88.5
    s.wire([(57, yA), (57, yC), (66, yC)])
    s.cap((66, yC), (70, yC), "Cf 22pF", loc="top")
    s.wire([(70, yC), (80, yC), (80, yA)])

    # U2A 输出 → BUF
    s.wire([out, (76, 62)])
    s.buffer(76, 60.2, 6.0, 3.6, "U3", "BUF634A")
    s.wire([(82, 62), (86, 62)])
    s.dot(84, 62)
    s.wire([(80, 66), (80, 62)]) if False else None
    s.wire([(84, 62), (92, 62)])
    port(s, 92, 62, "TP1 = F+（0.5 Vrms / 3.3V 直流）", d="r")
    pwr(s, 84.0, 66.4, "9VA")
    s.wire([(84.0, 66.4), (84.0, 63.8)])
    s.text(85.6, 66.6, "V+", fs=6.4, color=GREY)
    s.gnd(84.0, 58.0)
    s.text(85.6, 57.6, "V−=GND", fs=6.4, color=GREY)
    s.text(79.0, 57.2, "BW 脚接 V− = 宽带宽模式\n散热焊盘接 V−", ha="left", fs=6.2, color=GREY)

    # 说明
    s.text(6, 44, "增益与频率：", ha="left", fs=8.4, weight="bold", color=DARK)
    s.text(6, 39.5, "|A| = (Rf + VR1) / R1 = (22k + 0~2k) / 10k = 2.2 ~ 2.4，目标 2.36\n"
                     "Cf 与 (Rf+VR1) 组成一阶低通，f−3dB ≈ 1/(2π·23.6k·22pF) ≈ 306 kHz\n"
                     "C1 + R1 高通拐点 1.59 Hz；100 Hz 处仅衰减 0.013%、相移 0.9°",
           ha="left", fs=7.2)
    s.text(6, 28, "频率字（8 MHz MCLK）：", ha="left", fs=8.4, weight="bold", color=DARK)
    s.text(6, 23.5, "FREQREG = f × 2^28 / 8e6；1 kHz → 0x8312（写 0x4312/0x4002）\n"
                    "序列：0x2100（RESET+B28）→ 两字 → 0xC000（相位）→ 0x2000（正弦）\n"
                    "SPI mode2、MSB first、FSYNC 每 16 位一次；FSYNC 上拉 10kΩ 到 3V3A",
           ha="left", fs=7.2)
    s.text(6, 14, "上电检查：MCO 实测 8000.000 kHz → AD9833 静态 midscale（约 1.65 V）→ 1 kHz/0.6 Vpp → 调 VR1 到 1.414 Vpp → 接 10Ω 负载看跌幅 <1%。",
           ha="left", fs=7.0, color=GREY)
    s.save(os.path.join(OUT, "S2_excitation.png"))


if __name__ == "__main__":
    which = sys.argv[1:] or ["1", "2"]
    if "1" in which:
        sheet1()
    if "2" in which:
        sheet2()
