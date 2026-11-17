# -*- coding: utf-8 -*-
"""生成《测量部分原理图示意》docx（图 1–4 + 元件表 + 网络表 + 调试要点）"""
import os
from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "docs", "交付物", "测量部分原理图示意.docx")
NAVY = RGBColor(0x1F, 0x38, 0x64)
BLUE = RGBColor(0x2E, 0x54, 0x96)

doc = Document()
sec = doc.sections[0]
sec.page_width, sec.page_height = Cm(21.0), Cm(29.7)
sec.left_margin = sec.right_margin = Cm(2.2)
sec.top_margin = Cm(2.2)
sec.bottom_margin = Cm(2.0)

st = doc.styles["Normal"]
st.font.name = "宋体"
st.font.size = Pt(10.5)
st.element.rPr.rFonts.set(qn("w:eastAsia"), "宋体")
st.paragraph_format.line_spacing = 1.45
st.paragraph_format.space_after = Pt(4)


def cn(run, name="宋体"):
    run.font.name = name
    run._element.rPr.rFonts.set(qn("w:eastAsia"), name)


def para(text="", size=10.5, bold=False, color=None, align=None, first=0.74,
         space_before=0, space_after=4, font="宋体"):
    p = doc.add_paragraph()
    if align == "center":
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    elif align == "right":
        p.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    pf = p.paragraph_format
    pf.first_line_indent = Cm(first)
    pf.space_before = Pt(space_before)
    pf.space_after = Pt(space_after)
    if text:
        r = p.add_run(text)
        r.font.size = Pt(size)
        r.bold = bold
        if color is not None:
            r.font.color.rgb = color
        cn(r, font)
    return p


def h1(text):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(14)
    p.paragraph_format.space_after = Pt(6)
    r = p.add_run(text)
    r.font.size = Pt(15)
    r.bold = True
    r.font.color.rgb = NAVY
    cn(r, "微软雅黑")


def h2(text):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(9)
    p.paragraph_format.space_after = Pt(4)
    r = p.add_run(text)
    r.font.size = Pt(11.5)
    r.bold = True
    r.font.color.rgb = BLUE
    cn(r, "微软雅黑")


def bullet(text, size=10):
    p = doc.add_paragraph(style="List Bullet")
    p.paragraph_format.space_after = Pt(2)
    r = p.add_run(text)
    r.font.size = Pt(size)
    cn(r)


def figure(png, caption, width=16.2):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(6)
    p.paragraph_format.space_after = Pt(3)
    p.add_run().add_picture(os.path.join(HERE, png), width=Cm(width))
    c = doc.add_paragraph()
    c.alignment = WD_ALIGN_PARAGRAPH.CENTER
    c.paragraph_format.space_after = Pt(10)
    r = c.add_run(caption)
    r.font.size = Pt(9)
    r.font.color.rgb = RGBColor(0x44, 0x44, 0x44)
    cn(r)


def shade(cell, hexcolor="D9E2F3"):
    tcPr = cell._tc.get_or_add_tcPr()
    el = OxmlElement("w:shd")
    el.set(qn("w:val"), "clear")
    el.set(qn("w:fill"), hexcolor)
    tcPr.append(el)


def table(headers, rows, widths=None, size=8.5):
    t = doc.add_table(rows=1, cols=len(headers))
    t.style = "Table Grid"
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    hdr = t.rows[0].cells
    for i, h in enumerate(headers):
        hdr[i].text = ""
        p = hdr[i].paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        r = p.add_run(h)
        r.font.size = Pt(size)
        r.bold = True
        cn(r, "微软雅黑")
        shade(hdr[i])
    # 表头重复
    trPr = t.rows[0]._tr.get_or_add_trPr()
    el = OxmlElement("w:tblHeader")
    el.set(qn("w:val"), "true")
    trPr.append(el)
    for row in rows:
        cells = t.add_row().cells
        for i, val in enumerate(row):
            cells[i].text = ""
            p = cells[i].paragraphs[0]
            p.paragraph_format.space_after = Pt(1)
            r = p.add_run(str(val))
            r.font.size = Pt(size)
            cn(r)
    if widths:
        for i, w in enumerate(widths):
            for row in t.rows:
                row.cells[i].width = Cm(w)
    doc.add_paragraph().paragraph_format.space_after = Pt(2)


