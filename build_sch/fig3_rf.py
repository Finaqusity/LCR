# -*- coding: utf-8 -*-
"""图 3  Rf 六档量程网络（继电器切换）"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from schlib import Sch, BLUE, GREEN
from matplotlib.patches import Rectangle


def contact(s, x1, x2, y, label=""):
    s.ax.plot([x1], [y], "o", ms=6, mfc="white", mec="k", mew=1.8, zorder=5)
    s.ax.plot([x2], [y], "o", ms=6, mfc="white", mec="k", mew=1.8, zorder=5)
    s.wire([(x1 + 0.03, y + 0.09), (x2 - 0.05, y + 0.30)], lw=1.7)
    if label:
        s.text((x1 + x2) / 2, y + 0.46, label, ha="center", fs=9.5, weight="bold")


s = Sch((-0.6, 20.2), (-0.85, 11.4), w=11.0)

yL, yR = 2.0, 17.6          # 左/右母线
rows = [(9.4, "10Ω", "K1", "10Ω – 100Ω"),
        (8.1, "100Ω", "K2", "100Ω – 1kΩ"),
        (6.8, "1kΩ", "K3", "1kΩ – 10kΩ"),
        (5.5, "10kΩ", "K4", "10kΩ – 100kΩ"),
        (4.2, "100kΩ", "K5", "100kΩ – 1MΩ"),
        (2.9, "1MΩ", "K6", "1MΩ – 10MΩ")]

# 母线
s.wire([(yL, 2.55), (yL, 9.75)])
s.wire([(yR, 2.55), (yR, 9.75)])
s.text(yL + 1.9, 10.95, "求和节点（U4 反相输入）", ha="center", fs=9.5, weight="bold")
s.text(yR - 1.9, 10.95, "V_o（U4 缓冲输出）", ha="center", fs=9.5, weight="bold")
s.wire([(yL, 9.75), (yL, 10.6)])
s.wire([(yR, 9.75), (yR, 10.6)])

for y, val, k, cov in rows:
    xa, xb = yL, yR
    s.dot(xa, y)
    s.dot(xb, y)
    s.wire([(xa, y), (xa + 0.55, y)])
    s.res((xa + 0.55, y), (xa + 3.35, y), "Rf = " + val, loc="top")
    s.wire([(xa + 3.35, y), (xa + 4.05, y)])
    contact(s, xa + 4.05, xa + 5.15, y, k)
    s.wire([(xa + 5.15, y), (xb, y)])
    s.text(xa + 6.60, y - 0.34, "覆盖 " + cov, ha="left", va="center", fs=8.5, color=BLUE)

# 右侧说明
s.text(1.2, 1.05,
       "• 6 只继电器（G6K-2F-Y 5V 或干簧 SIL05-1A72-71L）由 ULN2803 驱动，同一时刻只吸合一只；\n"
       "  上电默认全部断开 → 求和节点悬空，软件必须“先选档再采集”。\n"
       "• 触点电阻 0.1Ω 量级：10Ω 档就是 1% 的串联误差，靠“短路校准”消掉（短路时它也串在回路里）。\n"
       "• 每档可并联 1–2.2pF（10Ω/100Ω 档不贴）：Cf 提高稳定性，但会随频率改变 |Rf|，由负载校准吸收。\n"
       "• 100kΩ/1MΩ 档的 1pF 杂散在 100kHz 已经是 −0.2%/−3.6°，所以高档位优先在 1kHz 及以下使用。",
       ha="left", va="top", fs=9, color="k")

s.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), "fig3_rf.png"))

