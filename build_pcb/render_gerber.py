# -*- coding: utf-8 -*-
"""Render Gerber layers to PNG with matplotlib (no cairo dependency)."""
import os, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Rectangle, FancyBboxPatch, Polygon
from matplotlib.collections import LineCollection
from gerbonara import GerberFile

ROOT = os.path.dirname(os.path.abspath(__file__))
GDIR = os.path.join(ROOT, "gerber")
OUT = os.path.join(ROOT, "img")
os.makedirs(OUT, exist_ok=True)


def draw_layer(ax, path, color, lw_scale):
    f = GerberFile.open(path)
    segs, widths = [], []
    for o in f.objects:
        tn = type(o).__name__
        if not getattr(o, "polarity_dark", True):
            continue
        if tn == "Line":
            a = o.aperture
            w = getattr(a, "diameter", None) or min(getattr(a, "w", 0.2), getattr(a, "h", 0.2))
            segs.append([(o.x1, o.y1), (o.x2, o.y2)])
            widths.append(max(w, 0.05) * lw_scale)
        elif tn == "Flash":
            a = o.aperture
            an = type(a).__name__
            if an == "CircleAperture":
                ax.add_patch(Circle((o.x, o.y), a.diameter / 2, color=color, lw=0))
            elif an == "RectangleAperture":
                ax.add_patch(Rectangle((o.x - a.w / 2, o.y - a.h / 2), a.w, a.h, color=color, lw=0))
            elif an == "ObroundAperture":
                w, h = a.w, a.h
                ax.add_patch(FancyBboxPatch((o.x - w / 2 + min(w, h) / 2, o.y - h / 2 + min(w, h) / 2),
                                            max(w - min(w, h), 1e-3), max(h - min(w, h), 1e-3),
                                            boxstyle=f"round,pad=0,rounding_size={min(w,h)/2}",
                                            color=color, lw=0))
            else:
                ax.add_patch(Rectangle((o.x - 0.3, o.y - 0.3), 0.6, 0.6, color=color, lw=0))
        elif tn == "Region":
            pts = list(o.outline)
            if len(pts) > 2:
                ax.add_patch(Polygon(pts, closed=True, color=color, lw=0))
    if segs:
        lc = LineCollection(segs, colors=color, linewidths=widths, capstyle="round", joinstyle="round")
        ax.add_collection(lc)
    return len(segs)


def render(names_colors, out_name, xlim=None, ylim=None, figsize=12, dpi=230):
    fig, ax = plt.subplots(figsize=(figsize, figsize * 0.92))
    for fn, color in names_colors:
        p = os.path.join(GDIR, fn)
        if not os.path.exists(p):
            print("skip", fn)
            continue
        lw_scale = figsize * 72.0 / 100.0  # 100 mm board -> pt per mm
        n = draw_layer(ax, p, color, lw_scale)
        print(fn, "segments", n)
    ax.set_aspect("equal")
    ax.set_xlim(*(xlim or (0, 100)))
    ax.set_ylim(*(ylim or (0, 100)))
    ax.axis("off")
    fig.tight_layout(pad=0.1)
    dst = os.path.join(OUT, out_name)
    fig.savefig(dst, dpi=dpi, facecolor="white")
    plt.close(fig)
    print("saved", dst)


if __name__ == "__main__":
    render([("Gerber_TopLayer.GTL", "#111111")], "L1_top.png")
    render([("Gerber_BottomLayer.GBL", "#111111")], "L4_bottom.png")
    render([("Gerber_InnerLayer1.G1", "#111111")], "L2_inner1.png")
    render([("Gerber_InnerLayer2.G2", "#111111")], "L3_inner2.png")
    render([("Gerber_TopLayer.GTL", "#BBBBBB"), ("Gerber_TopSilkscreenLayer.GTO", "#1133AA")], "top_silk.png")