# ---------------- 标题 ----------------
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_before = Pt(6)
r = p.add_run("RLC 数字电桥 · 测量部分原理图示意")
r.font.size = Pt(20)
r.bold = True
r.font.color.rgb = NAVY
cn(r, "微软雅黑")

p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p.add_run("信号源 → 自动平衡电桥 → 双通道调理 → ADC　（对应《原理详解》第 4–6 章）")
r.font.size = Pt(10.5)
r.font.color.rgb = RGBColor(0x40, 0x40, 0x40)
cn(r, "微软雅黑")

p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p.add_run("配套：STM32F407VET6 · 四端夹具 · 3.3 V 双通道同步采样 · 2026 年 11 月")
r.font.size = Pt(9)
r.font.color.rgb = RGBColor(0x66, 0x66, 0x66)
cn(r, "微软雅黑")

h1("〇、先看整体：四张图讲完一条信号链")
para("被测件 Zx 串在“激励”和“虚地”之间。我们要测的不是电流，而是两个电压的复数比：")
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p.add_run("Zx = − Zf · (Vin − VREF) / (Vo − VREF)")
r.font.size = Pt(12)
r.bold = True
r.font.color.rgb = NAVY
cn(r, "Cambria Math")
para("Vin 是被测件高端的实际电压，Vo 是 I-V 变换器的输出，Zf 是当前档位的反馈阻抗（已知、可切换）。"
     "这两路电压由同一个 ADC 同步采样，再用 DFT 取出基波的复数，相除就得到阻抗。", first=0.74)
table(["图", "内容", "关键结论"],
      [["图 1", "信号源链：AD9833 → 反相放大 → BUF634A → TP1",
        "0.5 Vrms、低阻、可微调；MCLK 由 MCU 的 MCO 供，保证 f0 与 fs 严格相干"],
       ["图 2", "模拟前端主体：3.3 V 参考 + 四端夹具 + I-V 变换",
        "虚地把电流“收”进反馈电阻，I ≈ (Vo − VREF)/Rf；S+ 单独测 DUT 实际电压"],
       ["图 3", "Rf 六档量程网络（继电器选择）",
        "10 Ω–1 MΩ 差 10⁵ 倍，靠切 Rf 让 |Vo| 始终在 0.05–0.5 Vrms"],
       ["图 4", "双通道调理：AC 耦合 + 1.65 V 偏置 + 保护 + ADC",
        "两路必须同步；偏置另算（1.65 V），不等于模拟参考 3.3 V"]],
      widths=[1.2, 7.4, 7.4])

# ---------------- 图 1 ----------------
h2("怎么用这份文档")
bullet("先按 图 1 → 图 2 → 图 3 → 图 4 的顺序看一遍；每张图下面的表解释“这一级为什么这么设计、参数怎么来的”。")
bullet("画板时用第六节的网络表逐条核对网络名；调试时按第七节的上电顺序，一步一个判据。")
bullet("元件表里的型号只是“能用”的例子。替换时守住三条底线：运放 GBW ≥ 50 MHz、Rf 精度 0.1%、继电器触点电阻 < 0.1 Ω。")

h1("一、信号源链（图 1）")
para("任务：给被测件一个“幅度已知、频率精确、输出阻抗极低”的正弦。三件事分别对应三个器件："
     "频率由 DDS 的相位累加器决定，幅度由运放增益决定，驱动能力由缓冲器决定。")
figure("fig1_source.png",
       "图 1　信号源链：AD9833（0.6 Vpp）→ C1 隔直 → U2 反相放大（增益 2.4–3.0 可调）→ BUF634A 缓冲 → TP1")
