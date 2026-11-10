# -*- coding: utf-8 -*-
"""
把 src/part1..part6.md 编译成中文 Word 文档《RLC电桥测量原理详解》。

支持的自定义标记：
  # 标题                 -> 文档大标题（只认第一处）
  副标题：...            -> 副标题
  ## / ###               -> 一级 / 二级标题
  > 引用                 -> 提示框（灰底 + 左边框）
  $$ ... $$              -> 居中公式
  @@EQ ...               -> 居中公式；过长时转为流程框
  @@FIG 路径 | 图题      -> 插图 + 图题
  @@TABLE 表题           -> 表题，后面紧跟 markdown 表格
  行内 $...$             -> Unicode 数学排版（上下标、分数、希腊字母）
"""
import os
import re
import sys
import glob
import json
import shutil

from docx import Document
from docx.shared import Pt, Cm, RGBColor, Emu
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK, WD_TAB_ALIGNMENT, WD_TAB_LEADER
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.section import WD_SECTION
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

try:
    from PIL import Image
except Exception:
    Image = None

ROOT = os.path.dirname(os.path.abspath(__file__))
SRC = sorted(glob.glob(os.path.join(ROOT, "src", "part*.md")))
OUT = os.path.join(ROOT, "RLC电桥测量原理详解.docx")
TOC_PAGES = os.path.join(ROOT, "build", "toc_pages.json")


def norm_toc(s):
    """目录页码匹配用的归一化：去掉空白和 $ 符号。"""
    return re.sub(r"[\s$]", "", s or "")


def load_toc_pages():
    try:
        with open(TOC_PAGES, "r", encoding="utf-8") as f:
            return {norm_toc(k): v for k, v in json.load(f).items()}
    except Exception:
        return {}

EA_BODY = "宋体"
EA_HEAD = "微软雅黑"
MATH_FONT = "Cambria Math"
TEXT_W_CM = 16.0

# ---------------------------------------------------------------- 数学符号表
GREEK = {
    "omega": "ω", "Omega": "Ω", "pi": "π", "varphi": "φ", "phi": "φ",
    "mu": "μ", "times": "×", "cdot": "·", "approx": "≈", "equiv": "≡",
    "Longrightarrow": "⟹", "Rightarrow": "⇒", "rightarrow": "→", "to": "→",
    "leftarrow": "←", "pm": "±", "mp": "∓", "geq": "≥", "ge": "≥",
    "leq": "≤", "le": "≤", "infty": "∞", "Delta": "Δ", "delta": "δ",
    "theta": "θ", "alpha": "α", "beta": "β", "gamma": "γ", "varepsilon": "ε",
    "epsilon": "ε", "lambda": "λ", "tau": "τ", "sigma": "σ", "rho": "ρ",
    "eta": "η", "kappa": "κ", "sum": "Σ", "angle": "∠", "parallel": "∥",
    "sim": "∼", "propto": "∝", "ll": "≪", "gg": "≫", "neq": "≠",
    "circ": "°", "partial": "∂", "prime": "′", "ldots": "…", "dots": "…",
    "quad": " ", "qquad": "  ", "log": "log", "ln": "ln", "tan": "tan",
    "cos": "cos", "sin": "sin", "arctan": "arctan", "arcsin": "arcsin",
    "arccos": "arccos", "max": "max", "min": "min", "exp": "exp",
    "lvert": "|", "rvert": "|", "vert": "|", "Vert": "‖", "lVert": "‖",
    "rVert": "‖", "degree": "°", "textmu": "μ", "mathrm": "", "text": "",
    "sqrt": "√", "left": "", "right": "", "big": "", "Big": "", "!": "",
}


def read_group(s, i):
    """s[i] 应是 '{'，返回 (组内文本, 组后下标)。"""
    assert s[i] == "{", (s, i)
    depth = 0
    j = i
    while j < len(s):
        if s[j] == "{":
            depth += 1
        elif s[j] == "}":
            depth -= 1
            if depth == 0:
                return s[i + 1:j], j + 1
        j += 1
    return s[i + 1:], len(s)


