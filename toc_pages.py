# -*- coding: utf-8 -*-
"""从 build/preview.pdf 里定位每个一级标题的页码，写入 build/toc_pages.json。

用法：python toc_pages.py [pdf路径] [--write]
不加 --write 时只打印，便于核对。
"""
import glob
import json
import os
import re
import sys

import pymupdf

ROOT = os.path.dirname(os.path.abspath(__file__))
PDF = os.path.join(ROOT, "build", "preview.pdf")
OUT = os.path.join(ROOT, "build", "toc_pages.json")


def norm(s):
    return re.sub(r"[\s$]", "", s or "")


def headings():
    hs = []
    for path in sorted(glob.glob(os.path.join(ROOT, "src", "part*.md"))):
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                m = re.match(r"##\s+(.*\S)\s*$", line)
                if m:
                    hs.append(m.group(1))
    return hs


def main():
    pdf = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("-") else PDF
    write = "--write" in sys.argv
    doc = pymupdf.open(pdf)
    print("PDF:", os.path.basename(pdf), "页数:", doc.page_count)
    pages = [norm(p.get_text("text")) for p in doc]
    result = {}
    for h in headings():
        key = norm(h)
        found = None
        for i in range(2, len(pages)):  # 跳过标题页与含目录的页
            if key and key in pages[i]:
                found = i + 1
                break
        print(f"{found if found else '??':>4}  {h}")
        if found:
            result[h] = found
    if write and result:
        os.makedirs(os.path.dirname(OUT), exist_ok=True)
        with open(OUT, "w", encoding="utf-8") as f:
            json.dump(result, f, ensure_ascii=False, indent=1)
        print("已写入", OUT, "共", len(result), "条")
    missing = [h for h in headings() if h not in result]
    if missing:
        print("!! 未定位:", missing)


if __name__ == "__main__":
    main()
