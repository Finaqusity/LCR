# -*- coding: utf-8 -*-
"""图 2  模拟前端主体：3.3V 参考（VREF_AFE）+ 四端夹具 + 自动平衡电桥（I-V 变换）"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from schlib import Sch, BLUE, GREEN
from matplotlib.patches import Polygon, Rectangle


def diode_up(s, x, y1, y2, label="", fs=8.0):
    mid = (y1 + y2) / 2
    tri = [(x - 0.14, mid - 0.12), (x + 0.14, mid - 0.12), (x, mid + 0.12)]
    s.ax.add_patch(Polygon(tri, closed=True, fill=False, lw=1.8, zorder=3))
    s.wire([(x - 0.15, mid + 0.12), (x + 0.15, mid + 0.12)])
    s.wire([(x, y1), (x, mid - 0.12)])
    s.wire([(x, mid + 0.12), (x, y2)])
    if label:
        s.text(x + 0.18, mid, label, ha="left", fs=fs)


s = Sch((-1.0, 27.5), (-3.4, 8.4), w=12.0)

# ================= 3.3V 参考（9V 供电才能摆开） =================
xv = 1.3
s.text(xv, 7.75, "3.3VA", ha="center", fs=9.5)
s.wire([(xv, 7.60), (xv, 7.35)])
s.res((xv, 7.35), (xv, 6.55), "10 Ω", loc="left")
s.dot(xv, 6.30)
s.wire([(xv, 6.55), (xv, 6.30)])
s.wire([(xv, 6.30), (xv, 5.95)])
s.cap((xv, 5.95), (xv, 5.45), "", loc="left")
s.gnd(xv, 5.45)
s.wire([(xv, 6.30), (2.60, 6.30)])

in1, in2, out = s.opamp(4.05, 6.30, label="U2B  OPA2810", fs=10)
s.wire([(2.60, 6.30), (2.60, in2[1]), in2])
s.wire([out, (5.25, 6.30), (5.25, 7.45), (in1[0] - 0.28, 7.45),
        (in1[0] - 0.28, in1[1]), in1])
s.wire([out, (5.55, 6.30)])
s.res((5.55, 6.30), (6.45, 6.30), "10Ω", loc="above" if False else "top")
s.dot(6.85, 6.30)
s.wire([(6.45, 6.30), (8.30, 6.30)])
# 10µF ∥ 100nF
s.wire([(6.85, 6.30), (6.85, 5.85)])
s.cap((6.85, 5.85), (6.85, 5.35), "")
s.gnd(6.85, 5.35)
s.wire([(7.60, 6.30), (7.60, 5.90)])
s.cap((7.60, 5.90), (7.60, 5.40), "")
s.gnd(7.60, 5.40)
s.text(7.22, 4.85, "10µF ∥ 100nF → AGND", ha="center", va="top", fs=8.5)
s.text(8.40, 6.30, "VREF_AFE = 3.3V", ha="left", va="center", fs=10, weight="bold")
s.text(-0.05, 4.30, "3.3VA → 10Ω → 去耦 → 跟随器（U2B 由 9V 供电）：参考必须是“低阻源”", ha="left", va="top", fs=8.3, color=BLUE)

# ================= 四端夹具 =================
bx, by, bw, bh = 11.5, 0.20, 4.6, 2.40
s.ax.add_patch(Rectangle((bx, by), bw, bh, fill=False, lw=1.9, zorder=3))
s.text(bx + bw / 2, by + bh + 0.10, "四端夹具（固定）", ha="center", va="bottom",
       fs=9.5, weight="bold")
# DUT
s.res((12.45, 1.30), (14.75, 1.30), "Z_x", loc="bottom")
s.dot(12.45, 1.30)
s.dot(14.75, 1.30)
s.wire([(bx, 2.05), (12.45, 2.05), (12.45, 1.30)])
s.wire([(bx, 0.55), (12.45, 0.55), (12.45, 1.30)])
s.wire([(14.75, 1.30), (15.40, 1.30), (15.40, 2.05), (bx + bw, 2.05)])
s.wire([(15.40, 1.30), (15.40, 0.55), (bx + bw, 0.55)])
# 端子名（框外）
s.text(bx - 0.12, 2.32, "F+", ha="right", fs=10, weight="bold")
s.text(bx - 0.12, 0.95, "S+", ha="right", fs=10, weight="bold")
s.text(bx + bw + 0.12, 2.32, "F−", ha="left", fs=10, weight="bold")
s.text(bx + bw + 0.12, 0.28, "S−", ha="left", fs=10, weight="bold")

# 激励
s.wire([(9.55, 2.05), (bx, 2.05)])
s.text(9.50, 2.38, "来自图 1（TP1）", ha="right", fs=9, color=GREEN)

# S+ → 1k → 保护 → 通道 1
s.wire([(bx, 0.55), (10.75, 0.55)])
s.res((10.75, 0.55), (9.75, 0.55), "1kΩ", loc="bottom")
s.dot(9.35, 0.55)
s.wire([(9.75, 0.55), (8.70, 0.55)])
diode_up(s, 9.35, 0.55, -0.15, "", fs=7.5)
s.text(8.95, -0.18, "BAT54S", ha="right", fs=7.5)
s.wire([(9.35, -0.15), (9.35, -0.45)])
s.text(9.35, -0.62, "GND", ha="center", va="top", fs=7.5)
s.cap((9.35, 0.55), (9.35, 1.25), "100pF", loc="left")
s.wire([(9.35, 1.25), (9.35, 1.55)])
s.text(9.35, 1.72, "9VA", ha="center", fs=7.5)
s.text(8.55, 0.55, "→ 图 4 通道 1", ha="right", va="center", fs=9, color=BLUE)

# ================= I-V 变换 =================
icx, icy = 20.45, 1.30
iv1, iv2, ivout = s.opamp(icx, icy, label="U4  OPA2810", extra="I-V 变换", fs=10)
# F− → 反相输入
s.wire([(bx + bw, 2.05), (19.15, 2.05), (19.15, iv1[1]), iv1])
s.dot(19.15, 2.05)
# S− 说明
s.wire([(bx + bw, 0.55), (17.15, 0.55)])
s.text(17.20, 0.55, "S−（本版与 F− 短接）", ha="left", va="center", fs=8, color=BLUE)
# 同相输入 → 虚地
s.wire([iv2, (iv2[0] - 0.60, iv2[1])])
s.wire([(iv2[0] - 0.60, iv2[1]), (iv2[0] - 0.60, -0.30)])
s.text(iv2[0] - 0.58, -0.45, "VREF_AFE", ha="center", va="top", fs=8.5, color=GREEN)
# 输出 → 缓冲
s.wire([ivout, (22.05, icy)])
s.buffer(22.05, icy - 0.45, 1.20, 0.90, "BUF634A", "环内")
s.wire([(23.25, icy), (24.60, icy)])
s.dot(23.95, icy)
s.text(24.70, icy, "V_o → 图 4 通道 2", ha="left", va="center", fs=9, weight="bold", color=BLUE)

# 反馈：Rf 网络（图 3）
xA, xB, yF = 19.15, 23.95, 4.55
s.wire([(xA, iv1[1]), (xA, yF)])
s.wire([(xB, icy), (xB, yF)])
s.dot(xA, iv1[1])
s.dot(xB, icy)
s.ax.add_patch(Rectangle((20.10, yF - 0.55), 2.85, 1.10, fill=False, lw=1.9, zorder=3))
s.text(21.52, yF + 0.14, "Rf 六档网络", ha="center", fs=9.5, weight="bold")
s.text(21.52, yF - 0.22, "10Ω–1MΩ · K1–K6", ha="center", fs=7.8, color=BLUE)
s.wire([(xA, yF), (20.10, yF)])
s.wire([(22.95, yF), (xB, yF)])

# ================= 注释 =================
s.text(-0.85, -1.35,
       "• 求和节点（F− 与 Rf 之间）四周必须加保护环：由 VREF_AFE 经 100Ω 驱动，包住焊盘与走线（1MΩ/10pF 档必需）。\n"
       "• U4 的高开环增益把求和点钳在 3.3V（虚地），流过 DUT 的电流几乎全部流进 Rf：I ≈ (V_o − VREF_AFE)/Rf。\n"
       "• 激励侧开环：TP1 由缓冲级固定，DUT 上的实际电压由 S+ 送到通道 1 单独测量，导线压降不进入结果。",
       ha="left", va="top", fs=9, color="k")

s.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig2_afe.png"))



