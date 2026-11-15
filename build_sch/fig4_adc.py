# -*- coding: utf-8 -*-
"""图 4  双通道调理：AC 耦合 + 1.65V 偏置 + 跟随器 + RC + 保护 → ADC"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from schlib import Sch, BLUE, GREEN
from matplotlib.patches import Polygon

s = Sch((-0.9, 21.4), (-2.1, 10.9), w=11.0)


def clamp(s, x, y):
    s.wire([(x, y), (x, y + 0.42)])
    tri = [(x - 0.13, y + 0.42), (x + 0.13, y + 0.42), (x, y + 0.68)]
    s.ax.add_patch(Polygon(tri, closed=True, fill=False, lw=1.8, zorder=4))
    s.wire([(x - 0.14, y + 0.68), (x + 0.14, y + 0.68)])
    s.wire([(x, y + 0.68), (x, y + 0.96)])
    s.text(x, y + 1.08, "3.3VA", ha="center", fs=7.5)
    s.wire([(x, y), (x, y - 0.42)])
    tri2 = [(x - 0.13, y - 0.68), (x + 0.13, y - 0.68), (x, y - 0.42)]
    s.ax.add_patch(Polygon(tri2, closed=True, fill=False, lw=1.8, zorder=4))
    s.wire([(x - 0.14, y - 0.68), (x + 0.14, y - 0.68)])
    s.wire([(x, y - 0.68), (x, y - 0.96)])
    s.gnd(x, y - 0.96)


# ---------------- VBIAS ----------------
s.text(1.2, 10.55, "3.3VD", ha="center", fs=9.5)
s.wire([(1.2, 10.40), (1.2, 10.10)])
s.res((1.2, 10.10), (1.2, 9.15), "10kΩ", loc="left")
s.dot(1.2, 8.90)
s.res((1.2, 8.70), (1.2, 7.80), "10kΩ", loc="right")
s.gnd(1.2, 7.80)
b1, b2, bo = s.opamp(3.95, 8.90, label="U6B  OPA2320", extra="偏置缓冲")
s.wire([(1.2, 8.90), (1.55, 8.90), (1.55, b2[1]), b2])
s.wire([(1.2, 8.90), (0.62, 8.90), (0.62, 8.55)])
s.cap((0.62, 8.55), (0.62, 7.85), "10µF", loc="left", off=0.30)
s.gnd(0.62, 7.85)

s.wire([bo, (5.35, 8.90), (5.35, 9.90), (b1[0] - 0.30, 9.90),
        (b1[0] - 0.30, b1[1]), b1])
s.wire([(b1[0] - 0.30, b1[1]), b1])
s.wire([bo, (6.10, 8.90)])
s.text(6.20, 8.90, "VBIAS = 1.65V", ha="left", va="center", fs=9.5, weight="bold")
s.text(6.20, 8.40, "（两路共用，低阻）", ha="left", va="center", fs=8.5, color=BLUE)


def channel(y, name, src, adc):
    s.text(0.05, y - 0.52, src, ha="left", fs=8.5, color=GREEN)
    s.wire([(0.20, y), (1.15, y)])
    s.cap((1.15, y), (1.85, y), "1µF", loc="top")
    nd = 2.30
    s.dot(nd, y)
    s.wire([(1.85, y), (3.05, y)])
    # 100k → VBIAS 网络标签
    s.wire([(nd, y), (nd, y + 0.40)])
    s.res((nd, y + 0.40), (nd, y + 1.20), "100kΩ", loc="left")
    s.wire([(nd, y + 1.20), (nd, y + 1.44)])
    s.text(nd + 0.08, y + 1.46, "VBIAS", ha="left", fs=8, color=BLUE)
    # 跟随器
    a1, a2, ao = s.opamp(4.50, y, label=name)
    s.wire([(3.05, y), (3.05, a2[1]), a2])
    s.wire([ao, (6.15, y), (6.15, y + 0.75), (a1[0], y + 0.75), a1])
    # 输出 RC
    s.wire([ao, (6.45, y)])
    s.res((6.45, y), (7.40, y), "100Ω", loc="top")
    s.dot(7.95, y)
    s.wire([(7.40, y), (8.65, y)])
    s.wire([(7.95, y), (7.95, y - 0.62)])
    s.cap((7.95, y - 0.62), (7.95, y - 1.24), "1nF", loc="left")
    s.gnd(7.95, y - 1.24)
    clamp(s, 8.65, y)
    s.text(8.45, y + 0.36, "BAT54S", ha="left", fs=7.5)
    s.wire([(8.65, y), (9.70, y)])
    s.text(9.80, y, adc, ha="left", va="center", fs=9.5, weight="bold", color=BLUE)


channel(5.40, "U5A OPA2320", "V_in（= S+）", "ADC1_IN0（PA0）")
channel(2.60, "U5B OPA2320", "V_o", "ADC2_IN1（PA1）")

# ---------------- 校准开关说明 ----------------
s.text(12.95, 4.35,
       "通道失配校准（K7，DPDT 继电器）：\n"
       "把两路输入同时切到同一个 CAL 信号（由 V_in 缓冲而来），\n"
       "读两路复数比值 → 得到增益/相位失配 α，\n"
       "之后每次测量都用同一 α 做复数修正：\n"
       "        Z_x = −Z_f · α · V_in / V_o",
       ha="left", va="top", fs=9, color="k")

s.text(-0.7, -0.30,
       "• 两级缓冲：跟随器只做高阻隔离，ADC 前再串 100Ω+1nF，把采样瞬间的电荷冲击滤掉。\n"
       "• AC 耦合 + 1.65V 重偏置：|V_o| 可用上限从 566 mVrms 提到 1.17 Vrms（3.3V ADC、1.65V 中点）。\n"
       "• 1µF + 100kΩ 的高通拐点约 1.6 Hz，对 100 Hz 衰减 <0.02%，但两通道必须同型号同批次。\n"
       "• 两路 ADC 必须由 TIM2 TRGO 同时触发（双重同步模式）；1µs 时差在 100kHz 就是 36° 相位误差。",
       ha="left", va="top", fs=9, color="k")

s.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig4_adc.png"))