def read_atom(s, i):
    """读取 '^' '\\frac' 之类的实参：{...} 或单个字符。"""
    while i < len(s) and s[i] == " ":
        i += 1
    if i >= len(s):
        return "", i
    if s[i] == "{":
        return read_group(s, i)
    if s[i] == "\\":
        j = i + 1
        while j < len(s) and s[j].isalpha():
            j += 1
        return s[i:j], j
    return s[i], i + 1


class T:
    """数学 token：text + 垂直对齐(None/'sub'/'sup')。"""

    __slots__ = ("text", "va")

    def __init__(self, text, va=None):
        self.text = text
        self.va = va

    def __repr__(self):
        return "T(%r,%r)" % (self.text, self.va)


def flat(tokens):
    return "".join(t.text for t in tokens)


THIN = "\u2009"


def need_paren(txt):
    return any(ch in txt for ch in "+- /|×·" + THIN)


def parse_math(s, va=None):
    """把一小段 LaTeX 子集转成 token 列表。"""
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == "\\":
            j = i + 1
            if j >= len(s):
                break
            if not s[j].isalpha():
                # \, \; \! \  \{ \} \| \% \&
                ch = s[j]
                i = j + 1
                if ch in " ,;:!":
                    out.append(T(" "))
                elif ch == "\\":
                    out.append(T(" "))
                elif ch in "{}%&#":
                    out.append(T(ch))
                elif ch == "|":
                    out.append(T("‖"))
                continue
            k = j
            while k < len(s) and s[k].isalpha():
                k += 1
            name = s[j:k]
            i = k
            if name in ("frac", "dfrac", "tfrac", "cfrac"):
                while i < len(s) and s[i] == " ":
                    i += 1
                a, i = read_group(s, i)
                while i < len(s) and s[i] == " ":
                    i += 1
                b, i = read_group(s, i)
                an, bn = parse_math(a, va), parse_math(b, va)
                at, bt = flat(an), flat(bn)
                lead = ""
                if an and an[0].text[:1] in ("-", "+") and len(an[0].text) == 1:
                    lead, an, at = an[0].text, an[1:], at[1:]
                if lead:
                    out.append(T(lead, va))
                if need_paren(at):
                    out.append(T("(", va)); out.extend(an); out.append(T(")", va))
                else:
                    out.extend(an)
                out.append(T("/", va))
                if need_paren(bt):
                    out.append(T("(", va)); out.extend(bn); out.append(T(")", va))
                else:
                    out.extend(bn)
            elif name in ("text", "mathrm", "operatorname", "textbf", "mbox"):
                while i < len(s) and s[i] == " ":
                    i += 1
                g, i = read_group(s, i)
                out.extend(parse_math(g.replace("$", ""), va))
            elif name == "underbrace":
                while i < len(s) and s[i] == " ":
                    i += 1
                g, i = read_group(s, i)
                out.extend(parse_math(g, va))
                while i < len(s) and s[i] == " ":
                    i += 1
                if i < len(s) and s[i] == "_":
                    lab, i = read_atom(s, i + 1)
                    out.append(T("（", va))
                    out.extend(parse_math(lab, va))
                    out.append(T("）", va))
            elif name == "sqrt":
                while i < len(s) and s[i] == " ":
                    i += 1
                if i < len(s) and s[i] == "{":
                    g, i = read_group(s, i)
                else:
                    g, i = read_atom(s, i)
                sub = parse_math(g, va)
                if len(sub) > 1:
                    out.append(T("√(", va)); out.extend(sub); out.append(T(")", va))
                else:
                    out.append(T("√", va)); out.extend(sub)
            elif name in ("left", "right", "big", "Big", "bigl", "bigr"):
                # 后面紧跟定界符
                while i < len(s) and s[i] == " ":
                    i += 1
                if i < len(s):
                    if s[i] == "\\":
                        jj = i + 1
                        while jj < len(s) and s[jj].isalpha():
                            jj += 1
                        d = s[i + 1:jj]
                        i = jj
                        d = {"lvert": "|", "rvert": "|", "vert": "|",
                             "Vert": "‖", "{": "{", "}": "}", "lVert": "‖", "rVert": "‖"}.get(d, "")
                        if d:
                            out.append(T(d, va))
                    elif s[i] not in ".":
                        out.append(T(s[i], va)); i += 1
                    else:
                        i += 1
            elif name in GREEK:
                g = GREEK[name]
                if g:
                    out.append(T(g, va))
            else:
                # 未知命令：输出名字本身（例如 \Bbb 之类），避免丢内容
                out.append(T(name, va))
        elif c == "^" or c == "_":
            nv = "sup" if c == "^" else "sub"
            body, i = read_atom(s, i + 1)
            inner = parse_math(body, nv)
            out.extend(inner)
        elif c == "{":
            g, i = read_group(s, i)
            out.extend(parse_math(g, va))
        elif c == "}":
            i += 1
        elif c == " ":
            i += 1
            if out and out[-1].text[-1:] not in (" ", THIN):
                out.append(T(THIN, va))
        else:
            out.append(T(c, va))
            i += 1
    # 合并相邻同类 token
    merged = []
    for t in out:
        if merged and merged[-1].va == t.va:
            merged[-1].text += t.text
        else:
            merged.append(T(t.text, t.va))
    return [t for t in merged if t.text != ""]


