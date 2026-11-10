#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""compute/numbers.py -- 《RLC 电桥测量装置原理详解》全部数值核算。

运行：python compute/numbers.py
输出：verify/numbers.md（本脚本直接生成，重跑应逐字节一致）

约定：
  * 激励 V_IN=0.5 Vrms，V_REF=2.5 V，单电源 +5 V；ADC 满量程 3.3 V、12 bit、N=1024。
  * 全篇用复阻抗；|Z| 指模值；Q=|X|/R；D=R/|X|=1/Q。
  * 频率档：100 Hz / 1 kHz / 10 kHz / 100 kHz。
  * 所有百分数均为相对误差的绝对值。
"""

import math
import cmath
import os
import sys

# Windows 控制台默认 GBK，打印含 µ/Ω/° 的正文前先切到 UTF-8
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

PI = math.pi
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_PATH = os.path.join(ROOT, "verify", "numbers.md")

# ----------------------------------------------------------------------------
# 公共参数
# ----------------------------------------------------------------------------
V_IN = 0.5          # Vrms
V_REF = 2.5         # V
V_SUP = 5.0         # V
ADC_FS = 3.3        # V
ADC_BITS = 12
N_DFT = 1024
FREQS = [100.0, 1e3, 1e4, 1e5]           # Hz
ENOB = 10.5                              # 估算用（12 bit ADC 的实际有效位数假设）

# 元件取值（每个数量级至少一个点）
C_BASIC = [1e-9, 1e-8, 1e-7, 1e-6, 10e-6]
C_HF = [100e-12, 1e-9, 10e-9, 100e-9]
L_BASIC = [100e-6, 1e-3, 10e-3, 100e-3]
L_HF = [10e-6, 100e-6, 1e-3]
R_LIST = [10.0, 100.0, 1e3, 1e4, 1e5, 1e6]

# 量程电阻档位
RF_LIST = [10.0, 100.0, 1e3, 1e4, 1e5, 1e6]

# ----------------------------------------------------------------------------
# 格式化小工具
# ----------------------------------------------------------------------------
def sig(x, n=3):
    """n 位有效数字；|x|<1e-4 或 >=1e6 时用 e 记法。"""
    if x == 0:
        return "0"
    ax = abs(x)
    e = math.floor(math.log10(ax))
    if -4 <= e <= 5:
        d = max(0, n - 1 - e)
        return f"{x:.{d}f}"
    m = x / (10 ** e)
    return f"{m:.{n-1}f}e{e:+d}"


_PREFIX = [(1e9, "G"), (1e6, "M"), (1e3, "k"), (1.0, ""),
           (1e-3, "m"), (1e-6, "µ"), (1e-9, "n"), (1e-12, "p")]


def eng(x, unit="", n=3):
    """工程记数：1.59 kΩ / 159 Ω / 1.00 µF / 500 mVrms ..."""
    if x == 0:
        return "0" + (" " + unit if unit else "")
    for scale, pre in _PREFIX:
        if abs(x) >= scale:
            return sig(x / scale, n) + (" " + pre if pre else " ") + unit
    return sig(x * 1e12, n) + " p" + unit


def pctf(frac, n=3):
    """0.02 -> '2.00%'"""
    return sig(frac * 100.0, n) + "%"


def pctv(val, n=3):
    """2.0 -> '2.00%'（val 已经是百分数）"""
    return sig(val, n) + "%"


def deg(rad, n=3):
    return sig(rad * 180.0 / PI, n) + "°"


def hz(f, n=3):
    return eng(f, "Hz", n)


def ohm(z, n=3):
    return eng(z, "Ω", n)


def farad(c, n=3):
    return eng(c, "F", n)


def henry(l, n=3):
    return eng(l, "H", n)


def volt(v, n=3):
    return eng(v, "Vrms", n)


def amp(i, n=3):
    return eng(i, "A", n)


# ----------------------------------------------------------------------------
# |Z|
# ----------------------------------------------------------------------------
def zc(c, f):
    return 1.0 / (2.0 * PI * f * c)


def zl(l, f):
    return 2.0 * PI * f * l


def pick(freqs, vals, lo, hi, mode):
    cand = [(f, v) for f, v in zip(freqs, vals) if lo <= v <= hi]
    if not cand:
        return None
    return (max if mode == "highest" else min)(cand, key=lambda t: t[0])


def pick_text(p, freqs, vals):
    if p is None:
        return "无（max|Z|=%s）" % ohm(max(vals))
    return hz(p[0])


def pick_z(p):
    return "—" if p is None else ohm(p[1])


# ----------------------------------------------------------------------------
# 报告缓冲
# ----------------------------------------------------------------------------
LINES = []


def w(s=""):
    LINES.append(s)


def table(headers, rows):
    # 单元格里的 "|" 必须转义，否则 markdown 表格会被截断
    esc = lambda x: str(x).replace("|", "\\|")
    w("| " + " | ".join(esc(h) for h in headers) + " |")
    w("|" + "|".join(["------"] * len(headers)) + "|")
    for r in rows:
        w("| " + " | ".join(esc(c) for c in r) + " |")
    w()


# ----------------------------------------------------------------------------
# 公共派生量
# ----------------------------------------------------------------------------
Q_LSB = ADC_FS / (2 ** ADC_BITS)
SIGMA_Q = Q_LSB / math.sqrt(12.0)
SIGMA_A = SIGMA_Q * math.sqrt(2.0 / N_DFT)
ENOB_FACTOR = 2 ** (ADC_BITS - ENOB)
SIGMA_A_EFF = SIGMA_A * ENOB_FACTOR
ADC_PEAK = ADC_FS / 2.0
VO_MAX_DIRECT = (ADC_FS - V_REF) / math.sqrt(2.0)       # 直接耦合、偏置 2.5 V 时允许的 |Vo|
VO_MAX_AC = ADC_PEAK / math.sqrt(2.0)                    # 交流耦合并偏置到 1.65 V
RATIO_MAX_ADC = (ADC_FS - V_REF) / (math.sqrt(2.0) * V_IN)
VO_PEAK_RATIO1 = math.sqrt(2.0) * V_IN
MARGIN_RATIO1 = ADC_FS - (V_REF + VO_PEAK_RATIO1)
FS_UTIL_RATIO1 = VO_PEAK_RATIO1 / ADC_PEAK
ZS_RS = 0.1
ZS_LS = 20e-9

# ----------------------------------------------------------------------------
# 00. 假设与模型
# ----------------------------------------------------------------------------
w("# RLC 电桥测量装置——正文引用数值核算（verify/numbers.md）")
w()
w("> 本文件由 `compute/numbers.py` 自动生成：文中每个数值都来自脚本计算，" )
w("> 重跑 `python compute/numbers.py` 应得到逐字节相同的本文件。" )
w()
w("## 0. 共同假设、符号与电桥模型")
w()
w("| 符号 | 取值 | 说明 |")
w("| --- | --- | --- |")
w(f"| `V_IN` | {volt(V_IN)} | 激励（TP1）正弦有效值 |")
w(f"| `V_REF` | {sig(V_REF)} V | 运放同相端偏置（虚地），单电源 +5 V 的中点 |")
w(f"| `V_SUP` | {sig(V_SUP)} V | 唯一外部电源 |")
w(f"| `ADC_FS` / 位数 | {sig(ADC_FS)} V / {ADC_BITS} bit | ADC 满量程与分辨率 |")
w(f"| `N` | {N_DFT} | 单频点 DFT 采样点数（相干采样） |")
w(f"| 频率档 | 100 Hz / 1 kHz / 10 kHz / 100 kHz | 基本 3 档 + 发挥 100 kHz |")
w()
w("**电桥关系（含有限开环增益的精确式，Lead 更正版）：**")
w()
w("```")
w("V_o - V_REF = - (Z_f/Z_x) * (V_in - V_REF) / [ 1 + (1 + Z_f/Z_x)/A_ol ]")
w("理想（A_ol -> inf）：Z_x = - Z_f * (V_in - V_REF)/(V_o - V_REF)")
w("记 r = |Z_f|/|Z_x|，误差因子  eps0 = (1 + r) / A_ol")
w("单极点运放、f >> f_p：A_ol = - j*GBW/f  =>  1/A_ol = + j*f/GBW")
w("  => Z_meas = Z_x * (1 + j*eps)，eps = (1+r)*f/GBW（实数）")
w("  => 相位误差 ≈ eps rad（一阶）；幅值误差 = sqrt(1+eps^2)-1 ≈ eps^2/2（二阶）")
w("  => 若 A_ol 带实部（多极点/复合运放）：幅值误差退化为一阶 ≈ eps")
w("```")
w()
w(f"量程设计取 `r = R_f/|Z_x|` 在 0.1–1 之间（见 E 节），故 `1+r` 在 1.1–2 之间，本报告取最坏 `r=1`，即 `eps = 2f/GBW`。")
w()

# ----------------------------------------------------------------------------
# A. |Z| 与推荐测试频率
# ----------------------------------------------------------------------------
w("## A. |Z| 与推荐测试频率")
w()
w("公式：`|Z_C| = 1/(2πfC)`，`|Z_L| = 2πfL`，`|Z_R| = R`。")
w(f"测试电流 `I = V_IN/|Z|`；I-V 输出幅度 `|V_o| = V_IN·R_f/|Z|`（R_f 为量程电阻）。")
w()
w("### A.1 基本电容档 C = 1 nF–10 µF：各频率下的 |Z|")
rows = []
for c in C_BASIC:
    vals = [zc(c, f) for f in FREQS]
    inwin = [f for f, v in zip(FREQS, vals) if 1e3 <= v <= 1e5]
    rows.append([farad(c)] + [ohm(v) for v in vals] +
                ["、".join(hz(f) for f in inwin) if inwin else "无"])
table(["电容值", "|Z|@100 Hz", "|Z|@1 kHz", "|Z|@10 kHz", "|Z|@100 kHz",
       "∈[1 kΩ,100 kΩ] 的频点"], rows)

w("### A.2 发挥 100 kHz 电容档 C = 100 pF–100 nF（规定在 100 kHz 测）")
rows = []
for c in C_HF:
    z = zc(c, 100e3)
    rows.append([farad(c), ohm(z), "是" if 1e3 <= z <= 1e5 else "否",
                 amp(V_IN / z)])
table(["电容值", "|Z|@100 kHz", "∈[1 kΩ,100 kΩ]?", "I@0.5 Vrms"], rows)

w("### A.3 基本电感档 L = 100 µH–100 mH：各频率下的 |Z|")
rows = []
for l in L_BASIC:
    vals = [zl(l, f) for f in FREQS]
    inwin = [f for f, v in zip(FREQS, vals) if 1e3 <= v <= 1e5]
    rows.append([henry(l)] + [ohm(v) for v in vals] +
                ["、".join(hz(f) for f in inwin) if inwin else "无"])
table(["电感值", "|Z|@100 Hz", "|Z|@1 kHz", "|Z|@10 kHz", "|Z|@100 kHz",
       "∈[1 kΩ,100 kΩ] 的频点"], rows)

w("### A.4 发挥 100 kHz 电感档 L = 10 µH–1 mH（规定在 100 kHz 测）")
rows = []
for l in L_HF:
    z = zl(l, 100e3)
    rows.append([henry(l), ohm(z), "是" if 1e3 <= z <= 1e5 else "否",
                 amp(V_IN / z)])
table(["电感值", "|Z|@100 kHz", "∈[1 kΩ,100 kΩ]?", "I@0.5 Vrms"], rows)

w("### A.5 电阻 R = 10 Ω–1 MΩ：|Z| 与频率无关")
rows = []
for r in R_LIST:
    rows.append([ohm(r), ohm(r), "是" if 1e3 <= r <= 1e5 else "否",
                 amp(V_IN / r), "R 的 |Z| 与 f 无关，频率只影响运放误差/杂散"])
table(["电阻值", "|Z|=R", "∈[1 kΩ,100 kΩ]?", "I@0.5 Vrms", "说明"], rows)

# ---- A.6 推荐频率表 ----
w("### A.6 推荐测试频率表（“让 |Z| 落在窗口内的最高频率” vs 工程修正）")
w()
w("严格规则：在 100 Hz/1 kHz/10 kHz/100 kHz 中，取使 |Z| 落在 1 kΩ–100 kΩ 的**最高**频率。")
w("工程修正：窗口放宽为 100 Ω–100 kΩ；电容仍取最高频（|Z| 越接近下限电流越大、信噪比越好），"
  "电感取**最低**频（否则 |Z| 逼近 100 kΩ，电流小且 1 pF 夹具杂散误差被放大，见 A.8），电阻固定 1 kHz。")
w()
rows = []
for c in C_BASIC:
    vals = [zc(c, f) for f in FREQS]
    sp = pick(FREQS, vals, 1e3, 1e5, "highest")
    ep = pick(FREQS, vals, 100, 1e5, "highest")
    note = ""
    if sp is None:
        note = "严格窗口无解（100 Hz 时仅 %s）" % ohm(max(vals))
    rows.append(["C " + farad(c), pick_text(sp, FREQS, vals), pick_z(sp),
                 pick_text(ep, FREQS, vals) if ep else "无（max %s）" % ohm(max(vals)),
                 pick_z(ep) if ep else "—", note])
for l in L_BASIC:
    vals = [zl(l, f) for f in FREQS]
    sp = pick(FREQS, vals, 1e3, 1e5, "highest")
    ep = pick(FREQS, vals, 100, 1e5, "lowest")
    note = ""
    if l <= 100e-6 * 1.0001:
        note = "100 Ω 窗口仍不够（max %s），需再降到 50 Ω 或降电平" % ohm(max(vals))
    elif sp is None:
        note = "严格窗口无解（max %s）" % ohm(max(vals))
    elif ep is not None and sp is not None and abs(sp[1] - ep[1]) > 1:
        note = "严格规则选到 100 kHz（%s），1 pF 杂散会放大误差，建议 %s" % (
            ohm(sp[1]), hz(ep[0]))
    rows.append(["L " + henry(l), pick_text(sp, FREQS, vals), pick_z(sp),
                 pick_text(ep, FREQS, vals) if ep else "无（max %s）" % ohm(max(vals)),
                 pick_z(ep) if ep else "—", note])
for r in R_LIST:
    vals = [r] * 4
    sp = pick(FREQS, vals, 1e3, 1e5, "highest")
    rows.append(["R " + ohm(r), pick_text(sp, FREQS, vals) if sp else "无",
                 pick_z(sp) if sp else "—", "1 kHz（固定）", ohm(r),
                 "|Z| 与 f 无关；频率取 1 kHz 以避开运放/杂散误差"])
for c in C_HF:
    z = zc(c, 100e3)
    rows.append(["C(100 kHz 档) " + farad(c), "100 kHz（规定）", ohm(z),
                 "100 kHz（规定）", ohm(z),
                 "在窗口内" if 1e3 <= z <= 1e5 else "低于下限，电流 %s" % amp(V_IN / z)])
for l in L_HF:
    z = zl(l, 100e3)
    rows.append(["L(100 kHz 档) " + henry(l), "100 kHz（规定）", ohm(z),
                 "100 kHz（规定）", ohm(z),
                 "在窗口内" if 1e3 <= z <= 1e5 else "低于下限，电流 %s" % amp(V_IN / z)])
table(["元件/取值", "严格规则频率", "该频 |Z|", "工程修正频率", "该频 |Z|", "说明"], rows)

# ---- A.7 覆盖性检验 ----
w("### A.7 规则覆盖性检验：1 kΩ–100 kΩ 窗口能否覆盖全部规定范围？")
w()
c_max_strict = 1.0 / (2 * PI * FREQS[0] * 1e3)
c_max_100 = 1.0 / (2 * PI * FREQS[0] * 100.0)
l_min_strict = 1e3 / (2 * PI * FREQS[-1])
l_min_100 = 100.0 / (2 * PI * FREQS[-1])
l_min_50 = 50.0 / (2 * PI * FREQS[-1])
c_hf_max_strict = 1.0 / (2 * PI * FREQS[-1] * 1e3)
c_hf_max_100 = 1.0 / (2 * PI * FREQS[-1] * 100.0)
c_hf_max_50 = 1.0 / (2 * PI * FREQS[-1] * 50.0)
l_hf_zmax = zl(1e-3, 100e3)
z_10u = zl(10e-6, 100e3)
w(f"窗口下限 1 kΩ 对应 100 Hz 时 C ≤ {farad(c_max_strict)}，或 100 kHz 时 L ≥ {henry(l_min_strict)}；"
  "窗口上限 100 kΩ 对 C/L 实际不起限制（总可以选更高频率把 |Z| 压下来），真正限制覆盖范围的是下限："
  f"10 µF 电容在 100 Hz 时 |Z| 只有 {ohm(zc(10e-6, 100))}，100 µH 电感在 100 kHz 时只有 {ohm(zl(100e-6, 100e3))}，都低于下限。")
w()
rows = [
    ["C 基本 1 nF–10 µF",
     f"1 nF–{farad(c_max_strict)}",
     f"{farad(c_max_strict)}–10 µF 不覆盖；10 µF 最大 |Z| = {ohm(zc(10e-6, 100))}（100 Hz）",
     f"1 nF–{farad(c_max_100)}", "全部"],
    ["C 发挥档 100 pF–100 nF（100 kHz 固定）",
     f"100 pF–{farad(c_hf_max_strict)}",
     f"{farad(c_hf_max_strict)}–100 nF 不覆盖；100 nF 仅 {ohm(zc(100e-9, 100e3))}",
     f"100 pF–{farad(c_hf_max_100)}", f"100 pF–{farad(c_hf_max_50)}"],
    ["L 基本 100 µH–100 mH",
     f"{henry(l_min_strict)}–100 mH",
     f"100 µH–{henry(l_min_strict)} 不覆盖；100 µH 最大 |Z| = {ohm(zl(100e-6, 100e3))}，1 mH = {ohm(zl(1e-3, 100e3))}",
     f"{henry(l_min_100)}–100 mH", f"{henry(l_min_50)}–100 mH"],
    ["L 发挥档 10 µH–1 mH（100 kHz 固定）",
     "无（全段 < 1 kΩ）",
     f"最大 |Z| 在 1 mH 处只有 {ohm(l_hf_zmax)}，10 µH 只有 {ohm(z_10u)}",
     "无", "无（10 µH 只有 %s）" % ohm(z_10u)],
    ["R 基本 10 Ω–100 kΩ",
     "1 kΩ–100 kΩ",
     "10 Ω–1 kΩ 不覆盖（|Z| 与 f 无关，频率无法补救）",
     "100 Ω–100 kΩ", "100 Ω–100 kΩ"],
    ["R 自动量程 10 Ω–1 MΩ",
     "1 kΩ–100 kΩ",
     "10 Ω–1 kΩ 与 100 kΩ–1 MΩ 不覆盖（频率无法补救，只能靠 R_f 量程）",
     "100 Ω–100 kΩ", "100 Ω–100 kΩ"],
]
table(["规定范围", "严格 [1 kΩ,100 kΩ] 可覆盖", "未覆盖端点及原因",
       "[100 Ω,100 kΩ] 可覆盖", "[50 Ω,100 kΩ] 可覆盖"], rows)
w("**结论：**")
w(f"1. 严格规则覆盖不了 6 项中的 5 项（仅 C 基本档 1 nF–{farad(c_max_strict)} 可）："
  f"C 10 µF（{ohm(zc(10e-6, 100))}）、C 发挥档 100 nF（{ohm(zc(100e-9, 100e3))}）、"
  f"L 100 µH（{ohm(zl(100e-6, 100e3))}）、L 发挥档全段（{ohm(z_10u)}–{ohm(l_hf_zmax)}）、R 的 10 Ω/100 Ω/1 MΩ。")
w("2. 频率选择只对 C/L 有效；R 的 |Z| 与 f 无关，R 的 10 Ω–1 kΩ 与 100 kΩ–1 MΩ 段只能靠 R_f 量程解决（E 节）。")
w("3. 把下限放宽到 100 Ω 后，C 基本档全部覆盖；L 基本档 100 µH 仍差（62.8 Ω < 100 Ω）；L 发挥档 10 µH（6.28 Ω）无任何窗口可覆盖。")
w("4. 若继续把下限降到 50 Ω（I ≤ 10 mA），L 100 µH 可进入窗口；但 L 10 µH（6.28 Ω）即使 10 Ω 窗口也覆盖不了，"
  "且 0.5 Vrms 下电流达 79.6 mA——这是本设计最硬的端点之一（见 E 节）。")
w()

# ---- A.8 窗口的物理理由 ----
w("### A.8 窗口上下限的物理理由（电流、摆幅、引线电阻）")
w()
rows = []
for z in [1e6, 1e5, 1e3, 100.0, 50.0, 10.0, z_10u]:
    i = V_IN / z
    lead = 0.1 / z
    if z >= 1e5:
        note = "电流过小，杂散/漏电/偏置电流占比上升"
    elif z <= 100.0:
        note = "电流大，需 100 mA 级缓冲；0.1 Ω 引线已占 %.2g%%" % (lead * 100)
    else:
        note = "常用区"
    rows.append([ohm(z), amp(i), pctf(lead), note])
table(["|Z|", "I = 0.5 Vrms/|Z|", "0.1 Ω 引线/接触占比", "说明"], rows)
w(f"另外：ADC 直接耦合、偏置 {sig(V_REF)} V 时，输出上限 {sig(ADC_FS)} V 只留正摆幅 "
  f"{sig(ADC_FS - V_REF)} V，故 |V_o| ≤ {volt(VO_MAX_DIRECT)}（峰值 {sig(VO_PEAK_RATIO1)} V 时已到 {sig(V_REF + VO_PEAK_RATIO1)} V，余量仅 {sig(MARGIN_RATIO1*1000)} mV）。"
  f"若交流耦合并重新偏置到 {sig(ADC_PEAK)} V，则 |V_o| ≤ {volt(VO_MAX_AC)}。")
w("总结：|Z| 过小时 |V_o| = V_IN·R_f/|Z_x| 会超过 ADC/运放摆幅（E 节 ratio>1 即削顶），|Z| 过大时电流过小、"
  "杂散/偏置电流/引线电阻占比上升——这就是选频窗口 [1 kΩ, 100 kΩ] 的物理依据。")
w()

# ----------------------------------------------------------------------------
# B. 运放有限开环增益（Lead 更正版：复数 A_ol）
# ----------------------------------------------------------------------------
w("## B. 运放有限开环增益：相位是一阶、幅值是二阶")
w()
w("由第 0 节精确传递函数，`Z_meas = Z_x·[1 + eps0]`，`eps0 = (1+r)/A_ol`。单极点运放在 f≫f_p 时")
w("`A_ol = -j·GBW/f`，所以 `1/A_ol = +j·f/GBW`，`eps0 = j·eps`，`eps = (1+r)·f/GBW`（实数）。于是")
w()
w("```")
w("|Z_meas|/|Z_x| = sqrt(1 + eps^2)  =>  幅值相对误差 = sqrt(1+eps^2) - 1 ≈ eps^2/2   （二阶）")
w("arg(Z_meas/Z_x) = atan(eps)         =>  相位误差 ≈ eps rad                   （一阶）")
w("若 A_ol 带实部（多极点/复合运放）：因子 1+eps 为实数 => 幅值误差 ≈ eps/(1+eps)  （一阶）")
w("```")
w()
w("取 r=1（Z_f=Z_x，即 β=0.5，量程窗口内最坏情况；r 在 0.1–1 时 `1+r` 在 1.1–2）。")
w("原始简报里的 β=1 情形对应 r=0（Z_f≪Z_x），此时 `1+r`=1、ε=f/GBW，各误差恰为上表的一半——"
  "所以 r=1/β=0.5 这列才是保守值。")
w()

def b_row(gbw, f):
    eps = 2.0 * f / gbw
    phase = math.atan(eps)
    amp2 = math.sqrt(1.0 + eps * eps) - 1.0
    worst = eps / (1.0 + eps)
    return [sig(gbw / 1e6, 3) + " MHz", sig(eps), deg(phase), pctf(amp2), pctf(worst)]

for f, title in [(1e4, "B.1 f = 10 kHz"), (1e5, "B.2 f = 100 kHz")]:
    w(f"### {title}（r = 1，即 Z_f = Z_x）")
    rows = [b_row(g, f) for g in [10e6, 20e6, 50e6, 100e6, 200e6]]
    table(["GBW", "|ε|", "相位误差", "幅值误差（单极点，二阶）",
           "幅值误差（A_ol 有实部，最坏）"], rows)

w("### B.3 GBW 需求反推")
w()
w("```")
w("幅值（单极点、二阶）：sqrt(1+eps^2)-1 = t  =>  eps = sqrt((1+t)^2-1)  =>  GBW = 2f/eps")
w("幅值（A_ol 有实部、最坏）：eps/(1+eps) = t  =>  eps = t/(1-t)          =>  GBW = 2f/eps")
w("相位：eps = tan(delta)                                                =>  GBW = 2f/tan(delta)")
w("```")
w()
def gbw_amp2(t, f):
    eps = math.sqrt((1.0 + t) ** 2 - 1.0)
    return 2.0 * f / eps

def gbw_worst(t, f):
    eps = t / (1.0 - t)
    return 2.0 * f / eps

def gbw_phase(delta_deg, f):
    return 2.0 * f / math.tan(delta_deg * PI / 180.0)

rows = []
for name, t, f in [
        ("R 自动量程 0.5% @ 100 kHz", 0.005, 1e5),
        ("R 自动量程 0.5% @ 1 kHz", 0.005, 1e3),
        ("R 基本 1% @ 100 kHz", 0.01, 1e5),
        ("C/L 2% @ 100 kHz", 0.02, 1e5),
]:
    rows.append([name, pctf(t), hz(f), eng(gbw_amp2(t, f), "Hz"),
                 eng(gbw_worst(t, f), "Hz")])
for name, d in [("相位误差 ≤ 0.1° @ 100 kHz", 0.1),
                ("相位误差 ≤ 0.573° @ 100 kHz", 0.573),
                ("D=0.01 测到 10%（δ≤0.0573°）@ 100 kHz", 0.0573),
                ("D=0.001 测到 10%（δ≤0.00573°）@ 100 kHz", 0.00573)]:
    rows.append([name, "—", "100 kHz", "—", eng(gbw_phase(d, 1e5), "Hz")])
table(["指标/条件", "目标", "f", "GBW 需求（单极点幅值）", "GBW 需求（相位或最坏实部）"], rows)

eps20 = 2.0 * 1e5 / 20e6
phase20 = math.atan(eps20)
amp20 = math.sqrt(1.0 + eps20 * eps20) - 1.0
worst20 = eps20 / (1.0 + eps20)
w(f"**回答：100 kHz 下 20 MHz GBW 运放**：`|ε| = {sig(eps20)}`，相位误差 ≈ **{deg(phase20)}**，"
  f"幅值误差（单极点、二阶）≈ **{pctf(amp20)}**；若 A_ol 有实部则幅值误差 ≈ **{pctf(worst20)}**。")
w()
w(f"* 对 C/L 的**数值**（2%）远远够用：幅值只差 {pctf(amp20)}；对 R（1%/0.5%）也够（单极点时）。")
w(f"* 但相位 {deg(phase20)} 折算成损耗因数 D 的误差约 {sig(phase20)}（= 1%），"
  f"只能把 D≈0.1 测到 10%，测不了 D=0.01；做元件类型判别（|X|/R=0.1 分界，相位差 5.71°）则刚好在 10% 边界误差的临界值（见 C 节）。")
w(f"* 若 A_ol 带实部（复合/多级运放），幅值误差一阶达 {pctf(worst20)}，会单独吃掉 0.5% 量程指标——选型时须确认 100 kHz 处 A_ol 仍以单极点虚部为主。")
w()

# ----------------------------------------------------------------------------
# C. 12 bit ADC + 单频点 DFT 的分辨率（Lead 更正版）
# ----------------------------------------------------------------------------
w("## C. 12 bit ADC + 单频点 DFT 的幅值/相位分辨率")
w()
w("### C.1 估算前提")
w()
w("```")
w("LSB  q = ADC_FS/2^12；量化噪声 σ_q = q/sqrt(12)（均匀分布、白噪声假设）")
w("相干采样（信号正好落在整数 bin）、单频点 DFT、矩形窗、高信噪比近似：")
w("  复幅度估计 A_hat = 2|X_k|/N；幅度噪声 σ_A = σ_q*sqrt(2/N)；")
w("  相位噪声 σ_φ = σ_A/A_peak （rad）；A_peak = sqrt(2)*|V_o,rms|")
w("噪声按每通道独立处理，两通道比值：ε_|Z| = sqrt(ε_A(V_o)^2 + ε_A(V_in)^2)")
w("```")
w()
w(f"q = {eng(Q_LSB, 'V')}；σ_q = {eng(SIGMA_Q, 'V')}；N={N_DFT} 时 σ_A = {eng(SIGMA_A, 'V')}（纯量化）；"
  f"取 ENOB={sig(ENOB)} 时 σ_A,eff = {eng(SIGMA_A_EFF, 'V')}（×{sig(ENOB_FACTOR)}）。")
w()

w("### C.2 单通道幅值/相位分辨率（量化 + ENOB 两种情景）")
rows = []
for vo in [1.0, 0.5, 0.2, 0.1, 0.05]:
    apeak = math.sqrt(2.0) * vo
    ea_q = SIGMA_A / apeak
    sf_q = ea_q
    ea_e = SIGMA_A_EFF / apeak
    sf_e = ea_e
    rows.append([volt(vo), sig(apeak, 3) + " V", pctf(ea_q), deg(sf_q),
                 pctf(ea_e), deg(sf_e), sig(sf_e) + " rad"])
table(["|V_o| (rms)", "A_peak", "ε_A（纯量化）", "σ_φ（纯量化）",
       "ε_A（ENOB 10.5）", "σ_φ（ENOB 10.5）", "σ_φ（rad）"], rows)
w("注：|V_o| 的上限由 D 节量程设计给出；|V_in|=0.5 Vrms 通道自身噪声更小，"
  "比值中 |V_o| 通道通常主导。上表是单通道结果。")
w()

w("### C.3 折算到 |Z| 相对误差、D 分辨率与两通道合成")
rows = []
for vo in [0.5, 0.2, 0.1, 0.05]:
    apeak = math.sqrt(2.0) * vo
    ea_vo = SIGMA_A_EFF / apeak
    ea_vin = SIGMA_A_EFF / (math.sqrt(2.0) * V_IN)
    ea_ratio = math.sqrt(ea_vo ** 2 + ea_vin ** 2)
    sf_vo = ea_vo
    sf_vin = ea_vin
    sf_ratio = math.sqrt(sf_vo ** 2 + sf_vin ** 2)
    rows.append([volt(vo), pctf(ea_ratio), deg(sf_ratio), sig(sf_ratio) + " rad",
                 "≈ " + sig(sf_ratio * 1e6, 3) + " ppm"])
table(["|V_o| (rms)", "|Z| 相对误差（幅值）", "相位分辨率", "D 分辨率（≈σ_φ）",
       "D 分辨率"], rows)
w(f"结论：12 bit + N={N_DFT} 的**幅值分辨率**在 |V_o|≥0.1 Vrms 时优于 {pctf(SIGMA_A_EFF/(math.sqrt(2.0)*0.1))}，"
  f"最差（|V_o|=0.05 Vrms）也只有 {pctf(math.sqrt((SIGMA_A_EFF/(math.sqrt(2.0)*0.05))**2 + (SIGMA_A_EFF/(math.sqrt(2.0)*V_IN))**2))}；"
  f"**相位分辨率**优于 {sig(SIGMA_A_EFF/(math.sqrt(2.0)*0.1)*1e6, 3)} ppm（rad），即 D 的分辨率优于 1e-3 量级。"
  "量化/DFT 不是 2% / 1% 指标的瓶颈。")
w()

w("### C.4 相位误差对 C 值的影响（一阶体现在 D 上，不是 C 上）")
w()
w("```")
w("纯电容 Z = -j/(ωC)：相位旋转 δ 后 |Z| 不变 => dC/C = δ^2/2（二阶）")
w("有损耗电容（D=R/|X|）：C = 1/(ω·|Z|·sqrt(1+D^2))；相位误差 δ 使表观 D 变化 ≈ δ")
w("  => dC/C ≈ D·δ（一阶项被 D 压小）")
w("一般公式（Z=R+jX, Q=|X|/R）：dR/R = ε_A - Q·δ；dX/X = ε_A + δ/Q")
w("```")
w()
w(f"若指标只由相位误差决定：纯 C 要达到 2% 的 C 值误差，允许 δ = sqrt(2×0.02) = {deg(math.sqrt(2*0.02))}；"
  f"要达到 0.5%，允许 δ = sqrt(2×0.005) = {deg(math.sqrt(2*0.005))}。"
  "实际 2%/0.5% 指标主要由幅值精度承担（C.6），相位只要远小于上述上限即可。")
w()
rows = []
for d_deg in [0.1, 0.573, 0.115, 1.146]:
    d_rad = d_deg * PI / 180.0
    second = d_rad * d_rad / 2.0
    rows.append([sig(d_deg, 3) + "°", sig(d_rad, 3) + " rad", pctf(second),
                 pctf(0.1 * d_rad), pctf(0.01 * d_rad), sig(d_rad, 3)])
table(["相位误差 δ", "δ (rad)", "纯 C: dC/C=δ²/2", "D=0.1 时 dC/C≈D·δ",
       "D=0.01 时 dC/C≈D·δ", "表观 D 误差≈δ"], rows)
w("对应关系：0.1° 是题设通道相位失配；0.573° 是 20 MHz GBW 在 100 kHz 的运放相位误差；"
  "0.115° 是 100 MHz GBW；1.15°（0.02 rad）是 10 MHz GBW 在 100 kHz 的运放相位误差，旧 1/(Aβ) 公式会把它误当成 2% 的幅值误差（仅作对比）。")
w()

w("### C.4b 幅值/相位分辨率折算到 R 与 X 的相对误差（Q=10 与 Q=0.1）")
w()
w("由 `Z=R+jX`、`Q=|X|/R` 的一阶误差传播：`dR/R = ε_A − Q·δ`，`dX/X = ε_A + δ/Q`"
  "（δ 为相位误差，rad；ε_A 为 |Z| 幅值相对误差；两者独立时再 RSS 合成）。")
w("取 N=1024、ENOB10.5 的量化/DFT 分辨率（相位误差 δ 与 ε_A 同源，按同量级代入）：")
rows = []
for vo in [0.1, 0.05]:
    eps_a = math.sqrt((SIGMA_A_EFF / (math.sqrt(2.0) * vo)) ** 2 +
                      (SIGMA_A_EFF / (math.sqrt(2.0) * V_IN)) ** 2)
    delta = eps_a
    for qv in [10.0, 0.1]:
        rows.append([volt(vo), sig(qv, 3), deg(delta), pctf(eps_a),
                     pctf(eps_a - qv * delta), pctf(eps_a + delta / qv)])
table(["|V_o|", "Q", "相位分辨率 δ", "幅值误差 ε_A", "dR/R（一阶）", "dX/X（一阶）"], rows)
w("Q=10 时 dR/R 由 Q·δ 主导（|V_o|=0.1 Vrms 时 −0.19%，0.05 Vrms 时 −0.37%），Q=0.1 时 dX/X 由 δ/Q 主导"
  "（+0.23% / +0.46%）；两者都远小于 2% 指标，说明 ADC 分辨率不是限制，真正限制 R/X 分离的是相位误差"
  "（GBW、通道失配）——但 R/C/L 值本身只依赖 |Z| 与损耗修正，对纯元件的一阶影响仍是上面 C.4 的二阶结论。")
w()

w("### C.5 D 测量、元件类型判别所需的相位精度")
w()
w("```")
w("分界 |X|/R = 0.1  <=> 阻抗角 atan(0.1) = 5.71°；")
w("要求分界处 (X/R) 判别误差 ≤10%  =>  δ ≈ 0.01/(1+0.1^2) rad = 0.0099 rad = 0.567°")
w("测 D 到相对误差 p：δ ≤ p·D（rad）")
w("```")
w()
rows = []
for name, d in [
    ("判别 R/C 分界 |X|/R=0.1，分界误差 ≤10%", 0.567),
    ("测 D=0.1 到 10%（δ≤0.01 rad）", 0.01 * 180 / PI),
    ("测 D=0.01 到 10%（δ≤0.001 rad）", 0.001 * 180 / PI),
    ("测 D=0.001 到 10%（δ≤0.0001 rad）", 0.0001 * 180 / PI),
    ("测 D=0.01 到 1%（δ≤0.0001 rad）", 0.0001 * 180 / PI),
]:
    rows.append([name, deg(d * PI / 180), eng(gbw_phase(d, 1e5), "Hz")])
table(["用途", "所需相位精度", "100 kHz 下用单极点运放 GBW 需 ≥"], rows)
w(f"ADC 本身的相位分辨率（|V_o|=0.1 Vrms、ENOB10.5）约 {deg(SIGMA_A_EFF/(math.sqrt(2.0)*0.1))}，"
  f"远好于上述要求；真正的相位误差主导项是运放有限 GBW（B 节）与通道间相位失配 0.1°。")
w()

# ---- 误差预算数据（G 节渲染；C.6 先引用校准后残量） ----
def rss(vals):
    return math.sqrt(sum(v * v for v in vals))


# (名称, 未校准 R%, 未校准 C/L%, 校准后 R%, 校准后 C/L%, 主要影响, 备注)
BUDGET = [
    ("R_f 阻值公差/初始精度", 0.10, 0.10, 0.02, 0.02, "R/C/L",
     "0.1% 电阻 + 出厂标定，校准后残余 0.02%"),
    ("ADC 两通道增益失配 0.05 dB", 0.577, 0.577, 0.02, 0.02, "R/C/L",
     "0.05 dB = 0.577%；比值式测量不抵消通道增益差，必须两通道增益校准"),
    ("ADC 两通道相位失配 0.1°", 0.001, 0.002, 0.001, 0.001, "R；C/L 数值（D·δ 二阶）",
     "0.1° = 1.75 mrad；对纯 C 值影响二阶可忽略，主要影响 D/ESR"),
    ("运放有限 GBW 100 MHz（单极点，100 kHz）", 0.0002, 0.0002, 0.0002, 0.0002, "R/C/L 幅值（二阶）",
     "相位 0.115°；若 A_ol 有实部幅值退化一阶 0.2%，选型需确认"),
    ("ADC 量化 + DFT + 前端噪声", 0.006, 0.041, 0.006, 0.041, "R/C/L",
     "N=1024、ENOB10.5；|V_o|=0.05 Vrms 最差；多次平均可降低"),
    ("激励频率准确度（时钟）", 0.0, 0.005, 0.0, 0.005, "C/L",
     "用实际时钟频率计算（晶振 50 ppm）；若按标称值算则直接引入 0.1%"),
    ("夹具/输入杂散 C_p（100 kHz，100 pF 档）", 0.002, 1.0, 0.0, 0.2, "C/L（高阻小电容）",
     "1 pF / 100 pF = 1%；残余由 C_p 漂移（0.2 pF）决定；10 pF 时更差（见 F 节）"),
    ("R_f 并联寄生 C_f（100 kΩ、1 pF、100 kHz）", 0.002, 0.197, 0.0, 0.02, "C/L（高阻档）",
     "|Z_f| 偏 −0.197%、相位 +3.60°、表观 D=0.063；开+短路校准消不掉，需负载校准"),
    ("四线 Kelvin 引线/接触（0.1 Ω 残余）", 0.02, 0.02, 0.02, 0.02, "R（10 Ω 档）",
     "两线时 0.1 Ω/10 Ω = 1%，必须四线；四线后残余按 0.02%"),
    ("电源/温度漂移（激励幅度、R_f 温漂）", 0.05, 0.05, 0.05, 0.05, "R/C/L",
     "比值式抵消激励漂移；R_f 温漂取 0.05% 保守值"),
    ("DFT 泄漏（非相干/频率漂移）", 0.01, 0.01, 0.01, 0.01, "R/C/L",
     "相干采样、整周期；残余按 0.01%"),
    ("ADC 非线性 INL（12 bit）", 0.02, 0.02, 0.01, 0.01, "R/C/L",
     "±1 LSB 量级；两通道比值部分抵消"),
    ("随机噪声（热噪声/运放噪声）", 0.005, 0.005, 0.005, 0.005, "R/C/L",
     "10 次平均后更小"),
]
ADC_ROW = "ADC 量化 + DFT + 前端噪声"
RSS_R_UNCAL = rss([r[1] for r in BUDGET])
RSS_CL_UNCAL = rss([r[2] for r in BUDGET])
RSS_R_CAL = rss([r[3] for r in BUDGET])
RSS_CL_CAL = rss([r[4] for r in BUDGET])
RSS_R_CAL_EX_ADC = rss([r[3] for r in BUDGET if r[0] != ADC_ROW])
RSS_CL_CAL_EX_ADC = rss([r[4] for r in BUDGET if r[0] != ADC_ROW])
WC_R_UNCAL = sum(r[1] for r in BUDGET)
WC_CL_UNCAL = sum(r[2] for r in BUDGET)
WC_R_CAL = sum(r[3] for r in BUDGET)
WC_CL_CAL = sum(r[4] for r in BUDGET)
ADC_CL_WORST = math.sqrt((SIGMA_A_EFF / (math.sqrt(2.0) * 0.05)) ** 2 +
                         (SIGMA_A_EFF / (math.sqrt(2.0) * V_IN)) ** 2) * 100.0
ADC_CL_TYP = math.sqrt((SIGMA_A_EFF / (math.sqrt(2.0) * 0.1)) ** 2 +
                       (SIGMA_A_EFF / (math.sqrt(2.0) * V_IN)) ** 2) * 100.0

w("### C.6 2% 电容 / 1% 电阻 / 0.5% 电阻指标中，幅值误差预算还剩多少？")
w()
w("“已占”取 G 节校准后 RSS（除 ADC 量化/DFT 行）；ADC 占用取 |V_o|=0.05 Vrms 最差与 0.1 Vrms 典型。")
rows = []
for name, target, occ, adc_w, adc_t in [
    ("C 2%", 0.02, RSS_CL_CAL_EX_ADC / 100.0, ADC_CL_WORST / 100.0, ADC_CL_TYP / 100.0),
    ("R 1%（1 kHz）", 0.01, RSS_R_CAL_EX_ADC / 100.0, ADC_CL_WORST / 100.0, ADC_CL_TYP / 100.0),
    ("R 0.5%（1 kHz）", 0.005, RSS_R_CAL_EX_ADC / 100.0, ADC_CL_WORST / 100.0, ADC_CL_TYP / 100.0),
]:
    rem_w = math.sqrt(max(target ** 2 - occ ** 2 - adc_w ** 2, 0.0))
    rem_t = math.sqrt(max(target ** 2 - occ ** 2 - adc_t ** 2, 0.0))
    rows.append([name, pctf(target), pctf(occ), pctf(adc_w) + " / " + pctf(adc_t),
                 pctf(rem_w) + " / " + pctf(rem_t)])
table(["指标", "目标", "其它项已占（校准后 RSS）", "ADC 量化/DFT 占用",
       "剩余 RSS 幅值预算"], rows)
w(f"即：ADC 量化/DFT（最差 |V_o|=0.05 Vrms、贡献 {pctv(ADC_CL_WORST)}）只占 2% 电容目标的约 "
  f"{sig(ADC_CL_WORST / 2.0 * 100.0, 2)}%、1% 电阻目标的约 {sig(ADC_CL_WORST * 100.0, 2)}%；"
  "剩余预算几乎全部留给杂散、R_f 并联寄生、通道增益失配等（G 节），它们才是必须靠校准解决的项。")
w()

# ----------------------------------------------------------------------------
# D. 时序预算
# ----------------------------------------------------------------------------
w("## D. 时序预算（单次测量 ≤ 1 s，100 Hz 档也要满足）")
w()
w("公式：")
w("```")
w("每档采集 10 个周期：T_acq = 10/f")
w("N=1024 点相干采样所需采样率：f_s = N/T_acq = N·f/10 = 102.4·f")
w("  （记录长度 10/f，正好 10 个整周期，落在 DFT 第 10 个 bin，无泄漏）")
w("单遍 T1 = T_acq + 5 ms（量程切换/建立） + 10 ms（DFT） + 5 ms（显示）")
w("量程搜索最坏重复 2 遍：T2 = 2·(T_acq + 5 + 10) + 5 ms")
w("```")
w()
w("### D.1 各频率档的采集时间、所需采样率与总时间")
rows = []
for f in FREQS:
    t_acq = 10.0 / f
    fs = N_DFT * f / 10.0
    t1 = t_acq + 5e-3 + 10e-3 + 5e-3
    t2 = 2.0 * (t_acq + 5e-3 + 10e-3) + 5e-3
    rows.append([hz(f), sig(t_acq * 1e3, 3) + " ms", eng(fs, "Sps"),
                 sig(t1 * 1e3, 3) + " ms", sig(t2 * 1e3, 3) + " ms",
                 sig((1.0 - t2) * 1e3, 3) + " ms", "满足" if t2 < 1.0 else "不满足"])
table(["频率档", "10 个周期 T_acq", "相干采样率 f_s（N=1024）", "单遍 T1",
       "两遍 T2（最坏）", "距 1 s 余量", "判定"], rows)
w()
w("### D.2 100 kHz 档：1024 点 × 10 周期的采样率过高，改用多周期记录")
w("100 kHz、10 个周期只有 100 µs，N=1024 要求 f_s = 10.24 MSPS；"
  "常见 MCU 内置 12 bit SAR ADC 只有约 2.4 MSPS，做不到。改用“记录 M 个周期、N 点”可降低 f_s：")
w()
w("```")
w("f_s = N·f/M，记录长度 = M/f；幅度分辨率 ∝ sqrt(2/N)（N 越小越差）")
w("```")
w()
rows = []
for n, m in [(1024, 10), (1024, 64), (256, 12), (128, 10)]:
    fs = n * 100e3 / m
    t_rec = m / 100e3
    sig_a = SIGMA_A_EFF * math.sqrt(1024.0 / n)
    eps_a = sig_a / (math.sqrt(2.0) * 0.1)
    rows.append([str(n), str(m), eng(fs, "Sps"), sig(t_rec * 1e6, 3) + " µs",
                 pctf(eps_a),
                 "可用" if fs <= 3e6 else "需外置高速 ADC 或降低 N"])
table(["N", "记录周期数 M", "f_s = N·f/M", "记录长度",
       "单通道幅值分辨率（ENOB10.5，|V_o|=0.1 Vrms）", "可行性（内置 SAR ≈2.4 MSPS）"], rows)
w()
w(f"取 N=256、M=12（f_s = {eng(256*100e3/12, 'Sps')}）时，100 kHz 档单遍 T1 = "
  f"{sig((12/100e3 + 5e-3 + 10e-3 + 5e-3)*1e3, 3)} ms，两遍 "
  f"{sig((2*(12/100e3 + 5e-3 + 10e-3) + 5e-3)*1e3, 3)} ms，仍远小于 1 s；"
  f"代价是幅值分辨率从 {pctf(SIGMA_A_EFF/(math.sqrt(2.0)*0.1))} 降到 "
  f"{pctf(SIGMA_A_EFF*2.0/(math.sqrt(2.0)*0.1))}，仍远好于 2% 指标。")
w()
w("### D.3 100 Hz 档：多块平均")
w(f"100 Hz 档 10 个周期 = {sig(10/100.0*1e3, 3)} ms，单遍 {sig((10/100.0+0.02)*1e3, 3)} ms、"
  f"两遍 {sig((2*(10/100.0+0.015)+0.005)*1e3, 3)} ms。若取 4 块平均提高信噪比 √4 = 2 倍：")
w(f"`T = 4×(100+5+10) + 5 = {sig((4*(0.1+0.005+0.01)+0.005)*1e3, 3)} ms`（单量程）；"
  f"两遍量程搜索最坏 `2×4×115 + 5 = {sig((2*4*0.115+0.005)*1e3, 3)} ms`，仍 < 1 s。")
w()

# ----------------------------------------------------------------------------
# E. 量程 R_f 设计
# ----------------------------------------------------------------------------
w("## E. 量程 R_f 设计")
w()
w("公式（理想电桥、Z_f=R_f）：`|V_o|/|V_in| = R_f/|Z_x|`。")
w("要求比值 ∈[0.1, 1]（并留 ADC 上限余量），则一档覆盖 `|Z_x| ∈ [R_f, 10·R_f]`，")
w(f"输出 `|V_o| = V_IN·R_f/|Z_x| ∈ [0.05, 0.5] Vrms`。ADC 直接耦合、偏置 {sig(V_REF)} V 时允许 "
  f"|V_o| ≤ {volt(VO_MAX_DIRECT)}（比值 ≤ {sig(RATIO_MAX_ADC)}）。")
w()
w("### E.1 档位表")
rows = []
for rf in RF_LIST:
    zmin, zmax = rf, 10.0 * rf
    note = ""
    if abs(rf - 10.0) < 1e-9:
        note = "10 Ω–100 Ω DUT（R=10 Ω、L=10 µH 见 E.2/E.3）"
    elif abs(rf - 1e6) < 1e-9:
        note = "1 MΩ DUT（限 1 kHz）"
    rows.append([ohm(rf), ohm(zmin) + " – " + ohm(zmax), "0.1 – 1",
                 "0.05 – 0.5", amp(V_IN / zmax) + " – " + amp(V_IN / rf), note])
table(["R_f 档位", "覆盖 |Z_x| 区间", "|V_o|/|V_in| 理论范围", "|V_o| 范围（Vrms）",
       "该档 DUT 电流（最小 – 最大）", "备注"], rows)
w(f"6 档合并覆盖 |Z_x| = 10 Ω – 10 MΩ，覆盖 R 10 Ω–1 MΩ 与 C/L 的常用范围（除 L 10 µH 的 6.28 Ω，见 E.3）。")
w(f"各档在 |Z_x|=R_f（ratio=1）时电流：10 Ω→{amp(V_IN/10)}、100 Ω→{amp(V_IN/100)}、"
  f"1 kΩ→{amp(V_IN/1e3)}、10 kΩ→{amp(V_IN/1e4)}、100 kΩ→{amp(V_IN/1e5)}、1 MΩ→{amp(V_IN/1e6)}；"
  "低阻档要 50 mA 输出缓冲，高阻档 0.5 µA 电流对偏置/漏电敏感。")
w()
w("### E.2 端点验证（R = 10 Ω 与 R = 1 MΩ）")
rows = [
    ["R = 10 Ω（R_f=10 Ω）", ohm(10.0), "1.00", volt(V_IN), sig(V_REF + math.sqrt(2.0)*V_IN, 3) + " V",
     amp(V_IN/10.0), "ratio=1；峰值 3.21 V，余量 92.9 mV；I=50 mA，需 100 mA 缓冲"],
    ["R = 10 Ω（错误：R_f=100 Ω）", ohm(100.0), "10.0", volt(V_IN*10.0),
     sig(V_REF + math.sqrt(2.0)*V_IN*10.0, 3) + " V", amp(V_IN/10.0),
     "ratio=10 ⇒ 需 5 Vrms 输出（峰 9.57 V），物理上做不到"],
    ["R = 10 Ω（R_f=100 Ω + 激励 0.05 Vrms）", ohm(100.0), "10.0（对 0.05 Vrms）",
     volt(0.05*100.0/10.0), sig(V_REF + math.sqrt(2.0)*0.05*10.0, 3) + " V", amp(0.05/10.0),
     "|V_o|=0.5 Vrms、I=5 mA；但 DUT 电平 0.05 Vrms，不满足 0.5 Vrms±10%"],
    ["R = 1 MΩ（R_f=1 MΩ，1 kHz）", ohm(1e6), "1.00", volt(V_IN),
     sig(V_REF + math.sqrt(2.0)*V_IN, 3) + " V", amp(V_IN/1e6),
     "I=0.5 µA；必须 1 kHz 测（100 kHz 时 R_f∥1 pF 极点 159 kHz）"],
]
table(["情形", "R_f", "|V_o|/|V_in|", "|V_o|", "输出峰值", "DUT 电流", "结论"], rows)
w()
w("### E.3 低阻/高阻端点专项验证")
rows = []
for name, z, rf, note in [
    ("L = 10 µH @100 kHz", zl(10e-6, 100e3), 10.0,
     "ratio=1.59 > 1.131，|V_o|=0.796 Vrms、峰 3.63 V 削顶；需 4.7 Ω 档（ratio 0.748）或降低激励；I=79.6 mA"),
    ("L = 100 µH @100 kHz", zl(100e-6, 100e3), 10.0,
     "ratio=0.159，|V_o|=0.079 Vrms，用 R_f=10 档即可"),
    ("C = 100 nF @100 kHz", zc(100e-9, 100e3), 10.0,
     "ratio=0.628，|V_o|=0.314 Vrms，I=31.4 mA"),
    ("C = 10 µF @100 Hz", zc(10e-6, 100.0), 100.0,
     "ratio=0.628，|V_o|=0.314 Vrms，I=3.14 mA"),
    ("R = 10 Ω @1 kHz", 10.0, 10.0, "ratio=1，I=50 mA；四线必需"),
    ("R = 1 MΩ @1 kHz", 1e6, 1e6, "ratio=1，I=0.5 µA；注意 1 pF 并联极点 159 kHz 与偏置电流"),
]:
    ratio = rf / z
    vo = V_IN * ratio
    peak = V_REF + math.sqrt(2.0) * vo
    rows.append([name, ohm(z), ohm(rf), sig(ratio, 3), volt(vo),
                 sig(peak, 3) + " V", amp(V_IN / z), note])
table(["端点", "|Z_x|", "建议 R_f", "|V_o|/|V_in|", "|V_o|", "输出峰值", "DUT 电流", "结论"], rows)
w()
w("**E 节结论：**")
w("1. 6 档 R_f（10 Ω/100 Ω/1 kΩ/10 kΩ/100 kΩ/1 MΩ）覆盖 |Z_x| = 10 Ω–10 MΩ，"
  "R 10 Ω–1 MΩ 全部落在 ratio 0.1–1 内（1 MΩ 端点 ratio=1）。")
w("2. R=10 Ω 不能配 R_f=100 Ω（ratio=10、|V_o|=5 Vrms 饱和）：正确做法是 "
  "(a) 增加 R_f=10 Ω 档（I=50 mA，需 100 mA 缓冲），或 (b) 把激励降到 0.05 Vrms"
  "（但违反 0.5 Vrms 测试电平指标）。")
w(f"3. 10 Ω 档 ratio=1 时输出峰值 {sig(V_REF + math.sqrt(2.0)*V_IN, 3)} V，距 {sig(ADC_FS)} V 只有 "
  f"{sig(MARGIN_RATIO1*1e3, 3)} mV 余量；若 ADC 前端有 1% 增益误差就可能削顶，建议留 ratio ≤0.95 或 AC 耦合。")
w(f"4. L=10 µH@100 kHz（{ohm(zl(10e-6, 100e3))}）超出 10 Ω 最小档能力（ratio=1.59），"
  "要么加 4.7 Ω 档、要么降低该档激励；这是 100 kHz 电感扩展指标最难的端点。")
w("5. R=1 MΩ 的 0.5% 自动量程必须在 1 kHz 实现；100 kHz 下 R_f=1 MΩ 并联 1 pF 使 |Z_f| 偏 18%、"
  "相位 32°，无法靠开路/短路校准消除（F 节）。")
w()

# ----------------------------------------------------------------------------
# F. 100 kHz 高频误差：夹具杂散与开路/短路校准
# ----------------------------------------------------------------------------
w("## F. 100 kHz 高频误差：夹具杂散与开路/短路校准")
w()
w("公式：")
w("```")
w("夹具并联杂散 C_p 并在 Z_x 上：C_meas ≈ C + C_p（两电容并联直接相加）")
w("  未校准 C 相对误差 ≈ C_p/C；|Z| 相对误差 ≈ -C_p/C（一阶）")
w("开路（只剩 C_p）：Z_open = Z_short + 1/(jωC_p)")
w("短路（接触电阻 + 残余电感）：Z_short = R_s + jωL_s")
w("两点校准：Z_x = (Z_m - Z_short)·Z_open / (Z_open - (Z_m - Z_short))")
w("  理想情况下完全消掉 C_p；残余只来自 C_p 在“校准后/测量时”的变化 ΔC_p：")
w("  ΔC/C ≈ -ΔC_p/C")
w("```")
w()
fF = 100e3
wF = 2.0 * PI * fF
C10 = 10e-12
Zx10 = -1j / (wF * C10)
Zshort10 = ZS_RS + 1j * wF * ZS_LS


def zp_with_cp(zx, cp):
    return 1.0 / (1.0 / zx + 1j * wF * cp)


def cap_from_z(z):
    return -1.0 / (wF * z.imag)


w("### F.1 未校准：C = 10 pF，夹具并联杂散 C_p = 1 pF / 2 pF（100 kHz）")
rows = []
for cp in [1e-12, 2e-12]:
    zm = Zshort10 + zp_with_cp(Zx10, cp)
    c_meas = cap_from_z(zm)
    zerr = (abs(zm) - abs(Zx10)) / abs(Zx10)
    rows.append([farad(cp), farad(c_meas), pctf((c_meas - C10) / C10),
                 pctf(zerr), pctf(cp / C10)])
table(["C_p", "测得 C", "C 相对误差", "|Z| 相对误差", "≈C_p/C"], rows)
w("C=10 pF、C_p=1 pF 时未校准误差 +10%，2 pF 时 +20%，都远超 5% 指标——10 pF 档必须先做开路校准。")
w()
w("### F.2 开路 + 短路校准后的残余误差（C_p 漂移 ΔC_p）")
rows = []
cp_nom = 1e-12
zopen_nom = Zshort10 + 1.0 / (1j * wF * cp_nom)
for dcp in [0.0, 0.05e-12, 0.1e-12, 0.2e-12, 0.5e-12]:
    zm = Zshort10 + zp_with_cp(Zx10, cp_nom + dcp)
    zc = (zm - Zshort10) * zopen_nom / (zopen_nom - (zm - Zshort10))
    c_corr = cap_from_z(zc)
    zerr = (abs(zc) - abs(Zx10)) / abs(Zx10)
    rows.append([farad(dcp) if dcp else "0（C_p 完全稳定）", farad(c_corr),
                 pctf((c_corr - C10) / C10), pctf(zerr)])
table(["C_p 漂移 ΔC_p", "校准后 C", "C 相对误差", "|Z| 相对误差"], rows)
w("说明：开+短路校准把 C_p 的一阶影响完全消掉；残余只取决于 C_p 的稳定性。"
  "要把 10 pF 的误差压在 5% 内，C_p 在校准与测量之间的漂移需 ≲ 0.5 pF；要 ≤2% 则需 ≲ 0.2 pF。")
w()
w("### F.3 与 Z_f 并联的寄生 C_f（量程电阻自身寄生）")
w()
w("R_f 的并联寄生 C_f 使有效反馈阻抗 Z_f,eff = R_f/(1+jωR_fC_f)，等效为乘性因子 A = 1+jωR_fC_f：")
w("`Z_meas = A·Z_x`。于是 |Z| 偏 |A| 倍、相位移 arg(A)，且开+短路校准消不掉 A，"
  "必须做负载校准（第三标准）或在软件里按已知 C_f 修正。")
rows = []
for rf, cf, ff in [(1e4, 1e-12, 100e3), (1e5, 1e-12, 100e3), (1e5, 2e-12, 100e3),
                   (1e6, 1e-12, 100e3), (1e6, 1e-12, 1e3)]:
    wloc = 2.0 * PI * ff
    a = 1.0 + 1j * wloc * rf * cf
    amp_err = (1.0 / abs(a) - 1.0)
    phase = math.atan2(a.imag, a.real)
    d_app = math.tan(phase)
    rows.append([ohm(rf), farad(cf), hz(ff), pctf(abs(a) - 1.0), deg(phase),
                 pctf(amp_err), sig(d_app, 3)])
table(["R_f", "C_f", "频率", "|A|-1", "arg(A)", "C（按 Z 模值算）误差", "表观 D"], rows)
a100k = 1.0 + 1j * wF * 1e5 * 1e-12
a1m = 1.0 + 1j * wF * 1e6 * 1e-12
w(f"* R_f=100 kΩ、C_f=1 pF、100 kHz 时 |A|-1 = {pctf(abs(a100k)-1.0)}、相位 {deg(math.atan(a100k.imag))}，"
  f"按 |Z| 换算的 C 误差 {pctf(1.0/abs(a100k)-1.0)}，表观 D≈{sig(math.tan(math.atan(a100k.imag)), 3)}——"
  "损耗角/ESR 类显示会被它污染。")
w(f"* R_f=1 MΩ、C_f=1 pF 的极点只有 {hz(1.0/(2*PI*1e6*1e-12))}；100 kHz 时 |A|-1 达 "
  f"{pctf(abs(a1m)-1.0)}、相位 {deg(math.atan(a1m.imag))}——1 MΩ 档在 100 kHz 不可用，"
  "这也是 E 节要求 1 MΩ 自动量程在 1 kHz 实现的原因。")
w()
w("### F.4 校准信号电平与短路校准风险")
w()
w("```")
w("开路校准：Z_open ≈ 1/(jωC_p)（1 pF @100 kHz ≈ 1.59 MΩ）")
w("  |V_o| = V_IN·R_f/|Z_open|；R_f=100 kΩ、1 pF 时 ≈ 31.4 mVrms（2 pF 时 62.8 mVrms）")
w("短路校准：Z_short ≈ 0.1 Ω（+jω·20 nH = j0.0126 Ω @100 kHz）")
w("  R_f=10 Ω 时若仍用 0.5 Vrms 激励：I = 5 A、|V_o| = 50 V —— 必须把激励降到 mV 级")
w("```")
w()
zopen1 = 1.0 / (wF * 1e-12)
zopen2 = 1.0 / (wF * 2e-12)
vin_need = 0.5 * ZS_RS / 10.0
i_need = vin_need / ZS_RS
p_need = i_need * i_need * ZS_RS
rows = [
    ["开路校准（1 pF）", ohm(zopen1), volt(V_IN), volt(V_IN*1e5/zopen1),
     amp(V_IN/zopen1), "正常"],
    ["开路校准（2 pF）", ohm(zopen2), volt(V_IN), volt(V_IN*1e5/zopen2),
     amp(V_IN/zopen2), "正常"],
    ["短路校准，R_f=10 Ω，0.5 Vrms", ohm(ZS_RS), volt(V_IN), volt(V_IN*10.0/ZS_RS),
     amp(V_IN/ZS_RS), "5 A / 50 V，不可行"],
    ["短路校准，R_f=10 Ω，5 mV", ohm(ZS_RS), volt(vin_need), volt(0.5),
     amp(i_need), f"可行：I={amp(i_need)}、P={sig(p_need*1e3, 3)} mW"],
]
table(["校准项", "被测阻抗", "激励", "|V_o|", "电流", "结论"], rows)
w()
w("短路校准为何必要（不校时 0.1 Ω 接触电阻的影响）：")
rows = []
for name, z in [("L = 10 µH @100 kHz", 2 * PI * 100e3 * 10e-6),
                ("L = 100 µH @100 kHz", 2 * PI * 100e3 * 100e-6),
                ("C = 10 pF @100 kHz", abs(Zx10))]:
    rows.append([name, ohm(z), pctf(0.1 / z), pctf(0.02 / z)])
table(["低阻端点", "|Z_x|", "0.1 Ω 未校准占比", "短路校准后残余 0.02 Ω 占比"], rows)
w(f"另外 20 nH 的短路残余电感在 100 kHz 只有 {ohm(2*PI*100e3*20e-9)}，对 6.28 Ω 的 L=10 µH 是 "
  f"{pctf(2*PI*100e3*20e-9/6.283)}；低频/低阻档务必先做短路校准，否则 10 Ω 档会有 1% 量级固定偏差。")
w()

# ----------------------------------------------------------------------------
# G. 总误差预算
# ----------------------------------------------------------------------------
w("## G. 总误差预算")
w()
w("合成方式：`RSS = sqrt(Σ e_i^2)`（独立随机误差）；`最坏和 = Σ|e_i|`（系统性、同号最坏）。")
w()
w("### G.1 误差源 → 相对误差贡献")
BTAB = {r[0]: r for r in BUDGET}


def verdict(ur, ucl, cr, ccl):
    worst_un = max(ur, ucl)
    worst_cal = max(cr, ccl)
    if worst_un > 0.5 and worst_cal <= 0.05:
        return "未校准不可接受 → 必须校准"
    if worst_un > 0.5:
        return "未校准不可接受"
    return "可接受"


rows = []
for name, ur, ucl, cr, ccl, obj, note in BUDGET:
    rows.append([name, pctv(ur), pctv(ucl), pctv(cr), pctv(ccl), obj,
                 verdict(ur, ucl, cr, ccl), note])
table(["误差源", "未校准 R", "未校准 C/L", "校准后 R", "校准后 C/L",
       "主要影响", "是否可接受/措施", "备注"], rows)
w()
w("### G.2 三个指标的合计（RSS 与最坏情况）")
rows = []
for name, target, rss_u, rss_c, wc_u, wc_c in [
    ("R 1%（基本档）", 0.01, RSS_R_UNCAL, RSS_R_CAL, WC_R_UNCAL, WC_R_CAL),
    ("C/L 2%（基本 + 发挥）", 0.02, RSS_CL_UNCAL, RSS_CL_CAL, WC_CL_UNCAL, WC_CL_CAL),
    ("R 0.5%（自动量程）", 0.005, RSS_R_UNCAL, RSS_R_CAL, WC_R_UNCAL, WC_R_CAL),
]:
    ok_u = "通过" if (rss_u < target * 100 and wc_u < target * 100) else "不通过"
    ok_c = "通过" if (rss_c < target * 100 and wc_c < target * 100) else "仍不通过"
    rows.append([name, pctv(target * 100), pctv(rss_u), pctv(rss_c), pctv(wc_u),
                 pctv(wc_c), "未校准：" + ok_u + "；校准后：" + ok_c])
table(["指标", "目标", "RSS 未校准", "RSS 校准后", "最坏和 未校准", "最坏和 校准后", "判定"], rows)
w("（数值单位均为 %；R 0.5% 与 R 1% 用同一组 R 列误差，因为误差机制相同。）")
w()
w("### G.3 主导项结论")
gain_row = BTAB["ADC 两通道增益失配 0.05 dB"]
w(f"1. **R（1% 与 0.5%）**：未校准 RSS = {pctv(RSS_R_UNCAL)}，主导项是 ADC 两通道增益失配 "
  f"{pctv(gain_row[1], 2)}（占 RSS 平方和约 {sig(gain_row[1]**2/RSS_R_UNCAL**2*100, 3)}%），"
  f"其次是 R_f 公差 0.1%。校准后 RSS = {pctv(RSS_R_CAL)}，主导项变为温漂/电源 0.05% 与引线 0.02%。")
w(f"2. **C/L（2%）**：未校准 RSS = {pctv(RSS_CL_UNCAL)}，主导项是夹具杂散 C_p 1%（100 pF 档；10 pF 档为 10%）"
  f"与通道增益失配 {pctv(gain_row[2], 2)}；校准后 RSS = {pctv(RSS_CL_CAL)}，"
  "由杂散残余 0.2% 与 R_f 并联 C_f 0.197% 主导。")
w("3. **必做校准项**：R_f 标定（0.1%→0.02%）、ADC 两通道增益校准（0.577%→0.02%）、开路校准（C_p）、"
  "短路校准（0.1 Ω）；要消掉 R_f 并联 C_f 的乘性误差还需负载校准（第三标准）或已知 C_f 的软件修正。")
w()
w("### G.4 100 kHz 幅值误差主导项排序（按 Lead 指定顺序）")
stray = BTAB["夹具/输入杂散 C_p（100 kHz，100 pF 档）"]
cf = BTAB["R_f 并联寄生 C_f（100 kΩ、1 pF、100 kHz）"]
noise = BTAB["ADC 量化 + DFT + 前端噪声"]
rows = [
    ["1", "夹具/输入杂散 C_p", pctv(stray[2]), pctv(stray[4]),
     "开路校准 + 每次测量前重测 Z_open；10 pF 档未校准达 10%"],
    ["2", "R_f 并联寄生 C_f", pctv(cf[2]), pctv(cf[4]),
     "负载校准或按已知 C_f 软件修正；1 MΩ 档 100 kHz 不可用"],
    ["3", "ADC 两通道增益失配", pctv(gain_row[2], 2), pctv(gain_row[4], 2),
     "两通道增益校准；注意未校准时它比 C_f 项更大"],
    ["4", "噪声（量化/DFT/前端）", pctv(noise[2]), pctv(noise[4]),
     "|V_o|=0.05 Vrms 最差；多次平均可降"],
]
table(["排序", "误差源", "未校准贡献", "校准后贡献", "措施"], rows)
w("注意：**未校准**时 ADC 增益失配 0.577% 在数值上大于 R_f 并联寄生的 0.197%，只有在校准后（0.02%）"
  "才落到它之后；此排序对应“已做增益校准”的 100 kHz 幅值误差。")
w()

# ----------------------------------------------------------------------------
# H. 需要 Lead 注意的矛盾点 / 前提变化
# ----------------------------------------------------------------------------
w("## H. 需要 Lead 注意的矛盾点 / 前提变化")
w()
w(f"1. **0.5 Vrms 测试电平与 +5 V 单电源/3.3 V ADC 满量程冲突。** ADC 直接耦合、偏置 {sig(V_REF)} V 时 "
  f"|V_o| 只能到 {volt(VO_MAX_DIRECT)}，比值上限 {sig(RATIO_MAX_ADC)}；ratio=1 时输出峰值 "
  f"{sig(V_REF + math.sqrt(2.0)*V_IN, 3)} V，距 {sig(ADC_FS)} V 仅 {sig(MARGIN_RATIO1*1e3, 3)} mV，"
  f"ADC 只用了 {pctv(FS_UTIL_RATIO1*100)} 满量程（等效损失约 {sig(math.log2(1.0/FS_UTIL_RATIO1), 2)} bit）。"
  f"建议交流耦合重新偏置到 {sig(ADC_PEAK)} V（|V_o| 上限 {volt(VO_MAX_AC)}），或差分输出/前端衰减。")
w("2. **“按 |Z| 窗口选频率”只适用于 C/L，对 R 无效。** R 的 |Z| 与频率无关，"
  "R 10 Ω–1 kΩ 与 100 kΩ–1 MΩ 段只能靠 R_f 量程和四线夹具；"
  "文档不要写成“R 也按窗口自动选频”。")
w("3. **10 Ω 端点与 0.5 Vrms 电平矛盾。** R=10 Ω 若配 R_f=100 Ω，ratio=10、|V_o|=5 Vrms，物理上做不到；"
  "必须增加 R_f=10 Ω 档（I=50 mA，需 100 mA 缓冲）或把激励降到 0.05 Vrms——"
  "后者违反“测试电平 0.5 Vrms±10%”的指标。")
w(f"4. **L=10 µH@100 kHz（{ohm(zl(10e-6, 100e3))}）超出 6 档 R_f 最小档能力。** R_f=10 Ω 时 ratio=1.59 > "
  f"ADC 上限 {sig(RATIO_MAX_ADC)}，|V_o|=0.796 Vrms、峰值 3.63 V（削顶）；需 4.7 Ω 档（ratio 0.748）"
  "或降低该档激励；此时电流 79.6 mA。这是 100 kHz 电感扩展指标最硬的端点。")
w(f"5. **R=1 MΩ 的 0.5% 自动量程与 100 kHz 不能同时满足。** R_f=1 MΩ 并联 1 pF 的极点只有 "
  f"{hz(1.0/(2*PI*1e6*1e-12))}，100 kHz 下 |Z_f| 偏 {pctf(abs(a1m)-1.0)}、相位 {deg(math.atan(a1m.imag))}；"
  "必须限定 1 MΩ 档只在 1 kHz 测量，文档中要写清这个前提。")
w(f"6. **GBW 对幅值精度是二阶，不是主要矛盾。** 20 MHz GBW 在 100 kHz 下相位 {deg(phase20)}、"
  f"幅值误差仅 {pctf(amp20)}；2% C/L 与 1% R 的幅值指标对 GBW 的要求只有约 2 MHz（单极点）/40 MHz（A_ol 有实部）。"
  f"真正被 GBW 限制的是相位/D：{deg(phase20)} 对应 D 误差 {sig(phase20, 3)}，"
  "测 D=0.01 到 10% 需 GBW≥200 MHz 或软件相位补偿；100 kHz 下测 D=0.001（需 2 GHz）不现实，"
  "只能靠相位标定或改在低频测。")
w(f"7. **兼容“A_ol 有实部”的运放会让幅值误差退化为一阶。** 复合/多级运放在 100 kHz 处若 A_ol 带实部，"
  f"20 MHz GBW 的幅值误差从 {pctf(amp20)} 退化为 {pctf(worst20)}，直接吃掉 0.5% 自动量程指标——"
  "选型时要确认 100 kHz 处仍以单极点虚部为主，或做开环增益的相位/幅值标定。")
w(f"8. **12 bit ADC + DFT 不是瓶颈。** |V_o|≥0.1 Vrms 时 |Z| 幅值分辨率优于 {pctf(SIGMA_A_EFF/(math.sqrt(2.0)*0.1))}，"
  f"最差（0.05 Vrms）{pctv(ADC_CL_WORST)}；相位分辨率（|V_o|=0.1 Vrms）约 "
  f"{deg(SIGMA_A_EFF/(math.sqrt(2.0)*0.1))}，D 分辨率优于 1e-3；ADC 最差 {pctv(ADC_CL_WORST)} 只占 "
  f"2% 电容目标的约 {sig(ADC_CL_WORST/2.0*100.0, 2)}%、1% 电阻目标的约 {sig(ADC_CL_WORST*100.0, 2)}%。"
  "剩余预算几乎全给校准/杂散/失配。")
w("9. **开路+短路两点校准消不掉 R_f 并联寄生 C_f。** C_f 等效为乘性因子 A=1+jωR_fC_f："
  f"100 kΩ、1 pF、100 kHz 时 |Z| 误差 {pctf(abs(a100k)-1.0)}、相位 {deg(math.atan(a100k.imag))}、"
  f"表观 D={sig(math.tan(math.atan(a100k.imag)), 3)}；开+短路校准后仍留 A。"
  "要彻底消除需加负载校准（第三标准）或在软件里按已知 C_f 修正。")
w(f"10. **短路校准在 0.5 Vrms 下不可直接做。** R_f=10 Ω、Z_short=0.1 Ω 时 I={amp(V_IN/ZS_RS)}、"
  f"|V_o|={volt(V_IN*10.0/ZS_RS)}；必须把激励降到约 {volt(0.5*ZS_RS/10.0)}"
  f"（I={amp(0.5/10.0)}）再校短路，并保证放大器/夹具能承受。")
w(f"11. **两线夹具不满足 10 Ω 档 1% 指标。** 0.1 Ω 引线/接触在 10 Ω 上就是 {pctf(0.1/10.0)}，"
  "必须四线 Kelvin；文档要明确四线接法与开尔文夹。")
w(f"12. **100 kHz 10 pF 指标（5%）完全取决于 C_p 稳定性。** C_p=1 pF 时未校准误差 +10%；"
  f"开路校准后残余由 C_p 漂移决定：0.2 pF→{pctf(0.2/10.0)}，0.5 pF→{pctf(0.5/10.0)}。"
  "要满足 5% 需 C_p 在校准与测量间稳定在 ±0.25 pF 内，并每次测量前重做开路校准。")
w("13. **12 bit SAR 采样率不够 100 kHz×1024 点×10 周期。** 需 10.24 MSPS；"
  "内置 2.4 MSPS 级 SAR 应改为 N=256/M=12（2.13 MSPS）或 N=1024/M=64（1.6 MSPS），"
  "代价是分辨率降到 0.04% 量级，仍远好于 2%。时序余量充足：最坏两遍 100 Hz 档 235 ms，"
  "4 块平均两遍 925 ms，均 < 1 s。")
w("14. **频率误差 0.1% 与 C/L 值精度。** 若软件用标称频率换算 C/L，0.1% 频率误差直接进入结果；"
  "应当用实际采样时钟频率（晶振 50 ppm → 0.005%）计算，0.1% 只是显示频率与标称的偏差要求。")
w(f"15. **误差预算总结。** R 1%：未校准 RSS {pctv(RSS_R_UNCAL)}/最坏 {pctv(WC_R_UNCAL)}"
  f"（靠增益失配 {pctv(gain_row[1], 2)} 主导），校准后 {pctv(RSS_R_CAL)}/{pctv(WC_R_CAL)}；"
  f"C/L 2%：未校准 RSS {pctv(RSS_CL_UNCAL)}/最坏 {pctv(WC_CL_UNCAL)}（夹具杂散 1% 主导），"
  f"校准后 {pctv(RSS_CL_CAL)}/{pctv(WC_CL_CAL)}；"
  f"R 0.5%：未校准 {pctv(RSS_R_UNCAL)} 不通过，校准后 {pctv(RSS_R_CAL)} 通过。"
  "必须靠校准：通道增益、开路、短路、R_f 标定；建议增加负载校准消除 C_f。")
w()


# ----------------------------------------------------------------------------
# 输出
# ----------------------------------------------------------------------------
def main():
    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    text = "\n".join(LINES) + "\n"
    with open(OUT_PATH, "w", encoding="utf-8") as fh:
        fh.write(text)
    print(text)
    print(f"[written] {OUT_PATH}  ({len(LINES)} lines, {len(text)} chars)")


if __name__ == "__main__":
    main()

