import re
from docx import Document
d = Document("RLC电桥测量原理详解.docx")
paras = [p.text for p in d.paragraphs]
txt = "\n".join(paras)
print("paragraphs:", len(paras), "tables:", len(d.tables))
pats = [r"\$", r"\*\*", r"@@", r"\\[a-zA-Z]+", r"\{|\}", r"\bNone\b", r"\|", r"m0\d{4}", r"\bTODO\b", r"占位", r"整理中"]
for p in pats:
    hits = [(i, x) for i, x in enumerate(paras) if re.search(p, x)]
    if hits:
        print("PATTERN", p, "count", len(hits))
        for i, x in hits[:6]:
            print("   ", i, repr(x[:150]))