def math_plain(expr):
    return flat(parse_math(expr)).strip()


ALPHA_RE = re.compile(r"([A-Za-z]+)")


def add_math_runs(p, expr, size, bold=False, color=None):
    for tok in parse_math(expr):
        txt = tok.text
        if not txt:
            continue
        if tok.va is None:
            pieces = ALPHA_RE.split(txt)
            for k, piece in enumerate(pieces):
                if not piece:
                    continue
                r = p.add_run(piece)
                style_run(r, MATH_FONT, EA_HEAD, size, bold, italic=(k % 2 == 1), color=color)
        else:
            r = p.add_run(txt)
            style_run(r, MATH_FONT, EA_HEAD, size, bold, color=color)
            set_vert(r, "superscript" if tok.va == "sup" else "subscript")


def set_vert(run, val):
    rPr = run._element.get_or_add_rPr()
    el = OxmlElement("w:vertAlign")
    el.set(qn("w:val"), val)
    rPr.append(el)


def style_run(run, ascii_font, ea_font, size, bold=False, italic=False, color=None):
    run.font.size = Pt(size)
    run.bold = bold
    run.italic = italic
    if color:
        run.font.color.rgb = RGBColor.from_string(color)
    rPr = run._element.get_or_add_rPr()
    rFonts = rPr.find(qn("w:rFonts"))
    if rFonts is None:
        rFonts = OxmlElement("w:rFonts")
        rPr.insert(0, rFonts)
    rFonts.set(qn("w:ascii"), ascii_font)
    rFonts.set(qn("w:hAnsi"), ascii_font)
    rFonts.set(qn("w:eastAsia"), ea_font)


def tokenize(s):
    """切成 (文本, 是否粗体, 是否数学) 片段。"""
    out = []
    buf = []
    bold = False
    i = 0
    n = len(s)

    def flush():
        nonlocal buf
        if buf:
            out.append(("".join(buf), bold, False))
            buf = []

    while i < n:
        if s[i] == "$" and i + 1 < n and s[i + 1] == "$":
            i += 2
            continue
        if s[i] == "$":
            flush()
            j = s.find("$", i + 1)
            if j < 0:
                buf.append(s[i])
                i += 1
                continue
            out.append((s[i + 1:j], bold, True))
            i = j + 1
            continue
        if s[i] == "*" and i + 1 < n and s[i + 1] == "*":
            flush()
            bold = not bold
            i += 2
            continue
        buf.append(s[i])
        i += 1
    flush()
    return out


def add_rich(p, text, size, ea=EA_BODY, bold=False, color=None, ascii_font=None):
    ascii_font = ascii_font or ea
    for seg, b, is_math in tokenize(text):
        if not seg:
            continue
        if is_math:
            add_math_runs(p, seg, size, bold=(bold or b), color=color)
        else:
            r = p.add_run(seg)
            style_run(r, ascii_font, ea, size, bold=(bold or b), color=color)


# ---------------------------------------------------------------- 段落工具
def set_shading(p, fill):
    pPr = p._p.get_or_add_pPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), fill)
    pPr.append(shd)


def set_left_border(p, color="4472C4", sz=18):
    pPr = p._p.get_or_add_pPr()
    pBdr = OxmlElement("w:pBdr")
    left = OxmlElement("w:left")
    left.set(qn("w:val"), "single")
    left.set(qn("w:sz"), str(sz))
    left.set(qn("w:space"), "8")
    left.set(qn("w:color"), color)
    pBdr.append(left)
    pPr.append(pBdr)


