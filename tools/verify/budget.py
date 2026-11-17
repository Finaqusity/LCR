# -*- coding: utf-8 -*-
"""LCR 电桥固件 —— 采样时序 / DFT 成本 / 时间预算 / 校准存储 一致性核算脚本。

用途：改采样率、块长 S/N、块数 K、定时器、校准记录结构之后，跑本脚本，
      确认数值仍与 docs/软件架构.md §5（DFT 内核）、§9（性能预算）、§7.6（校准记录）一致。
      这是 handoff/交接文档要求"数值改动必须复算"的官方脚本（入库版）。

与旧版 budget_calc.py（本地草稿，TIM8/PSC=1/S=8@100k，已作废）的区别：
  * ADC 触发 = TIM2_TRGO（84 MHz，32 位），不是 TIM8；
  * 100 kHz 用 S=12 / N=1008 / fs=1.2 MSPS（不是 S=8 / 800 kSPS）；
  * 100 Hz 用 TIM2 ARR=52499（PSC=0），无需 TIM8 的 PSC=1 补丁；
  * 校准记录 v2 = 1240 B / 槽 1536 B / 双扇区 A/B；
  * 代码区 256 KB + sector6/sector7 两个 128 KB 校准扇区。

运行：python tools/verify/budget.py
退出码：0 = 全部断言通过；1 = 有断言失败（打印 FAIL 行）。
"""
import math
import sys

TIM2_HZ = 84e6          # APB1 定时器时钟（SYSCLK 168 / APB1 4 分频 42 MHz，定时器 ×2）
PCLK2 = 84e6            # ADC 预分频输入
ADC_CK = PCLK2 / 4      # 21 MHz（F407 ADC 上限 36 MHz，只能 PCLK2/2,4,6,8）
T_CONV_12 = 3 + 12      # SMP=3cyc + 12bit = 15 ADCCLK
T_CONV_10 = 3 + 10      # 13 ADCCLK（10bit 备选）
MCLK_DDS = 8e6          # PA8 MCO1 = HSE 8 MHz 直出
DDS_DEN = 2 ** 28

FREQS = [100, 1000, 10000, 100000]
S = [16, 16, 16, 12]            # fs = S * f0
N = [1024, 1024, 1024, 1008]    # 相干采样点数
K_DEFAULT = [1, 8, 16, 32]      # 每频点平均块数（架构 §9.3）
N_COARSE = 64                   # 粗测块长（架构 §5.1/§9.2）
RELAY_HOME_MS = 15.0            # 归位到 10k 默认档
GEAR_SWITCH_MS = 15.0           # 每次换档
GEAR_SWITCHES = 3.0             # 粗测 4 档 -> 精测目标档，平均换 3 次
UART_REPORT_MS = 10.42          # 一行 120 字符 @115200
OLED_REPORT_MS = 5.81           # 2 页 258 B @400 kbit/s
MISC_MS = 1.2                   # 状态机/按键/打印余量
CPU_CYCLES_PER_SAMPLE = (4, 8)  # 每样本双通道 S 相累加（4~8 周期）
SYS_HZ = 168e6

LSB12 = 3.3 / 4096
SIGMA_Q = LSB12 * 0.816         # ENOB 10.5 等效量化噪声（架构 §2.4/§9）

CAL_SLOT = 1536
CAL_SECTORS = 2
SECTOR_SIZE = 128 * 1024
CODE_FLASH = 256 * 1024

FAILS = []


def check(cond, msg):
    if not cond:
        FAILS.append(msg)
        print("  [FAIL] " + msg)


def header(t):
    print("\n" + "=" * 78)
    print(t)
    print("=" * 78)


def row(*cells):
    print("| " + " | ".join(str(c) for c in cells) + " |")


def timer_for(fs):
    """TIM2 84 MHz：q = 84e6/fs 取整，要求 |fs_actual/fs-1| < 1e-9 且 q 可用。"""
    q = TIM2_HZ / fs
    assert abs(q - round(q)) < 1e-9, "fs=%g 在 84 MHz 下不是整数分频" % fs
    q = int(round(q))
    return q, q - 1, TIM2_HZ / q


def dds_freq(f0):
    reg = int(round(f0 * DDS_DEN / MCLK_DDS))
    return reg, reg * MCLK_DDS / DDS_DEN


