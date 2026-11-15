# -*- coding: utf-8 -*-
"""把 docs/硬件设计-六层板.md 转成 docx（A4，中文排版）"""
import io
import os
import re
import sys

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_LINE_SPACING
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "docs", "硬件设计-六层板.md")
OUT = os.path.join(ROOT, "硬件设计-六层板.docx")

BODY_FONT = "宋体"
HEAD_FONT = "微软雅黑"
MONO_FONT = "Consolas"
ACCENT = RGBColor(0x1F, 0x38, 0x64)
ACCENT2 = RGBColor(0x2E, 0x54, 0x96)
GREY = RGBColor(0x55, 0x55, 0x55)


def set_cell_border(cell, **kwargs):
    tc = cell._tc
    tcPr = tc.get_or_add_tcPr()
    borders = tcPr.find(qn("w:tcBorders"))
    if borders is None:
        borders = OxmlElement("w:tcBorders")
        tcPr.append(borders)
    for edge in ("top", "left", "bottom", "right"):
        if edge in kwargs:
            el = borders.find(qn("w:" + edge))
            if el is None:
                el = OxmlElement("w:" + edge)
                borders.append(el)
            for k, v in kwargs[edge].items():
                el.set(qn("w:" + k), str(v))


def shade(cell, color):
    tcPr = cell._tc.get_or_add_tcPr()
    sh = OxmlElement("w:shd")
    sh.set(qn("w:val"), "clear")
    sh.set(qn("w:color"), "auto")
    sh.set(qn("w:fill"), color)
    tcPr.append(sh)


def cell_text(cell, text, bold=False, size=9.0, align=None):
    cell.text = ""
    p = cell.paragraphs[0]
    p.paragraph_format.space_before = Pt(1.5)
    p.paragraph_format.space_after = Pt(1.5)
    p.paragraph_format.line_spacing = 1.05
    if align is not None:
        p.alignment = align
    add_inline(p, text, size=size, bold=bold)


def add_inline(p, text, size=10.5, bold=False, color=None, mono=False):
    """支持 **粗体**、`代码` 两种行内标记。"""
    parts = re.split(r"(\*\*.+?\*\*|`[^`]+`)", text)
    for part in parts:
        if not part:
            continue
        if part.startswith("**") and part.endswith("**"):
            r = p.add_run(part[2:-2])
            r.bold = True
        elif part.startswith("`") and part.endswith("`"):
            r = p.add_run(part[1:-1])
            r.font.name = MONO_FONT
            r._element.rPr.rFonts.set(qn("w:eastAsia"), MONO_FONT)
            r.font.size = Pt(size - 1.0)
            r.font.color.rgb = RGBColor(0x9C, 0x27, 0x30) if color is None else color
            continue
        else:
            r = p.add_run(part)
            r.bold = bold
        r.font.size = Pt(size)
        if color is not None:
            r.font.color.rgb = color


def style_para(p, size=10.5):
    pf = p.paragraph_format
    pf.space_before = Pt(3)
    pf.space_after = Pt(3)
    pf.line_spacing = 1.5
    for r in p.runs:
        if r.font.size is None:
            r.font.size = Pt(size)
        r.font.name = BODY_FONT
        r._element.rPr.rFonts.set(qn("w:eastAsia"), BODY_FONT)


def add_heading(doc, text, level):
    p = doc.add_paragraph()
    pf = p.paragraph_format
    if level == 1:
        pf.space_before = Pt(6)
        pf.space_after = Pt(10)
    elif level == 2:
        pf.space_before = Pt(14)
        pf.space_after = Pt(6)
    else:
        pf.space_before = Pt(10)
        pf.space_after = Pt(4)
    p.paragraph_format.keep_with_next = True
    sizes = {1: 19, 2: 14.5, 3: 11.8}
    colors = {1: ACCENT, 2: ACCENT2, 3: ACCENT2}
    add_inline(p, text, size=sizes[level], bold=True, color=colors[level])
    for r in p.runs:
        r.font.name = HEAD_FONT
        r._element.rPr.rFonts.set(qn("w:eastAsia"), HEAD_FONT)
    if level <= 2:
        pPr = p._p.get_or_add_pPr()
        pbdr = OxmlElement("w:pBdr")
        bottom = OxmlElement("w:bottom")
        bottom.set(qn("w:val"), "single")
        bottom.set(qn("w:sz"), "8" if level == 1 else "4")
        bottom.set(qn("w:space"), "3")
        bottom.set(qn("w:color"), "1F3864" if level == 1 else "A8B8D8")
        pbdr.append(bottom)
        pPr.append(pbdr)
    return p


