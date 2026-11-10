import os, sys
import pymupdf
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.abspath(__file__))
PDF = os.path.join(ROOT, "build", "preview.pdf")
OUT = os.path.join(ROOT, "build", "pages")
os.makedirs(OUT, exist_ok=True)

doc = pymupdf.open(PDF)
n = doc.page_count
print("pages:", n)

paths = []
for i, page in enumerate(doc):
    pix = page.get_pixmap(dpi=110)
    p = os.path.join(OUT, "p%02d.png" % (i + 1))
    pix.save(p)
    paths.append(p)

# contact sheets: 4 pages per sheet (2x2)
cols, rows = 2, 2
thumb_w = 760
sheets = []
for s in range(0, len(paths), cols * rows):
    batch = paths[s:s + cols * rows]
    imgs = [Image.open(p).convert("RGB") for p in batch]
    scaled = []
    for im in imgs:
        w, h = im.size
        nh = int(thumb_w * h / w)
        scaled.append(im.resize((thumb_w, nh), Image.LANCZOS))
    ch = max(i.size[1] for i in scaled)
    sheet = Image.new("RGB", (cols * thumb_w + (cols + 1) * 10, rows * ch + (rows + 1) * 10), "white")
    dr = ImageDraw.Draw(sheet)
    for k, (im, p) in enumerate(zip(scaled, batch)):
        c, r = k % cols, k // cols
        x = 10 + c * (thumb_w + 10)
        y = 10 + r * (ch + 10)
        sheet.paste(im, (x, y))
        dr.text((x + 8, y + 6), os.path.basename(p), fill="red")
    sp = os.path.join(OUT, "sheet_%02d.png" % (s // (cols * rows) + 1))
    sheet.save(sp)
    sheets.append(sp)

print("sheets:", len(sheets))
print("\n".join(sheets))
