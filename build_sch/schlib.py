# -*- coding: utf-8 -*-
"""极简原理图符号库（matplotlib 手绘），用于本项目的“原理图示意”插图。"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon, Circle, FancyArrow

matplotlib.rcParams["font.sans-serif"] = ["Microsoft YaHei"]
matplotlib.rcParams["axes.unicode_minus"] = False

LW = 2.0
FS = 10.5
BLUE = "#1F4E79"
GREEN = "#1E7B34"


class Sch:
    def __init__(self, xlim, ylim, w=9.0, h=None, fs=FS):
        if h is None:
            h = w * (ylim[1] - ylim[0]) / (xlim[1] - xlim[0])
        self.fig, self.ax = plt.subplots(figsize=(w, h))
        self.ax.set_xlim(*xlim)
        self.ax.set_ylim(*ylim)
        self.ax.set_aspect("equal")
        self.ax.axis("off")
        self.fs = fs

    # ---------- 基本图元 ----------
    def wire(self, pts, lw=LW, color="k"):
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        self.ax.plot(xs, ys, color=color, lw=lw, solid_capstyle="round",
                     solid_joinstyle="round", zorder=2)

    def dot(self, x, y, color="k"):
        self.ax.plot([x], [y], "o", ms=5.2, color=color, zorder=5)

    def text(self, x, y, s, fs=None, ha="center", va="center", color="k",
             weight="normal", rot=0, bbox=False):
        kw = dict(fontsize=fs or self.fs, ha=ha, va=va, color=color,
                  fontweight=weight, rotation=rot, zorder=6)
        if bbox:
            kw["bbox"] = dict(facecolor="white", edgecolor="none", pad=0.6)
        self.ax.text(x, y, s, **kw)

    # ---------- 元件 ----------
    def res(self, p1, p2, label="", loc="top", off=0.30, zig=0.13):
        """电阻：沿 p1->p2 画锯齿。"""
        (x1, y1), (x2, y2) = p1, p2
        import math
        dx, dy = x2 - x1, y2 - y1
        L = math.hypot(dx, dy)
        ux, uy = dx / L, dy / L
        px, py = -uy, ux
        n = 6
        body0, body1 = 0.18, 0.82
        pts = [(x1, y1)]
        for i in range(2 * n + 1):
            t = body0 + (body1 - body0) * i / (2 * n)
            s = zig * (0 if i in (0, 2 * n) else (1 if i % 2 == 1 else -1))
            pts.append((x1 + dx * t + px * s, y1 + dy * t + py * s))
        pts.append((x2, y2))
        self.wire(pts)
        if label:
            mx, my = (x1 + x2) / 2, (y1 + y2) / 2
            if loc == "top":
                self.text(mx, my + off, label, va="bottom")
            elif loc == "bottom":
                self.text(mx, my - off, label, va="top")
            elif loc == "left":
                self.text(mx - off, my, label, ha="right")
            else:
                self.text(mx + off, my, label, ha="left")

    def cap(self, p1, p2, label="", loc="right", off=0.28, half=0.24, gap=0.14):
        """电容：两极板垂直于导线方向。"""
        (x1, y1), (x2, y2) = p1, p2
        import math
        dx, dy = x2 - x1, y2 - y1
        L = math.hypot(dx, dy)
        ux, uy = dx / L, dy / L
        px, py = -uy, ux
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        a = (mx - ux * gap / 2, my - uy * gap / 2)
        b = (mx + ux * gap / 2, my + uy * gap / 2)
        self.wire([(x1, y1), a])
        self.wire([b, (x2, y2)])
        for (cx, cy) in (a, b):
            self.wire([(cx - px * half, cy - py * half), (cx + px * half, cy + py * half)])
        if label:
            if loc == "right":
                self.text(mx + off, my, label, ha="left")
            elif loc == "left":
                self.text(mx - off, my, label, ha="right")
            elif loc == "top":
                self.text(mx, my + off, label, va="bottom")
            else:
                self.text(mx, my - off, label, va="top")

    def gnd(self, x, y, label="", s=0.34):
        for i, wdt in enumerate((1.0, 0.6, 0.26)):
            yy = y - i * s * 0.45
            self.wire([(x - s * wdt, yy), (x + s * wdt, yy)], lw=LW)
        if label:
            self.text(x, y - 3 * 0.45 * s - 0.16, label, fs=self.fs - 1, va="top")

    def opamp(self, x, y, w=1.7, h=1.5, label="", extra="", fs=None):
        """运放三角形：中心 (x,y)，输出尖端朝右。返回 in1(-) in2(+) out 三个锚点。"""
        tip = (x + w * 0.55, y)
        a = (x - w * 0.45, y + h / 2)
        b = (x - w * 0.45, y - h / 2)
        self.ax.add_patch(Polygon([a, b, tip], closed=True, fill=False, lw=LW, zorder=3))
        self.text(x - w * 0.34, y + h * 0.24, "−", fs=(fs or self.fs) + 1.5)
        self.text(x - w * 0.34, y - h * 0.24, "+", fs=(fs or self.fs) + 1.5)
        if label:
            self.text(x - w * 0.02, y - h / 2 - 0.22, label, va="top", fs=(fs or self.fs))
        if extra:
            self.text(x - w * 0.02, y - h / 2 - 0.62, extra, va="top", fs=(fs or self.fs) - 1.2, color=BLUE)
        # 输入引线
        self.wire([(x - w * 0.45 - 0.55, y + h * 0.28), (x - w * 0.45, y + h * 0.28)])
        self.wire([(x - w * 0.45 - 0.55, y - h * 0.28), (x - w * 0.45, y - h * 0.28)])
        return (x - w * 0.45 - 0.55, y + h * 0.28), (x - w * 0.45 - 0.55, y - h * 0.28), tip

    def buffer(self, x, y, w=2.0, h=1.1, label="", sub=""):
        """复合缓冲/子电路框：中间一个 ▶ 表示单向。"""
        self.ax.add_patch(Rectangle((x, y - h / 2), w, h, fill=False, lw=LW, zorder=3))
        self.text(x + w / 2, y + (0.18 if sub else 0), label, fs=self.fs)
        if sub:
            self.text(x + w / 2, y - 0.26, sub, fs=self.fs - 1.5, color=BLUE)
        return (x, y), (x + w, y)

    def icbox(self, x, y, w, h, title, lines=(), pins=None, fs=None):
        """矩形器件框；pins=(side, yoffset, name) 仅文本标注。"""
        self.ax.add_patch(Rectangle((x, y), w, h, fill=False, lw=LW, zorder=3))
        self.text(x + w / 2, y + h - 0.30, title, va="top", fs=(fs or self.fs), weight="bold")
        for i, ln in enumerate(lines):
            self.text(x + 0.22, y + h - 0.72 - i * 0.34, ln, ha="left", va="top", fs=(fs or self.fs) - 0.8)
        return self

    def pot(self, p1, p2, label="", loc="top", off=0.32, frac=0.55):
        """电位器：矩形 + 箭头。"""
        (x1, y1), (x2, y2) = p1, p2
        self.res(p1, p2, "", zig=0.0)
        mx, my = x1 + (x2 - x1) * frac, y1 + (y2 - y1) * frac
        self.wire([(x1, y1), (x2, y2)])
        self.ax.add_patch(FancyArrow(mx, my, 0, 0.34, width=0.02, head_width=0.16,
                                     head_length=0.18, length_includes_head=True, lw=1.2))
        if label:
            self.text(mx, my + off + 0.42 if loc == "top" else my - off - 0.42, label,
                      va="bottom" if loc == "top" else "top")
        return (mx, my)

    def save(self, path, dpi=200):
        self.fig.savefig(path, dpi=dpi, bbox_inches="tight", facecolor="white")
        plt.close(self.fig)
        print("saved", path)