h2("1.1 为什么这样搭")
table(["环节", "器件", "参数", "为什么这么选"],
      [["DDS", "AD9833（3.3 V 供电）",
        "MCLK = 8 MHz（来自 PA8/MCO1）；f_out = MCLK·FREQREG/2²⁸；输出 0.6 Vpp，内含 200 Ω",
        "便宜、SPI 三线、频率分辨率 0.0298 Hz；用 MCU 的 MCO 而不是独立晶振，保证 f0 与采样时钟同源（相干采样）"],
       ["隔直", "C1 = 10 µF", "与 R1 组成高通，拐点 1.6 Hz",
        "AD9833 输出带 1.65 V 直流偏置，必须挡住，否则后面放大器的直流工作点全乱"],
       ["放大", "U2 = OPA2810", "GBW 70 MHz、压摆 192 V/µs、噪声 6 nV/√Hz",
        "把 0.6 Vpp（0.21 Vrms）放大到 1.4 Vpp（0.5 Vrms）；反相放大让 TP1 的直流 = 3.3 V（接参考）"],
       ["增益设定", "R1 = 10 kΩ、Rf = 22 kΩ + VR1 10 kΩ",
        "|A| = (Rf + VR1)/R1 ≈ 2.4～3.0",
        "留 ±20% 余量，用 VR1 把 TP1 校到 0.5 Vrms ±1%（题目只要求 ±10%，但幅度误差会直接进结果）"],
       ["带宽限制", "Cf = 22 pF", "与 Rf∥VR1 构成一阶低通，f₋₃dB ≈ 268 kHz",
        "压掉 DDS 的采样台阶和镜像，同时保证 100 kHz 处衰减 <0.5%"],
       ["驱动", "U3 = BUF634A", "±250 mA、带宽 180 MHz",
        "10 Ω 档要输出 50 mA rms（70 mA 峰值）；OPA2810 只有 75 mA，撑不住且失真大。BUF634A 放反馈环内，直流精度仍由 U2 决定"]],
      widths=[1.7, 3.4, 5.0, 6.0])
h2("1.2 上电先查这三处")
bullet("TP1 直流电平：应为 3.3 V ± 50 mV（由 VREF_AFE 决定）。偏了就是 U2 的同相端没接对。")
bullet("TP1 幅度：空载 0.5 Vrms ±1%，接上 10 Ω 负载后下降 <1%（说明输出阻抗 <1 Ω）。若掉了 10%，是缓冲级没工作。")
bullet("TP1 波形：100 Hz / 1 kHz / 10 kHz / 100 kHz 四个频点都应干净，无台阶、无削顶；100 kHz 时用示波器 1 µs/div 看，正弦应完整。")

# ---------------- 图 2 ----------------
h1("二、模拟前端主体（图 2）")
para("这一张是全机的核心：左边把 3.3 V 变成“能带电流的 3.3 V 参考”，中间是被测件和四端夹具，"
     "右边是自动平衡电桥的 I-V 变换器。")
figure("fig2_afe.png",
       "图 2　模拟前端：3.3 V 参考（缓冲，9 V 供电）→ 四端夹具 → U4/BUF634A 组成的 I-V 变换器（Rf 网络见图 3）")
h2("2.1 3.3 V 参考：为什么必须是“低阻源”")
para("模拟前端由 +9 V 供电（5 V 升压），正弦信号必须骑在一个直流电平上，这个电平就是“参考 / 虚地”。"
     "它不是分压点，而是缓冲后的低阻源：整个测量的基准，所有电流都要从这里进出。")