def set_bottom_border(p, color="4472C4", sz=8):
    pPr = p._p.get_or_add_pPr()
    pBdr = OxmlElement("w:pBdr")
    bottom = OxmlElement("w:bottom")
    bottom.set(qn("w:val"), "single")
    bottom.set(qn("w:sz"), str(sz))
    bottom.set(qn("w:space"), "4")
    bottom.set(qn("w:color"), color)
    pBdr.append(bottom)
    pPr.append(pBdr)


def para(doc, align=None, before=0, after=6, line=1.35, indent=None, keep_next=False):
    p = doc.add_paragraph()
    pf = p.paragraph_format
    pf.space_before = Pt(before)
    pf.space_after = Pt(after)
    pf.line_spacing = line
    if align is not None:
        p.alignment = align
    if indent is not None:
        pf.first_line_indent = Cm(indent)
    if keep_next:
        pf.keep_with_next = True
    return p


def add_page_break(doc):
    p = doc.add_paragraph()
    r = p.add_run()
    r.add_break(WD_BREAK.PAGE)


def field(par, instr):
    r = par.add_run()
    fld = OxmlElement("w:fldChar")
    fld.set(qn("w:fldCharType"), "begin")
    r._r.append(fld)
    r2 = par.add_run()
    it = OxmlElement("w:instrText")
    it.set(qn("xml:space"), "preserve")
    it.text = instr
    r2._r.append(it)
    r3 = par.add_run()
    sep = OxmlElement("w:fldChar")
    sep.set(qn("w:fldCharType"), "separate")
    r3._r.append(sep)
    r4 = par.add_run("1")
    style_run(r4, EA_HEAD, EA_HEAD, 9, color="808080")
    r5 = par.add_run()
    end = OxmlElement("w:fldChar")
    end.set(qn("w:fldCharType"), "end")
    r5._r.append(end)


# ---------------------------------------------------------------- Markdown 解析
def split_table_row(line):
    cells = []
    buf = []
    in_math = False
    for ch in line:
        if ch == "$":
            in_math = not in_math
            buf.append(ch)
            continue
        if ch == "|" and not in_math:
            cells.append("".join(buf))
            buf = []
            continue
        buf.append(ch)
    cells.append("".join(buf))
    if cells and cells[0].strip() == "":
        cells = cells[1:]
    if cells and cells[-1].strip() == "":
        cells = cells[:-1]
    return [c.strip() for c in cells]