def main():
    # ---------------- A. 采样时序与频率精度 ----------------
    header("A. 采样时序 / 定时器 / DDS 频率（TIM2_TRGO，ADC 21 MHz）")
    row("f0", "S", "fs", "q=fs分频", "ARR", "N", "M=N/S", "块时长", "FREQREG", "f_DDS", "ppm", "DDS 误差")
    for i, f0 in enumerate(FREQS):
        fs = S[i] * f0
        q, arr, fs_act = timer_for(fs)
        m = N[i] // S[i]
        block_ms = N[i] / fs_act * 1e3
        reg, f_act = dds_freq(f0)
        ppm = (f_act / f0 - 1) * 1e6
        dds_err = f_act - f0
        row(f0, S[i], "%.6g" % fs_act, q, arr, N[i], m, "%.3f ms" % block_ms, reg,
            "%.6f Hz" % f_act, "%+.3f" % ppm, "%+.6f Hz" % dds_err)
        check(m == N[i] / S[i], "N%%S != 0 @ %g Hz" % f0)
        check(N[i] % S[i] == 0, "N 不可被 S 整除 @ %g Hz" % f0)
        check(abs(q - TIM2_HZ / fs) < 1e-9, "TIM2 分频非整数 @ %g Hz" % f0)
        check(arr <= 0xFFFFFFFF, "ARR 超出 32 位 @ %g Hz (TIM2 是 32 位)" % f0)
        check(abs(ppm) < 200, "DDS 频率误差 > 200 ppm @ %g Hz" % f0)

    t1250 = 1 / 1.2e6 * 1e9
    tconv = T_CONV_12 / ADC_CK * 1e9
    print("\n100 kHz 单样本：周期 %.1f ns，转换 %.1f ns，余量 %.1f ns（占周期 %.1f%%）"
          % (t1250, tconv, t1250 - tconv, tconv / t1250 * 100))
    check(tconv < t1250, "100 kHz 采样周期 < 转换时间")
    print("12 bit 单 ADC 上限 = %.4f MSPS；10 bit(13cyc) = %.4f MSPS"
          % (1 / (T_CONV_12 / ADC_CK) / 1e6, 1 / (T_CONV_10 / ADC_CK) / 1e6))

    # ---------------- B. DFT 成本 ----------------
    header("B. S 相折叠 DFT 成本（每样本 1 次加法 / 双通道）")
    row("N", "S", "每样本周期(4~8)", "一块耗时", "朴素 DFT(1024 点)", "加速比")
    for i, n in enumerate(N[:3] + [N[3]]):
        lo = n * CPU_CYCLES_PER_SAMPLE[0] / SYS_HZ * 1e6
        hi = n * CPU_CYCLES_PER_SAMPLE[1] / SYS_HZ * 1e6
        naive = 1024 * 32 / SYS_HZ * 1e6  # 32768 周期
        row(n, S[i], "4~8", "%.2f~%.2f µs" % (lo, hi), "%.1f µs" % naive, "%.2f~%.2f×" % (naive / hi, naive / lo))
    check(N[0] * 8 / SYS_HZ * 1e6 < 200, "单块 CPU 时间过大")

    # ---------------- C. ≤1 s 时间预算 ----------------
    header("C. ≤1 s 时间预算（100 Hz / 1 kHz / 10 kHz / 100 kHz）")
    row("f0", "粗测块(ms)", "固定开销(ms)", "K", "精测采集(ms)", "最坏总计(ms)", "≤1 s")
    totals = []
    for i, f0 in enumerate(FREQS):
        fs = S[i] * f0
        t_coarse = N_COARSE / fs * 1e3
        overhead = 4 * t_coarse + RELAY_HOME_MS + GEAR_SWITCHES * GEAR_SWITCH_MS + UART_REPORT_MS + OLED_REPORT_MS + MISC_MS
        block = N[i] / fs * 1e3
        total = overhead + K_DEFAULT[i] * block
        totals.append(total)
        row(f0, "%.3f" % t_coarse, "%.1f" % overhead, K_DEFAULT[i], "%.1f" % (K_DEFAULT[i] * block),
            "%.1f" % total, "✓" if total <= 1000 else "✗")
        check(total <= 1000, "%g Hz 最坏总时长 %.0f ms > 1 s" % (f0, total))
    print("\n架构文档 §9.2 的定稿值：876 / 604 / 180 / 105 ms（四舍五入），本脚本算得："
          + " / ".join("%.0f" % t for t in totals) + " ms")
    expect = [876, 604, 180, 105]
    for i, (got, exp) in enumerate(zip(totals, expect)):
        check(abs(got - exp) <= 2.5, "%g Hz 总时长与 §9.2 定稿差 %.1f ms" % (FREQS[i], got - exp))

    # ---------------- D. 扫频格点 ----------------
    header("D. 扫频（SCAN）频率格点：Δf = f0²·S / 84 MHz")
    row("f0", "S", "Δf", "相对 Δf/f0", "f0 允许范围(S 上限)")
    for i, f0 in enumerate(FREQS):
        df = f0 * f0 * S[i] / TIM2_HZ
        row(f0, S[i], "%.4g Hz" % df, "%.2f ppm" % (df / f0 * 1e6), "S=16: ≤87.5 kHz / S=12: ≤116.7 kHz")
    for s_, maxf in ((16, TIM2_HZ / 16 / 2), (12, TIM2_HZ / 12)):
        print("  S=%d 时 f0 上限（fs/2 奈奎斯特，N 任意）：%.1f kHz" % (s_, maxf / 1e3))

    # ---------------- E. ADC + DFT 噪声分辨率 ----------------
    header("E. 噪声与分辨率（12 bit @21 MHz，ENOB 10.5，矩形窗相干采样）")
    row("N", "σ_q(LSB)", "σ_A(µV)", "|Vo|=0.5 Vrms: |Z|1σ / φ1σ", "|Vo|=0.05 Vrms: |Z|1σ / φ1σ")
    for n in (128, 256, 1024):
        sigma_a = SIGMA_Q / math.sqrt(n / 2)
        rel = math.sqrt(2) * sigma_a / 0.5 * 100
        phi = math.sqrt(2) * sigma_a / 0.5 * 180 / math.pi
        rel5 = rel * 10
        phi5 = phi * 10
        row(n, "%.3f" % (SIGMA_Q / LSB12), "%.2f" % (sigma_a * 1e6),
            "%.4f%% / %.4f°" % (rel, phi), "%.4f%% / %.4f°" % (rel5, phi5))
    print("\nLSB = %.4f mV；σ_q = %.4f mV；DFT 噪声带宽 = f0/64（N=1024/S=16）")
    check(abs(LSB12 * 1e3 - 0.8057) < 1e-3, "LSB 与架构 §2.4 差太多")

    # ---------------- F. 校准记录与 Flash 分区 ----------------
    header("F. 校准记录 v2 与 Flash 分区（代码 256 KB + sector6/sector7）")
    blob = 1240
    per_sector = SECTOR_SIZE // CAL_SLOT
    row("cal_blob_t v2", "槽大小", "每扇区槽数", "扇区数", "扇区容量", "代码区")
    row("%d B" % blob, "%d B" % CAL_SLOT, per_sector, CAL_SECTORS, "%d KB" % (SECTOR_SIZE // 1024), "%d KB" % (CODE_FLASH // 1024))
    print("sector6 = 0x08040000–0x0805FFFF，sector7 = 0x08060000–0x0807FFFF（各 128 KB，A/B 双备份）")
    check(blob <= CAL_SLOT, "记录大于槽")
    check(per_sector * CAL_SLOT <= SECTOR_SIZE, "槽装不下扇区")
    check(CODE_FLASH + CAL_SECTORS * SECTOR_SIZE == 512 * 1024, "Flash 分区未覆盖 512 KB")
    print("写入耗时：%d B ≈ %d word × 16 µs ≈ %.2f ms；擦除 typ 1 s / max 4 s"
          % (blob, blob // 4, blob / 4 * 16e-3))

    # ---------------- G. 结论 ----------------
    header("G. 结论")
    print("• 四档 fs 均由 TIM2(84 MHz) 整数分频精确得到，定时器误差 0；")
    print("• 100 Hz 单块 640 ms → K 只能取 1；粗测必须 N=64 才能守住 ≤1 s；")
    print("• 100 kHz 采集窗 0.84 ms，DFT+结算占窗内 <5%%，1.2 MSPS 占 ADC 上限 86%%；")
    print("• 12 bit + N=1024 的 |Z| 单块 1σ ≈ 0.008%%（0.5 Vrms）/ 0.08%%（0.05 Vrms），")
    print("  0.01%% 级需 |Vo|≥0.5 Vrms 且多块平均（架构 §9.2 K 值即按此定）；")
    print("• 校准记录 1240 B/槽、1536 B 槽、85 槽/扇区，双扇区 A/B 掉电安全。")

    if FAILS:
        print("\n断言失败 %d 条：" % len(FAILS))
        for f in FAILS:
            print("  - " + f)
        return 1
    print("\n全部断言通过（0 失败）。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