table(["要求", "做法", "不这么做的后果"],
      [["电压准确", "3.3VA 经 10 Ω 滤波 + 缓冲（LDO 精度 ±1%）", "1% 电阻 → 25 mV 偏差，两个通道都偏，比率上大部分抵消，但 ADC 余量被吃掉"],
       ["输出阻抗低", "OPA2810-B 跟随器（9 V 供电）+ 10 Ω + 10 µF∥100 nF", "直接用分压点（5 kΩ 输出阻抗）：100 kHz 时被 10 Ω 档拉垮，虚地不再是 3.3 V，Zx 全错"],
       ["能吸能灌", "跟随器输出电流 ±50 mA（OPA2810，9 V 供电）", "10 Ω 档 50 mA 峰值，普通运放会限流、波形削顶"],
       ["干净", "10 Ω 隔离 + 10 µF 钽 + 100 nF + 10 nF，紧贴运放电源脚", "开关电源与继电器噪声直接叠在激励上，小信号读数跳动"]],
      widths=[2.6, 6.6, 7.0])
h2("2.2 四端夹具：10 Ω 档的分水岭")
para("两根线测 10 Ω 时，夹具接触电阻 10–30 mΩ 就是 0.1–0.3%，再叠加引线，1% 指标直接告吹。"
     "四端（开尔文）接法把“送电流的线”和“测电压的线”分开：电流线的压降不影响测量，因为测的是 DUT 引脚上的电压。")
bullet("F+ / S+ 都接被测件的高端引脚：F+ 送电流，S+ 取电压（S+ 上几乎没有电流，所以没有压降）。")
bullet("F− 接被测件的低端引脚，电流从这里流进 I-V 变换器的求和点（虚地）。")
bullet("S− 在夹具处与 F− 短接（本版按“二线 + 开路/短路校准”处理）；要完全四线，把 S− 引到第三个 ADC 通道，用 Vin − Vo 做差。")
bullet("夹具必须机械固定（亚克力/3D 打印底座 + M3 螺钉）：手按的接触电阻每次都不一样，10 pF 档 1 pF 的杂散就是 10%。")
h2("2.3 I-V 变换：整台仪器的“心脏”")
para("U4 的反相输入端就是求和点。运放的高增益让这个点电压几乎不变（等于同相端的 VREF），于是流过被测件的电流"
     "只能全部流进 Rf，在输出端变成电压。")
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p.add_run("I ≈ (Vo − VREF) / Zf　　⇒　　Zx = − Zf · (Vin − VREF) / (Vo − VREF)")
r.font.size = Pt(11.5)
r.bold = True
r.font.color.rgb = NAVY
cn(r, "Cambria Math")
table(["设计点", "取值", "理由"],
      [["运放", "OPA2810（GBW 70 MHz、FET 输入、RRIO）", "100 kHz 下环路增益仍很高，虚地“够硬”；FET 输入不吸电流，10 pF 档的漏电流影响最小"],
       ["输出缓冲", "BUF634A 放在 U4 的反馈环内", "既能驱动 250 mA，又不牺牲直流精度（环内的误差被 U4 的增益压掉）"],
       ["反馈阻抗", "Rf 六档：10 Ω / 100 Ω / 1 kΩ / 10 kΩ / 100 kΩ / 1 MΩ（图 3）", "让 |Vo| 始终落在 0.05–0.5 Vrms；量程比 = Rf 之比，校准后精度由 Rf 的比值决定"],
       ["反馈电容", "每档并 1–2.2 pF（10 Ω/100 Ω 不贴）", "增益 ≥10 时容易在几十 MHz 处自激；电容把高频环路增益压下来，残余误差由负载校准吸收"],
       ["保护环", "求和点四周由 VREF（经 100 Ω）驱动一圈", "1 MΩ 档与 10 pF 档必需：PCB 表面漏电流与杂散电容都被 guard 拉到等电位"]],
      widths=[2.2, 7.0, 7.0])