def parse_blocks(paths):
    blocks = []
    for path in paths:
        with open(path, encoding="utf-8") as f:
            lines = f.read().split("\n")
        i = 0
        while i < len(lines):
            line = lines[i].rstrip()
            s = line.strip()
            if not s:
                i += 1
                continue
            if s == "---":
                i += 1
                continue
            if s.startswith("# ") and not s.startswith("## "):
                blocks.append(("title", s[2:].strip(), path))
                i += 1
                continue
            if s.startswith("## "):
                blocks.append(("h1", s[3:].strip(), path))
                i += 1
                continue
            if s.startswith("### "):
                blocks.append(("h2", s[4:].strip(), path))
                i += 1
                continue
            if s.startswith("#### "):
                blocks.append(("h3", s[5:].strip(), path))
                i += 1
                continue
            if s.startswith("@@FIG"):
                body = s[5:].strip()
                if "|" in body:
                    p_, cap = body.split("|", 1)
                else:
                    p_, cap = body, ""
                blocks.append(("fig", (p_.strip(), cap.strip()), path))
                i += 1
                continue
            if s.startswith("@@TABLE"):
                blocks.append(("tcap", s[7:].strip(), path))
                i += 1
                continue
            if s.startswith("@@EQ"):
                blocks.append(("eq", s[4:].strip(), path))
                i += 1
                continue
            if s.startswith("$$"):
                content = s
                if content.count("$$") >= 2:
                    blocks.append(("eq", content.strip("$").strip(), path))
                    i += 1
                    continue
                # 跨行公式
                buf = [s[2:]]
                i += 1
                while i < len(lines) and "$$" not in lines[i]:
                    buf.append(lines[i].rstrip())
                    i += 1
                if i < len(lines):
                    buf.append(lines[i].replace("$$", "").rstrip())
                    i += 1
                blocks.append(("eq", " ".join(x.strip() for x in buf if x.strip()), path))
                continue
            if s.startswith(">"):
                buf = []
                while i < len(lines) and lines[i].strip().startswith(">"):
                    buf.append(lines[i].strip()[1:].strip())
                    i += 1
                blocks.append(("quote", " ".join(buf), path))
                continue
            if s.startswith("|"):
                tlines = []
                while i < len(lines) and lines[i].strip().startswith("|"):
                    tlines.append(lines[i].strip())
                    i += 1
                blocks.append(("table", tlines, path))
                continue
            if s.startswith("- "):
                buf = []
                while i < len(lines) and lines[i].strip().startswith("- "):
                    buf.append(lines[i].strip()[2:].strip())
                    i += 1
                blocks.append(("ul", buf, path))
                continue
            m = re.match(r"^(\d+)\.\s+(.*)$", s)
            if m:
                buf = []
                while i < len(lines):
                    mm = re.match(r"^(\d+)\.\s+(.*)$", lines[i].strip())
                    if not mm:
                        break
                    buf.append((mm.group(1), mm.group(2).strip()))
                    i += 1
                blocks.append(("ol", buf, path))
                continue
            # 普通段落：连续非空行合并
            buf = [s]
            i += 1
            while i < len(lines):
                nxt = lines[i].strip()
                if (not nxt or nxt.startswith("#") or nxt.startswith("@@")
                        or nxt.startswith("$$") or nxt.startswith(">")
                        or nxt.startswith("|") or nxt.startswith("- ")
                        or nxt == "---" or re.match(r"^\d+\.\s+", nxt)):
                    break
                buf.append(nxt)
                i += 1
            blocks.append(("p", " ".join(buf), path))
    return blocks


# ---------------------------------------------------------------- 表格
def set_cell_shading(cell, fill):
    tcPr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), fill)
    tcPr.append(shd)


def set_cell_margins(table, top=40, bottom=40, left=70, right=70):
    tblPr = table._tbl.tblPr
    mar = OxmlElement("w:tblCellMar")
    for tag, val in (("top", top), ("left", left), ("bottom", bottom), ("right", right)):
        el = OxmlElement("w:%s" % tag)
        el.set(qn("w:w"), str(val))
        el.set(qn("w:type"), "dxa")
        mar.append(el)
    tblPr.append(mar)


def repeat_header(row):
    trPr = row._tr.get_or_add_trPr()
    el = OxmlElement("w:tblHeader")
    el.set(qn("w:val"), "true")
    trPr.append(el)


def add_table(doc, tlines):
    rows = [split_table_row(l) for l in tlines]
    if len(rows) >= 2 and re.match(r"^[\s:\-|]+$", tlines[1]):
        rows = [rows[0]] + rows[2:]
    if not rows:
        return
    ncols = max(len(r) for r in rows)
    rows = [r + [""] * (ncols - len(r)) for r in rows]
    size = 9.0 if ncols <= 6 else (8.5 if ncols <= 8 else 8.0)

    table = doc.add_table(rows=len(rows), cols=ncols)
    table.style = "Table Grid"
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = True
    set_cell_margins(table)

    # 列宽按内容长度加权
    weights = []
    for c in range(ncols):
        w = 4
        for r in rows:
            w = max(w, len(re.sub(r"\$[^$]*\$", "xx", r[c]).replace("**", "")))
        weights.append(w)
    total = sum(weights)
    widths = [max(1.15, TEXT_W_CM * w / total) for w in weights]
    scale = TEXT_W_CM / sum(widths)
    widths = [w * scale for w in widths]
    for c in range(ncols):
        table.columns[c].width = Cm(widths[c])

    for ri, r in enumerate(rows):
        row = table.rows[ri]
        for ci, cell_text in enumerate(r):
            cell = row.cells[ci]
            cell.width = Cm(widths[ci])
            p = cell.paragraphs[0]
            p.paragraph_format.space_before = Pt(1.5)
            p.paragraph_format.space_after = Pt(1.5)
            p.paragraph_format.line_spacing = 1.15
            plain = re.sub(r"\$[^$]*\$", "", cell_text)
            plain = plain.replace("**", "").strip()
            if ri == 0:
                p.alignment = WD_ALIGN_PARAGRAPH.CENTER
                add_rich(p, cell_text, size, bold=True)
                set_cell_shading(cell, "D9E2F3")
            else:
                if len(plain) <= 12 and not re.search(r"[，；。]", plain):
                    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
                add_rich(p, cell_text, size)
    repeat_header(table.rows[0])
    # 表格后留一点空
    sp = doc.add_paragraph()
    sp.paragraph_format.space_before = Pt(0)
    sp.paragraph_format.space_after = Pt(0)
    sp.paragraph_format.line_spacing = 0.6
    return table


