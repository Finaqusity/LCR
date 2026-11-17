# docs/交付物 —— 导出的成品与原始题目

本目录只放**导出的 docx / pdf**，markdown 源在 `docs/` 下（原理详解源文见 `docs/源稿/`；`DeepSeek/` 是本地草稿区，gitignore）。
所有文件均由脚本生成，不要手工改二进制；改内容请改源 md 后重跑脚本。

| 文件 | 内容 | 来源 / 重生成方式 |
|---|---|---|
| `测量方向-RLC电桥测量装置.docx` | **原始题目**（校队招新测量方向的赛题说明） | 外部提供，勿改 |
| `RLC电桥测量原理详解.docx` / `.pdf` | 原理详解 60 页（13 章 + 附录 A–E）：题目翻译、阻抗基础、自动平衡电桥、幅相测量、校准、误差预算、调试验收 | 源为 `docs/源稿/part1–6.md`（已入库）；生成脚本 `DeepSeek/build_docx.py` 在本地草稿区未入库（依赖 `build/toc_pages.json`）；再由 `tools/export_pdf.ps1` 转 PDF |
| `硬件设计思路.docx` / `.pdf` | 讲解版硬件设计思路（答辩 / 报告素材，8 页） | `docs/硬件设计思路.md` → `python build_sch/make_md_doc.py docs/硬件设计思路.md docs/交付物/硬件设计思路.docx "硬件设计思路"` → `tools/export_pdf.ps1` |
| `硬件设计-六层板.docx` / `.pdf` | 六层板设计细则（最终投的是四层板，作设计依据与存档） | `docs/硬件设计-六层板.md` → `python build_sch/make_hw_doc.py`（或 `make_md_doc.py`）→ `tools/export_pdf.ps1` |
| `测量部分原理图示意.docx` / `.pdf` | AFE 四张原理图（信号源 / 前端 / 六档 / 双通道）+ 元件表 + 网络表 + 调试要点 | `python build_sch/make_sch_doc.py`（读 `build_sch/fig*.png`）→ `tools/export_pdf.ps1` |
| `模拟部分原理图集.pdf` | S0–S5 六页总览，画图 / 接线时对照 | `python build_sch/sch_pages.py`（读 `build_sch/fig*.png`，直接出 PDF） |
| `交接简报.docx` / `.pdf` | 群发版一页上手（4 页）：三分钟跑通、必读顺序、30 条定稿红线、任务与协作规则 | `docs/交接简报.md` → `python build_sch/make_md_doc.py docs/交接简报.md docs/交付物/交接简报.docx "LCR 测量装置 · 交接简报"` → `tools/export_pdf.ps1` |

## 常用命令

```powershell
# 需要装了 python-docx 的 python（本仓库用依赖目录里的 3.12.14）
$py = "C:\Users\Finaqusity\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe"

# 1) md -> docx（通用脚本，可选 title 参数）
& $py build_sch\make_md_doc.py docs\硬件设计思路.md docs\交付物\硬件设计思路.docx "硬件设计思路"

# 2) docx -> pdf（Word COM，输出文件不能已存在？会直接覆盖）
powershell -ExecutionPolicy Bypass -File tools\export_pdf.ps1 -Docx docs\交付物\硬件设计思路.docx -Pdf docs\交付物\硬件设计思路.pdf

# 3) 原理图类
& $py build_sch\make_sch_doc.py
& $py build_sch\sch_pages.py

# 4) 数值一致性核算（改采样率/块长/K/校准结构后必跑，0 失败才算通过）
& $py tools\verify\budget.py
```

> 注意：`make_md_doc.py` / `make_hw_doc.py` 的默认输出已指向本目录；显式传参时也请把目标写在本目录下，避免又落回仓库根。