h2("2.4 最容易翻车的三件事")
bullet("虚地当“地”用：参考是 3.3 V 的低阻电源，不是 GND。数字部分的地电流绝不能从虚地走，否则读数里全是数字噪声。")
bullet("继电器全断时读数乱跳：求和点悬空，输入会饱和。软件必须先选档、等 5 ms 让继电器稳定，再触发采样。")
bullet("把 S+ 接到激励源输出而不是 DUT 引脚：引线压降又回来了，10 Ω 档立刻多出 1%。")

# ---------------- 图 3 ----------------
h1("三、Rf 六档量程网络（图 3）")
para("被测阻抗从 10 Ω 到 1 MΩ，跨 5 个数量级；同一个 Rf 不可能同时测好两端。"
     "做法是切换反馈阻抗，让 |Vo| 与 |Vin| 的比值稳定落在 0.1–1 之间——这就是“量程”。")
figure("fig3_rf.png",
       "图 3　Rf 六档量程网络：六只继电器（同一时刻只吸合一只）把选中的 Rf 接进 U4 的反馈环")
h2("3.1 档位与覆盖范围")
table(["档位", "Rf", "合适的 |Zx|", "|Vo|（Vin = 0.5 Vrms）", "典型用途"],
      [["K1", "10 Ω", "10 Ω – 100 Ω", "0.05 – 0.5 Vrms", "小电感、大电容（10 µF@100 Hz = 159 Ω 用不上，100 µH@100 kHz = 6.3 Ω 需降激励）"],
       ["K2", "100 Ω", "100 Ω – 1 kΩ", "0.05 – 0.5 Vrms", "100 µH–1 mH 中频段、1 µF@1 kHz"],
       ["K3", "1 kΩ", "1 kΩ – 10 kΩ", "0.05 – 0.5 Vrms", "10 nF@10 kHz、1 mH@10 kHz、10 kΩ 电阻"],
       ["K4", "10 kΩ", "10 kΩ – 100 kΩ", "0.05 – 0.5 Vrms", "1 nF@100 kHz、100 mH@10 kHz"],
       ["K5", "100 kΩ", "100 kΩ – 1 MΩ", "0.05 – 0.5 Vrms", "100 pF@100 kHz、1 MΩ 电阻（1 kHz）"],
       ["K6", "1 MΩ", "≥ 1 MΩ", "0.05 – 0.5 Vrms", "10 pF@100 kHz（159 kΩ 其实用 K5 更稳，K6 留作储备）"]],
      widths=[1.2, 1.6, 3.2, 3.6, 6.6])
h2("3.2 选档规则（软件实现）")
bullet("先粗测一次（K3 档、1 kHz）：得到 |Zx| 的量级，类型（R/L/C）也能顺带判断。")
bullet("电阻：|Z| 与频率无关，固定 1 kHz，选使比值在 0.1–1 的档位。")
bullet("电容：取“能测的最高频率”，再选档使 |Z| ≥ 1 kΩ（小电容怕杂散，频率越高越好）。")
bullet("电感：取“能测的最低频率”，再选档使 |Z| ≤ 30 kΩ（大电感怕杂散电容，频率越低越好）。")
bullet("端点特例：10 µH@100 kHz 只有 6.28 Ω，即使 K1 档比值也 1.59（削顶）→ 需要 4.7 Ω 档，或把该档激励降到 0.35 Vrms。")

# ---------------- 图 4 ----------------
h1("四、双通道调理与 ADC（图 4）")
para("两路电压必须先变成“适合 ADC 的样子”：去掉 3.3 V 直流、重新偏置到 1.65 V、限带、保护。"
     "注意：1.65 V 是 ADC 的中点，不等于模拟参考 3.3 V——这是新手最容易混的一处。")
figure("fig4_adc.png",
       "图 4　双通道调理：AC 耦合（1 µF）→ 1.65 V 重偏置 → 跟随器 → 100 Ω + 1 nF → BAT54S 保护 → ADC1/ADC2")