# ---------------------------------------------------------------- 图片
def add_figure(doc, path, caption):
    full = os.path.join(ROOT, path.replace("\\", "/"))
    if not os.path.exists(full):
        for cand in glob.glob(os.path.join(ROOT, "figs", os.path.basename(path))):
            full = cand
            break
    if not os.path.exists(full):
        p = para(doc, WD_ALIGN_PARAGRAPH.CENTER, before=6, after=2)
        r = p.add_run("[缺图：%s]" % path)
        style_run(r, EA_BODY, EA_BODY, 9, color="C00000")
        if caption:
            cap = para(doc, WD_ALIGN_PARAGRAPH.CENTER, after=10)
            add_rich(cap, caption, 9, color="595959")
        print("  ! 缺图:", path)
        return
    w_cm = 15.5
    try:
        if Image is not None:
            with Image.open(full) as im:
                iw, ih = im.size
            ar = ih / float(iw)
            if w_cm * ar > 9.6:
                w_cm = 9.6 / ar
            if w_cm > 15.5:
                w_cm = 15.5
    except Exception:
        pass
    p = para(doc, WD_ALIGN_PARAGRAPH.CENTER, before=8, after=3, line=1.0, keep_next=True)
    run = p.add_run()
    run.add_picture(full, width=Cm(w_cm))
    if caption:
        cap = para(doc, WD_ALIGN_PARAGRAPH.CENTER, after=10, line=1.2)
        add_rich(cap, caption, 9, color="595959")


# ---------------------------------------------------------------- 各类块输出
def restyle_heading(p, size, color):
    """标题里的 run 统一字号/粗细/颜色，但保留各 run 的字体（数学 run 仍是 Cambria Math）。"""
    for run in p.runs:
        run.font.size = Pt(size)
        run.bold = True
        run.font.color.rgb = RGBColor.from_string(color)


def emit_h1(doc, text, first=False):
    p = doc.add_paragraph()
    pf = p.paragraph_format
    if not first:
        pf.page_break_before = True
    pf.space_before = Pt(2)
    pf.space_after = Pt(12)
    pf.line_spacing = 1.1
    pf.keep_with_next = True
    add_rich(p, text, 17, ea=EA_HEAD, bold=True, color="1F3864")
    set_bottom_border(p, "4472C4", 10)
    p.style = doc.styles["Heading 1"]
    restyle_heading(p, 17, "1F3864")
    return p


def emit_h2(doc, text):
    p = doc.add_paragraph()
    pf = p.paragraph_format
    pf.space_before = Pt(13)
    pf.space_after = Pt(6)
    pf.line_spacing = 1.15
    pf.keep_with_next = True
    add_rich(p, text, 12.5, ea=EA_HEAD, bold=True, color="2E5496")
    p.style = doc.styles["Heading 2"]
    restyle_heading(p, 12.5, "2E5496")
    return p


def emit_h3(doc, text):
    p = para(doc, before=8, after=4, keep_next=True)
    add_rich(p, "**" + text + "**", 11)
    return p


def emit_quote(doc, text):
    p = para(doc, before=6, after=8, line=1.3)
    pf = p.paragraph_format
    pf.left_indent = Cm(0.5)
    pf.right_indent = Cm(0.3)
    pf.space_before = Pt(6)
    pf.space_after = Pt(9)
    add_rich(p, text, 9.5, color="333333")
    set_shading(p, "F2F5FA")
    set_left_border(p, "4472C4", 18)


