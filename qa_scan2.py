import re, sys
sys.stdout.reconfigure(encoding="utf-8")
from docx import Document
for f in ["RLC电桥测量原理详解.docx", "build/preview.docx"]:
    d = Document(f)
    paras = [p.text for p in d.paragraphs]
    bad = [x for x in paras if ("$" in x) or ("**" in x) or ("@@" in x) or re.search(r"\\[a-zA-Z]{2,}", x)]
    print(f, "| paras", len(paras), "| suspicious", len(bad))
    for x in bad[:8]:
        print("   >>", x[:160])