h2("4.1 每一级在干什么")
table(["环节", "取值", "作用"],
      [["AC 耦合", "1 µF + 100 kΩ（到 VBIAS）", "挡住 3.3 V 直流；高通拐点 1.6 Hz，对 100 Hz 只衰减 0.02%"],
       ["重偏置", "VBIAS = 1.65 V（3.3 V/2，由 U6B 缓冲）", "把信号放在 ADC 量程正中；|Vo| 可用上限从 566 mVrms 提到 1.17 Vrms"],
       ["跟随器", "U5A / U5B（OPA2320，RRIO）", "高阻隔离，不让 ADC 的采样电容“抽”到前级；两路用同一封装，温漂同步"],
       ["限带", "100 Ω + 1 nF（拐点 1.6 MHz）", "吸收采样瞬间的电荷冲击，压低高频噪声与混叠"],
       ["保护", "BAT54S 双肖特基钳位", "把输入钳在 −0.3 V～3.6 V，防止前端故障打坏 ADC"],
       ["采样", "ADC1_IN0(PA0) / ADC2_IN1(PA1)，TIM2 TRGO 同时触发", "两路必须同一时刻采样：1 µs 时差在 100 kHz 就是 36° 相位误差"]],
      widths=[2.0, 6.2, 7.8])
h2("4.2 通道失配校准（K7）")
para("两个通道的增益和相位不可能完全一样（运放、电阻、ADC 都有差异），这是乘性误差，开路/短路校准消不掉。"
     "用一个双刀继电器把两路输入同时切到同一个 CAL 信号，读两路复数比值就得到失配系数 α：")
p = doc.add_paragraph()
p.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = p.add_run("α = Vch1 / Vch2（同一信号）　⇒　Zx = − Zf · α · (Vin − VREF) / (Vo − VREF)")
r.font.size = Pt(11)
r.bold = True
r.font.color.rgb = NAVY
cn(r, "Cambria Math")
para("α 是复数，一次校准同时修掉幅度与相位。每个频率校一次，存在 MCU 的 flash 里。")

# ---------------- 元件表 ----------------
h1("五、元件表（测量部分）")
para("型号以立创商城/原厂手册核对为准；价格是 2026 年的量级参考，不是报价。", first=0.74)
table(["位号", "器件", "参数/型号", "封装", "数量", "备注"],
      [["U1", "DDS", "AD9833BRMZ", "MSOP-10", "1", "3.3 V 供电；MCLK 由 PA8 供 8 MHz"],
       ["U2", "运放（激励 + 参考缓冲）", "OPA2810（双运放）", "SOIC-8", "1", "A 通道激励放大、B 通道 3.3 V 参考缓冲"],
       ["U3", "缓冲器", "BUF634A", "SOIC-8（带散热焊盘）", "1", "250 mA，注意散热"],
       ["U4", "运放（I-V）", "OPA2810", "SOIC-8", "1", "与 U2 同型号，备件通用"],
       ["U5", "运放（通道）", "OPA2320（双）", "SOIC-8", "1", "两路通道共用一个封装，温漂同步"],
       ["U6", "运放（偏置）", "OPA2320（双）", "SOIC-8", "1", "只做 1.65 V 偏置；3.3 V 参考由 U2B(OPA2810) 提供"],
       ["U7", "继电器驱动", "ULN2803A", "DIP-18/SOIC-18", "1", "6 路量程 + 1 路校准开关"],
       ["K1–K6", "继电器", "G6K-2F-Y 5 V（或干簧 SIL05-1A72-71L）", "SMD", "6", "触点电阻 <0.1 Ω；干簧更贵但漏电更小"],
       ["K7", "继电器（校准）", "G6K-2F-Y 5 V（双刀）", "SMD", "1", "把两路输入同切到 CAL"],
       ["R1", "电阻", "10 kΩ 0.1% 25 ppm", "0805", "1", "激励增益设定"],
       ["Rf", "电阻", "10 Ω/100 Ω/1 kΩ/10 kΩ/100 kΩ/1 MΩ，0.1%", "0805/1206", "6", "1 MΩ 用 1206 或两只串联，注意耐压与漏电"],
       ["R2/R3", "电阻", "10 kΩ 0.1%（备用）", "0805", "2", "原虚地分压已取消（改 LDO 参考 + 缓冲），留空位"],
       ["R4/R5", "电阻", "10 kΩ 0.1%（1.65 V 分压）", "0805", "2", ""],
       ["C1", "电容", "10 µF/16 V X7R", "1206", "1", "隔直"],
       ["Cf", "电容", "1–2.2 pF C0G", "0603", "6", "每档一只，可空贴"],
       ["C2/C3", "电容", "10 µF 钽 + 100 nF + 10 nF", "1206/0603", "各 1", "参考输出滤波"],
       ["C4–C7", "电容", "1 µF/16 V X7R", "0603", "4", "两路 AC 耦合"],
       ["C8/C9", "电容", "1 nF C0G", "0603", "2", "ADC 前限带"],
       ["D1–D4", "二极管", "BAT54S（双肖特基）", "SOT-23", "4", "S+ 与两路 ADC 输入钳位"],
       ["VR1", "电位器", "10 kΩ 多圈", "3296W", "1", "TP1 幅度校准"]],
      widths=[1.3, 2.2, 5.4, 2.3, 1.0, 4.4], size=8)