def emit_eq(doc, text):
    if len(math_plain(text)) <= 62:
        p = para(doc, WD_ALIGN_PARAGRAPH.CENTER, before=8, after=8, line=1.25)
        add_math_runs(p, text, 11.5)
        # 居中公式两侧留白
        p.paragraph_format.left_indent = Cm(0.4)
        p.paragraph_format.right_indent = Cm(0.4)
    else:
        p = para(doc, before=6, after=8, line=1.25)
        p.paragraph_format.left_indent = Cm(0.4)
        p.paragraph_format.right_indent = Cm(0.2)
        add_math_runs(p, text, 9.5)
        set_shading(p, "F5F7FA")


def emit_ul(doc, items):
    for it in items:
        p = para(doc, before=0, after=3, line=1.3)
        pf = p.paragraph_format
        pf.left_indent = Cm(0.75)
        pf.first_line_indent = Cm(-0.45)
        r = p.add_run("• ")
        style_run(r, EA_BODY, EA_BODY, 10.5, color="4472C4")
        add_rich(p, it, 10.5)
        for run in p.runs[1:]:
            pass


def emit_ol(doc, items):
    for num, it in items:
        p = para(doc, before=0, after=3, line=1.3)
        pf = p.paragraph_format
        pf.left_indent = Cm(0.95)
        pf.first_line_indent = Cm(-0.0)
        r = p.add_run("%s. " % num)
        style_run(r, EA_BODY, EA_BODY, 10.5, bold=True, color="2E5496")
        add_rich(p, it, 10.5)


def emit_p(doc, text):
    p = para(doc, WD_ALIGN_PARAGRAPH.JUSTIFY, before=0, after=6, line=1.4, indent=0.74)
    add_rich(p, text, 10.5)


# ---------------------------------------------------------------- 文档框架
def setup_document():
    doc = Document()
    sec = doc.sections[0]
    sec.page_width = Cm(21.0)
    sec.page_height = Cm(29.7)
    sec.left_margin = Cm(2.5)
    sec.right_margin = Cm(2.5)
    sec.top_margin = Cm(2.5)
    sec.bottom_margin = Cm(2.2)
    sec.header_distance = Cm(1.2)
    sec.footer_distance = Cm(1.2)

    normal = doc.styles["Normal"]
    normal.font.name = EA_BODY
    normal.font.size = Pt(10.5)
    rPr = normal.element.get_or_add_rPr()
    rFonts = rPr.find(qn("w:rFonts"))
    if rFonts is None:
        rFonts = OxmlElement("w:rFonts")
        rPr.insert(0, rFonts)
    rFonts.set(qn("w:ascii"), EA_BODY)
    rFonts.set(qn("w:hAnsi"), EA_BODY)
    rFonts.set(qn("w:eastAsia"), EA_BODY)

    # 页眉
    hdr = sec.header.paragraphs[0]
    hdr.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    r = hdr.add_run("RLC 电桥测量装置 · 原理详解")
    style_run(r, EA_HEAD, EA_HEAD, 8.5, color="8C8C8C")
    set_bottom_border(hdr, "BFBFBF", 4)

    # 页脚
    ftr = sec.footer.paragraphs[0]
    ftr.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = ftr.add_run("— ")
    style_run(r, EA_HEAD, EA_HEAD, 9, color="808080")
    field(ftr, " PAGE ")
    r = ftr.add_run(" —")
    style_run(r, EA_HEAD, EA_HEAD, 9, color="808080")
    return doc


def title_page(doc, title, subtitle, meta_lines):
    for _ in range(3):
        para(doc, before=0, after=0, line=1.0)
    p = para(doc, WD_ALIGN_PARAGRAPH.CENTER, before=0, after=6, line=1.15)
    add_rich(p, "**" + title + "**", 26)
    for run in p.runs:
        style_run(run, EA_HEAD, EA_HEAD, 26, bold=True, color="1F3864")
    p2 = para(doc, WD_ALIGN_PARAGRAPH.CENTER, before=2, after=6, line=1.3)
    add_rich(p2, subtitle, 11, color="595959")
    # 装饰线
    p3 = para(doc, WD_ALIGN_PARAGRAPH.CENTER, before=6, after=6, line=1.0)
    r = p3.add_run("◆ ◆ ◆")
    style_run(r, EA_HEAD, EA_HEAD, 9, color="4472C4")
    for _ in range(2):
        para(doc, before=0, after=0, line=1.0)
    for i, line in enumerate(meta_lines):
        pm = para(doc, WD_ALIGN_PARAGRAPH.CENTER, before=0, after=4, line=1.35)
        add_rich(pm, line, 10, color="404040")
    add_page_break(doc)