def split_row(line):
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    cells, cur, in_code, in_math = [], "", False, False
    i = 0
    while i < len(line):
        ch = line[i]
        if ch == "\\" and i + 1 < len(line) and line[i + 1] == "|":
            cur += "|"
            i += 2
            continue
        if ch == "`":
            in_code = not in_code
        elif ch == "$":
            in_math = not in_math
        if ch == "|" and not in_code and not in_math:
            cells.append(cur.strip())
            cur = ""
        else:
            cur += ch
        i += 1
    cells.append(cur.strip())
    return cells


def is_sep_row(cells):
    return all(re.fullmatch(r":?-{2,}:?", c.replace(" ", "")) for c in cells if c != "")


def add_table(doc, rows):
    ncol = max(len(r) for r in rows)
    t = doc.add_table(rows=0, cols=ncol)
    t.style = "Table Grid"
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    body = [r for r in rows if not is_sep_row(r)]
    total_w = 16.0
    # 列宽：按内容长度加权，限制在 [16%, 40%]
    weights = []
    for j in range(ncol):
        lens = [len(r[j]) if j < len(r) else 0 for r in body]
        weights.append(max(4, max(lens) if lens else 4))
    s = sum(weights)
    widths = [max(0.16, min(0.40, w / s)) for w in weights]
    s2 = sum(widths)
    widths = [w / s2 * total_w for w in widths]
    for i, row in enumerate(body):
        cells = t.add_row().cells
        for j in range(ncol):
            txt = row[j] if j < len(row) else ""
            cell_text(cells[j], txt, bold=(i == 0), size=8.8,
                      align=WD_ALIGN_PARAGRAPH.CENTER if i == 0 else None)
            cells[j].width = Cm(widths[j])
            border = {"val": "single", "sz": "4", "color": "BFBFBF"}
            set_cell_border(cells[j], top=border, bottom=border, left=border, right=border)
            if i == 0:
                shade(cells[j], "D9E2F3")
            if len(body) <= 9 and i < len(body) - 1:
                cells[j].paragraphs[0].paragraph_format.keep_with_next = True
    # 表头行重复
    trPr = t.rows[0]._tr.get_or_add_trPr()
    th = OxmlElement("w:tblHeader")
    th.set(qn("w:val"), "true")
    trPr.append(th)
    doc.add_paragraph().paragraph_format.space_after = Pt(2)


def add_code(doc, lines):
    for i, ln in enumerate(lines):
        p = doc.add_paragraph()
        pf = p.paragraph_format
        pf.space_before = Pt(0)
        pf.space_after = Pt(0)
        pf.line_spacing = 1.0
        pf.left_indent = Cm(0.4)
        r = p.add_run(ln if ln else " ")
        r.font.name = MONO_FONT
        r._element.rPr.rFonts.set(qn("w:eastAsia"), MONO_FONT)
        pf.keep_together = True
        if i < len(lines) - 1:
            pf.keep_with_next = True
        r.font.size = Pt(8.2)
        pPr = p._p.get_or_add_pPr()
        sh = OxmlElement("w:shd")
        sh.set(qn("w:val"), "clear")
        sh.set(qn("w:color"), "auto")
        sh.set(qn("w:fill"), "F4F6F8")
        pPr.append(sh)
    doc.add_paragraph().paragraph_format.space_after = Pt(2)