para("合计约 ¥80–120（不含 PCB 与夹具）；继电器与运放是大头。", first=0.74)

# ---------------- 网络表 ----------------
h1("六、关键网络连接表")
para("画原理图时按这张表逐条核对，可避免“引脚对、网络名错”的低级错误。", first=0.74)
table(["网络名", "连接点（源 → 目的）", "说明"],
      [["MCO_8M", "PA8（MCO1）→ 串 33–100 Ω → AD9833 pin5（MCLK）", "8 MHz，f0 与 fs 同源"],
       ["SPI1_SCK / MOSI", "PA5 / PA7 → AD9833 pin7 / pin6", "SPI 模式 2，MSB first"],
       ["FSYNC", "PA4 → AD9833 pin8", "软件片选（低有效）"],
       ["TP1 / F+", "U3 输出 → 夹具 F+ → 通道 1 的 AC 耦合电容", "题目要求的测试点"],
       ["S+", "夹具 S+ → 1 kΩ → BAT54S/100 pF → U5A 输入", "测 DUT 高端实际电压"],
       ["F−", "夹具 F− → U4 反相输入（求和点）", "电流流入虚地"],
       ["S−", "夹具 S− 与 F− 短接（本版）", "v2 可引到第三通道做完全四线"],
       ["Vo", "U4/BUF634A 输出 → 图 3 右母线 → 通道 2 的 AC 耦合电容", "代表电流"],
       ["VREF_AFE", "U2B(OPA2810) 输出 → 虚地节点、U4 同相端、保护环驱动", "3.3 V 低阻"],
       ["VBIAS", "U6B 输出 → 两路 100 kΩ → U5A/U5B 同相端", "1.65 V"],
       ["ADC1_IN0 / ADC2_IN1", "U5A/U5B 输出 → 100 Ω → 1 nF → PA0 / PA1", "同时触发采样"],
       ["REL1–REL6", "PD0–PD5（或 74HC595）→ ULN2803 → 继电器线圈", "同一时刻只吸合一只"],
       ["CAL_SW", "PD6 → ULN2803 → K7 线圈", "校准模式"],
       ["VIN_5V", "KF301 / Type-C VBUS → 双肖特基 ORing → 5 V 轨", "外部 5 V 优先"]],
      widths=[3.0, 8.6, 4.4], size=8)

