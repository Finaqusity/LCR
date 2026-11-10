# -*- coding: utf-8 -*-
"""Dump all text, tables and image inventory from the assignment docx."""
import sys
from docx import Document
from docx.table import Table
from docx.text.paragraph import Paragraph
from docx.oxml.ns import qn

path = "测量方向-RLC电桥测量装置.docx"
doc = Document(path)

body = doc.element.body
out = []

def iter_block_items(parent):
    for child in parent.iterchildren():
        if child.tag == qn('w:p'):
            yield Paragraph(child, doc)
        elif child.tag == qn('w:tbl'):
            yield Table(child, doc)

for block in iter_block_items(body):
    if isinstance(block, Paragraph):
        txt = block.text
        style = block.style.name if block.style is not None else ""
        if txt.strip():
            out.append(f"[{style}] {txt}")
        # detect images
        if block._p.findall('.//' + qn('a:blip')):
            out.append("  <<IMAGE HERE>>")
    else:
        out.append("[TABLE]")
        for r in block.rows:
            cells = []
            for c in r.cells:
                cells.append(c.text.replace("\n", " / "))
            out.append(" | ".join(cells))
        out.append("[/TABLE]")

text = "\n".join(out)
with open("docx_dump.txt", "w", encoding="utf-8") as f:
    f.write(text)
print(text)
print("\n=== char count:", len(text))

# image inventory
import zipfile
with zipfile.ZipFile(path) as z:
    for n in z.namelist():
        if n.startswith("word/media/"):
            print("MEDIA:", n, z.getinfo(n).file_size)