def build():
    md = io.open(SRC, encoding="utf-8").read().split("\n")
    doc = Document()
    sec = doc.sections[0]
    sec.page_width = Cm(21.0)
    sec.page_height = Cm(29.7)
    sec.left_margin = Cm(2.3)
    sec.right_margin = Cm(2.3)
    sec.top_margin = Cm(2.2)
    sec.bottom_margin = Cm(2.0)

    st = doc.styles["Normal"]
    st.font.name = BODY_FONT
    st.font.size = Pt(10.5)
    st.element.rPr.rFonts.set(qn("w:eastAsia"), BODY_FONT)

    i = 0
    n = len(md)
    first_h1 = True
    while i < n:
        line = md[i].rstrip()
        s = line.strip()
        if not s:
            i += 1
            continue
        # 代码块
        if s.startswith("```"):
            i += 1
            buf = []
            while i < n and not md[i].strip().startswith("```"):
                buf.append(md[i].rstrip("\n"))
                i += 1
            i += 1
            add_code(doc, buf)
            continue
        # 表格
        if s.startswith("|"):
            rows = []
            while i < n and md[i].strip().startswith("|"):
                rows.append(split_row(md[i]))
                i += 1
            add_table(doc, rows)
            continue
        # 标题
        m = re.match(r"^(#{1,4})\s+(.*)$", s)
        if m:
            lvl = min(3, len(m.group(1)))
            txt = m.group(2).strip()
            if lvl == 1 and first_h1:
                p = doc.add_paragraph()
                p.alignment = WD_ALIGN_PARAGRAPH.CENTER
                p.paragraph_format.space_before = Pt(18)
                p.paragraph_format.space_after = Pt(8)
                add_inline(p, txt, size=21, bold=True, color=ACCENT)
                for r in p.runs:
                    r.font.name = HEAD_FONT
                    r._element.rPr.rFonts.set(qn("w:eastAsia"), HEAD_FONT)
                first_h1 = False
            else:
                add_heading(doc, txt, lvl)
            i += 1
            continue
        # 引用
        if s.startswith(">"):
            txt = s.lstrip(">").strip()
            p = doc.add_paragraph()
            pf = p.paragraph_format
            pf.left_indent = Cm(0.5)
            pf.space_before = Pt(4)
            pf.space_after = Pt(4)
            pf.line_spacing = 1.35
            add_inline(p, txt, size=9.2, color=GREY)
            pPr = p._p.get_or_add_pPr()
            pbdr = OxmlElement("w:pBdr")
            left = OxmlElement("w:left")
            left.set(qn("w:val"), "single")
            left.set(qn("w:sz"), "18")
            left.set(qn("w:space"), "8")
            left.set(qn("w:color"), "B4C6E7")
            pbdr.append(left)
            pPr.append(pbdr)
            i += 1
            continue
        # 分隔线
        if re.fullmatch(r"-{3,}", s):
            p = doc.add_paragraph()
            p.paragraph_format.space_before = Pt(2)
            p.paragraph_format.space_after = Pt(2)
            pPr = p._p.get_or_add_pPr()
            pbdr = OxmlElement("w:pBdr")
            bottom = OxmlElement("w:bottom")
            bottom.set(qn("w:val"), "single")
            bottom.set(qn("w:sz"), "6")
            bottom.set(qn("w:space"), "1")
            bottom.set(qn("w:color"), "D0D7E5")
            pbdr.append(bottom)
            pPr.append(pbdr)
            i += 1
            continue
        # 列表
        if re.match(r"^([-*+]|\d+\.)\s+", s):
            txt = re.sub(r"^([-*+]|\d+\.)\s+", "", s)
            num = re.match(r"^(\d+)\.\s+", s)
            bullet = ("%s. " % num.group(1)) if num else "• "
            checked = None
            m2 = re.match(r"^\[( |x|X)\]\s+(.*)$", txt)
            if m2:
                checked = (m2.group(1) in "xX")
                txt = m2.group(2)
            p = doc.add_paragraph()
            pf = p.paragraph_format
            pf.left_indent = Cm(0.75)
            pf.first_line_indent = Cm(-0.4)
            pf.space_before = Pt(1.5)
            pf.space_after = Pt(1.5)
            pf.line_spacing = 1.4
            pre = ("☑ " if checked else "☐ ") if checked is not None else bullet
            r = p.add_run(pre)
            r.font.size = Pt(10.2)
            r.font.name = BODY_FONT
            r._element.rPr.rFonts.set(qn("w:eastAsia"), BODY_FONT)
            add_inline(p, txt, size=10.2)
            i += 1
            continue
        # 普通段落
        p = doc.add_paragraph()
        p.paragraph_format.first_line_indent = Cm(0.0)
        add_inline(p, s, size=10.5)
        style_para(p)
        i += 1

    # 页脚页码
    footer = sec.footer
    fp = footer.paragraphs[0]
    fp.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = fp.add_run()
    fld1 = OxmlElement("w:fldChar")
    fld1.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = "PAGE"
    fld2 = OxmlElement("w:fldChar")
    fld2.set(qn("w:fldCharType"), "end")
    r._r.append(fld1)
    r._r.append(instr)
    r._r.append(fld2)
    r.font.size = Pt(9)
    r.font.name = "Times New Roman"

    cp = doc.core_properties
    cp.title = "硬件设计：单板 · 6 层 · F407VET6 直焊"
    cp.author = "电赛校队招新 · 测量方向"
    cp.subject = "RLC 数字电桥测量装置 硬件设计"
    doc.save(OUT)
    print("saved", OUT)


if __name__ == "__main__":
    build()




