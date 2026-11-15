# -*- coding: utf-8 -*-
"""图 1  信号源链（AD9833 → 隔直/增益/低通 → 复合缓冲 → TP1）"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from schlib import Sch, BLUE, GREEN

s = Sch((0, 19.5), (-3.6, 4.9), w=10.5)

# ---------- AD9833 ----------
x0, y0, w, h = 0.6, -1.5, 5.0, 3.9
s.icbox(x0, y0, w, h, "AD9833  DDS")
s.text(x0 + 0.25, y0 + h - 0.85, "5  MCLK  ← MCO_8M (PA8)", ha="left", fs=9)
s.text(x0 + 0.25, y0 + h - 1.20, "6  SDATA ← SPI1_MOSI (PA7)", ha="left", fs=9)
s.text(x0 + 0.25, y0 + h - 1.55, "7  SCLK  ← SPI1_SCK (PA5)", ha="left", fs=9)
s.text(x0 + 0.25, y0 + h - 1.90, "8  FSYNC ← PA4", ha="left", fs=9)
s.text(x0 + 0.25, y0 + h - 2.30, "2  VDD = 3.3VA + 100nF", ha="left", fs=9)
s.text(x0 + 0.25, y0 + h - 2.65, "3  CAP/2.5V → 100nF → AGND", ha="left", fs=9)
s.text(x0 + 0.25, y0 + h - 3.00, "1  COMP → 10nF → AGND", ha="left", fs=9)
s.text(x0 + 0.25, y0 + h - 3.35, "4/10  DGND/AGND", ha="left", fs=9)
s.text(x0 + 0.25, y0 + h - 3.68, "9  VOUT → 0.6Vpp（10-bit DAC）", ha="left", fs=9)

yv = 0.82
# VOUT → C1 → R1 → OPAMP(-)
s.wire([(x0 + w, yv), (6.30, yv)])
s.cap((6.30, yv), (7.05, yv), "C1 10µF", loc="top")
s.wire([(7.05, yv), (7.45, yv)])
s.res((7.45, yv), (9.05, yv), "R1 10kΩ", loc="top")
s.wire([(9.05, yv), (10.05, yv)])

op = s.opamp(11.55, 0.40, label="U2  OPA2810", extra="GBW 70MHz")
in1, in2, out = op
# 同相端接 3.3V 参考
s.wire([in2, (in2[0] - 0.85, in2[1])])
s.text(in2[0] - 0.95, in2[1], "3.3V\nVREF_AFE", ha="right", fs=9, color=GREEN)

# 输出 → 缓冲
s.wire([out, (13.35, 0.40)])
bin_, bout_ = s.buffer(13.35, -0.15, 2.35, 1.10, "BUF634A", "250mA 复合缓冲")

# 缓冲输出 → 节点 N → TP1
s.wire([(bout_[0], 0.40), (17.05, 0.40)])
s.dot(16.35, 0.40)
s.wire([(16.35, 0.40), (17.05, 0.40)])
s.text(17.15, 0.40, "TP1 = F+", ha="left", fs=11, weight="bold")
s.text(17.15, 0.02, "0.5 Vrms / 3.3V 直流", ha="left", fs=9, color=BLUE)

# ---------- 反馈网络（复合放大：反馈取自缓冲之后） ----------
xA, yTop = 10.05, 4.05
xB = 16.35
s.wire([(xA, yv), (xA, yTop)])
s.wire([(xB, 0.40), (xB, yTop)])
s.dot(xA, yv)
s.dot(xB, 0.40)
# 上支路：Rf + VR1
s.wire([(xA, 3.05), (11.10, 3.05)])
s.res((11.10, 3.05), (12.75, 3.05), "Rf 22kΩ 0.1%", loc="bottom")
s.wire([(12.75, 3.05), (13.10, 3.05)])
s.pot((13.10, 3.05), (14.55, 3.05), "", frac=0.62)
s.wire([(14.55, 3.05), (xB, 3.05)])
s.text(14.02, 3.62, "VR1 10kΩ", va="bottom", fs=s.fs)
s.dot(xA, 3.05)
s.dot(xB, 3.05)
# 下支路：Cf
s.wire([(xA, 4.05), (12.30, 4.05)])
s.cap((12.30, 4.05), (13.10, 4.05), "Cf 22pF", loc="top")
s.wire([(13.10, 4.05), (xB, 4.05)])
s.dot(xA, yTop)
s.dot(xB, yTop)

# ---------- 注释 ----------
s.text(0.6, -2.75,
       "增益 ≈ −(Rf+VR1)/R1 ≈ 2.4～3.0（用 VR1 把 TP1 校准到 0.5 Vrms ±1%）\n"
       "Cf 与 Rf 构成 f≈300kHz 的一阶低通，压掉 DDS 的高频镜像与台阶\n"
       "反馈取自 BUF634A 之后：直流精度由 U2 决定，驱动能力由缓冲级提供\nU2/U3 由 +9 V 模拟电源供电（5 V→MT3608 升压）；同相端参考 VREF_AFE = 3.3 V",
       ha="left", va="top", fs=9.5, color=BLUE)

s.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig1_source.png"))