def toc_page(doc, entries):
    pages = load_toc_pages()
    p = para(doc, before=0, after=10, line=1.1, keep_next=True)
    r = p.add_run("目　录")
    style_run(r, EA_HEAD, EA_HEAD, 15, bold=True, color="1F3864")
    for text, level in entries:
        pe = para(doc, before=1, after=1, line=1.3)
        pf = pe.paragraph_format
        num = pages.get(norm_toc(text))
        if level == 1 and num:
            pf.tab_stops.add_tab_stop(Cm(15.6), WD_TAB_ALIGNMENT.RIGHT, WD_TAB_LEADER.DOTS)
        if level == 1:
            pf.left_indent = Cm(0.2)
            add_rich(pe, text, 10.5, bold=True, color="1F3864")
        else:
            pf.left_indent = Cm(0.9)
            add_rich(pe, text, 9.5, color="404040")
        if level == 1 and num:
            rn = pe.add_run("\t" + str(num))
            style_run(rn, EA_BODY, EA_BODY, 10.5, color="1F3864")
    add_page_break(doc)


def build():
    blocks = parse_blocks(SRC)
    print("解析块数:", len(blocks))
    doc = setup_document()

    # 前置：标题、副标题
    title = "RLC 电桥测量装置 原理详解"
    subtitle = ""
    intro = None
    rest = list(blocks)
    if rest and rest[0][0] == "title":
        title = rest[0][1]
        rest = rest[1:]
    if rest and rest[0][0] == "p":
        subtitle = rest[0][1]
        rest = rest[1:]
    for prefix in ("副标题：", "副标题:"):
        if subtitle.startswith(prefix):
            subtitle = subtitle[len(prefix):].strip()
    if rest and rest[0][0] == "quote":
        intro = rest[0][1]
        rest = rest[1:]

    meta = [
        "—— 基于电赛校队招新「测量方向 RLC 电桥测量装置」题目",
        "面向：零基础大一新生　|　目标：把「自动平衡电桥」讲透",
        "2026 年 11 月",
    ]
    title_page(doc, title, subtitle, meta)

    # 引言 + 目录
    if intro:
        p = para(doc, WD_ALIGN_PARAGRAPH.JUSTIFY, before=0, after=10, line=1.45, indent=0.74)
        add_rich(p, intro, 10.5)
    entries = [(b[1], 1) for b in rest if b[0] == "h1"]
    toc_page(doc, entries)

    first_h1 = True
    for kind, payload, path in rest:
        if kind == "h1":
            emit_h1(doc, payload, first=first_h1)
            first_h1 = False
        elif kind == "h2":
            emit_h2(doc, payload)
        elif kind == "h3":
            emit_h3(doc, payload)
        elif kind == "p":
            emit_p(doc, payload)
        elif kind == "quote":
            emit_quote(doc, payload)
        elif kind == "eq":
            emit_eq(doc, payload)
        elif kind == "ul":
            emit_ul(doc, payload)
        elif kind == "ol":
            emit_ol(doc, payload)
        elif kind == "fig":
            add_figure(doc, payload[0], payload[1])
        elif kind == "tcap":
            cap = para(doc, WD_ALIGN_PARAGRAPH.CENTER, before=10, after=4, line=1.2, keep_next=True)
            add_rich(cap, "**" + payload + "**", 9.5, color="1F3864")
        elif kind == "table":
            add_table(doc, payload)
        else:
            print("未知块类型:", kind)

    # 文档属性
    cp = doc.core_properties
    cp.title = title
    cp.subject = "自动平衡电桥 RLC 测量原理"
    cp.author = "电赛校队招新·测量方向"
    cp.comments = "由 Markdown 源文件自动生成"

    out_path = sys.argv[1] if len(sys.argv) > 1 else OUT
    doc.save(out_path)
    print("已生成:", out_path, os.path.getsize(out_path), "字节")


if __name__ == "__main__":
    build()