# ---------------- 画板与调试 ----------------
h1("七、画板与调试要点")
h2("7.1 PCB 布局")
bullet("分区顺序：电源（左）→ 激励（中）→ 夹具（右前）→ I-V（右后）→ 调理/ADC（最右）；模拟区与数字区之间不割地。")
bullet("L2 完整地平面；L3 分区电源（+5V / +9VA / +3.3VD / +3.3VA / +1.65V）；割电源不割地。")
bullet("求和点（U4 反相输入）是最高阻节点：走线 ≤5 mm，四周 guard 环，guard 由 VREF_AFE 经 100 Ω 驱动。")
bullet("继电器线圈电流（每只 ~30 mA）就近加 100 µF + 续流二极管，线圈地与模拟地只在电源入口单点相连。")
bullet("夹具安装孔用 M3 固定在板边，测量时被测件必须夹紧不动；夹具到板子的引线 ≤30 mm 且成对绞合。")
h2("7.2 上电顺序（第一次通电务必按这个来）")
table(["步骤", "动作", "合格判据"],
      [["1", "只焊电源部分，5 V 输入限流 300 mA", "5 V 轨 4.9–5.1 V，9 V 轨 8.8–9.2 V，3.3 V 轨 3.25–3.35 V，静态电流 <60 mA"],
       ["2", "量参考：VREF_AFE 节点", "3.300 V ± 50 mV；示波器 AC 耦合 <1 mVrms"],
       ["3", "焊激励链，TP1 空载", "0.5 Vrms ±5%，四个频点波形干净"],
       ["4", "焊 I-V + 通道，夹具短接", "读数约为 0 Ω（<50 mΩ），相位接近 0°"],
       ["5", "接 1 kΩ 0.1% 标准电阻", "读数 1 kΩ ±1%（未校准）；做一次通道校准后 ±0.3%"],
       ["6", "接 10 kΩ + 100 nF 并联", "|Z| 与相位与理论值差 <2%"]],
      widths=[1.2, 7.6, 7.2])
h2("7.3 校准顺序（每次改板/换夹具都要重做）")
bullet("通道失配 α：K7 切到 CAL，四个频率各测一次，存 flash。")
bullet("开路：夹具空置，每个频率 × 每个量程各测一次，存 Zopen。")
bullet("短路：粗铜线短接夹具，激励降到约 5 mVrms（否则 10 Ω 档电流 5 A），每个量程测一次，存 Zshort。")
bullet("负载：1 kΩ（必要时 100 Ω、10 kΩ）0.1% 标准电阻，每量程一次，得到 αload。")
para("测量时按 Zx = −Zf·α·(Vin−VREF)/(Vo−VREF)，再做开路/短路修正："
     "Zx = (Zm − Zs)(Zo − Zs)/(Zo − Zm)。", first=0.74)

h1("八、和题目指标的对应关系")
table(["题目要求", "本设计如何满足"],
      [["激励 100 Hz–100 kHz、0.5 Vrms ±10%、频率误差 ≤0.1%",
        "AD9833 + MCO 同源 8 MHz；MCLK 精度即晶振精度（±20 ppm）；VR1 校准幅度"],
       ["R 10 Ω–100 kΩ ≤1%；C 1 nF–10 µF ≤2%；L 100 µH–100 mH ≤2%",
        "六档 Rf + 四端夹具 + 通道/开路/短路校准；误差预算 RSS ≈0.2%（校准后）"],
       ["自动识别 R/L/C",
        "由 φ 判断：0°→R、−90°→C、+90°→L；同时给出 D 或 Q"],
       ["自动量程、单次 ≤1 s",
        "粗测一次（1 kHz）后按表选档；100 kHz 采集 1024 点仅 640 µs"],
       ["100 kHz 测 10 pF ≤5%",
        "K5 档 + 每次开路校准；关键在于杂散电容稳定在 ±0.25 pF 以内（夹具几何固定 + guard 环）"],
       ["100 kHz 测 100 pF–100 nF、10 µH–1 mH",
        "K2–K5 档；C 用 100 kHz，L 用 10 kHz 或更低；10 µH 需降激励或加 4.7 Ω 档"]],
      widths=[7.4, 8.8])

doc.save(OUT)
print("saved", OUT)




