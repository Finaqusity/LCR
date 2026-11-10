## 附录 A　公式速查

| 名称 | 公式 | 说明 |
| --- | --- | --- |
| 复阻抗（串联）| $Z=R_s+jX$ | $X=\omega L-\dfrac{1}{\omega C}$ |
| 电容阻抗 | $Z_C=\dfrac{1}{j\omega C}=-j\dfrac{1}{\omega C}$ | 容抗 $X_C=-1/(\omega C)$ |
| 电感阻抗 | $Z_L=j\omega L$ | 感抗 $X_L=\omega L$ |
| 导纳 | $Y=1/Z=G+jB$ | 并联模型方便 |
| 串联↔并联 | $R_p=R_s(1+1/D^2)$，$X_p=X_s(1+D^2)$ | $D=R_s/\lvert X_s\rvert$ |
| 损耗角正切 | $D=\tan\delta=R_s/\lvert X_s\rvert$ | 电容 $D=\omega R_sC_s$，电感 $D=R_s/(\omega L_s)$ |
| 品质因数 | $Q=1/D$ | 电感常用 $Q$ |
| 自动平衡电桥母公式 | $Z_x=-Z_f\dfrac{V_{in}-V_{REF}}{V_o-V_{REF}}$ | $Z_f=R_f$ |
| 幅度比（量程）| $\dfrac{\lvert V_o-V_{REF}\rvert}{\lvert V_{in}-V_{REF}\rvert}=\dfrac{R_f}{\lvert Z_x\rvert}$ | 选档依据 |
| 同步解调 | $I=\dfrac{A}{2}\cos\varphi$，$Q=\dfrac{A}{2}\sin\varphi$ | 低通后 |
| 单频 DFT | $X=\dfrac{2}{N}\sum x[n]e^{-j2\pi f_0nT_s}$ | 相干采样 |
| 相量比 | $\dfrac{V_1}{V_2}=\dfrac{\lvert V_1\rvert}{\lvert V_2\rvert}\angle(\varphi_1-\varphi_2)$ | 复数除法 |
| 开路/短路校准 | $Z_x=\dfrac{(Z_m-Z_{short})(Z_{open}-Z_{short})}{Z_{open}-Z_m}$ | 两点校准 |
| 负载校准 | $Z_{x}=\dfrac{Z_{x,\text{raw}}}{\alpha}$，$\alpha=\dfrac{Z_{m,load}}{Z_{load,true}}$ | 复系数 |
| 电容值 | $C=\dfrac{1}{2\pi f\lvert Z\rvert}$（纯容）| 或用 $C_s=\dfrac{-1}{2\pi f X}$ |
| 电感值 | $L=\dfrac{\lvert Z\rvert}{2\pi f}$（纯感）| 或用 $L_s=\dfrac{X}{2\pi f}$ |
| 运放误差因子 | $\varepsilon=\left(1+\dfrac{Z_f}{Z_x}\right)\dfrac{1}{A}$ | 相位误差 $\approx\lvert\varepsilon\rvert$ |
| 频率字（AD9833）| $W=\text{round}\left(\dfrac{f_{out}\cdot2^{28}}{f_{MCLK}}\right)$ | $f_{MCLK}=25$ MHz |
| 相干采样条件 | $f_0T_sN=M$（整数）| $T_s=1/f_s$ |
| 类型判别（双频）| $\lvert Z(10f)\rvert/\lvert Z(f)\rvert$：≈10 感、≈0.1 容、≈1 阻 | 与相位校准无关 |

## 附录 B　题目指标 ↔ 实现要点对照

@@TABLE 表 B-1　基本要求
| 题目指标 | 本文对应章节 | 实现要点 | 主要风险 |
| --- | --- | --- | --- |
| 激励 100 Hz/1 kHz/10 kHz，0.5 Vrms ±10% | 5.1、10.3 | AD9833 或 DAC 查表，频率字计算 | 幅度随频率下降、削顶 |
| TP1 频率误差 ≤0.1% | 11.2 | 晶振实际频率修正；用示波器频率计验证 | 频率字公式错、$f_{MCLK}$ 假设错 |
| 自动识别 R/L/C 并显示 | 7.1、7.2 | 双频幅度比 + 相角；显示类型+数值+单位 | 阈值太严误判电解电容/低 Q 电感 |
| R 10 Ω～100 kΩ ≤1% | 4.5、9.2 | 0.1% $R_f$ + 继电器切换 + 负载校准 | 串联残余（短路校准）、$R_f$ 公差 |
| C 1 nF～10 μF ≤2% | 4.5、8.3 | 查表 3 选频率（1 nF 用 100 kHz/10 kHz，大电容逐级降频）、开路校准 | 夹具杂散、电解电容等效模型 |
| L 100 μH～100 mH ≤2% | 4.5、8.3 | 频率选 1 k/10 kHz；四端夹具 | 100 μH@10 kHz 只有 6.3 Ω，串联残余 |
| 测试频率自动切换 | 7.3 | 查表 3/表 4 规则 | 边界值选错档 |

@@TABLE 表 B-2　发挥部分
| 题目指标 | 本文对应章节 | 实现要点 | 主要风险 |
| --- | --- | --- | --- |
| 100 kHz 下测 C 100 pF～100 nF、L 10 μH～1 mH，单次 ≤1 s（题目未单列精度，按"与商用表比对"理解，可参照基本档 ≤2%）| 6.3、9.3 | 1 Msps 采样、$R_f$ 并小电容补偿、负载校准 | 运放相位误差、$R_f$ 寄生电容、1 pF 杂散（100 pF 时 1%）|
| 开路/短路校准后 100 kHz 测 10 pF ≤5% | 8.2～8.5 | 开路校准去夹具杂散、负载校准去增益误差 | 杂散重复性、屏蔽漏电 |
| 自动量程 R 10 Ω～1 MΩ ≤0.5% | 7.4、9.2 | 扩展到 1 MΩ 档 + 保护环 | 1 MΩ 档漏电流、噪声 |
| 单次测量 ≤1 s | 6.3.3、7.5 | 10～50 周期采样 + 3 次以内换档 | 100 Hz 下采样窗口长（100 ms）|

## 附录 C　中英文术语对照

| 中文 | 英文 | 说明 |
| --- | --- | --- |
| 自动平衡电桥 | auto-balancing bridge | 本题指定方法 |
| 复阻抗 | complex impedance | $Z=R+jX$ |
| 相量 | phasor | 幅值+相位表示正弦量 |
| 虚地 | virtual ground | 运放负端电位被强制等于正端 |
| 跨阻放大器 / I-V 转换 | transimpedance amplifier / I-V converter | 把电流转成电压 |
| 反馈电阻 | feedback resistor（$R_f$）| 决定量程 |
| 运算放大器 | operational amplifier（op amp）| 核心器件 |
| 增益带宽积 | gain-bandwidth product（GBW）| 选运放的关键参数 |
| 同步解调 / 锁相放大 | synchronous demodulation / lock-in amplifier | 模拟幅相检测 |
| 离散傅里叶变换 | DFT | 数字幅相检测 |
| 相干采样 | coherent sampling | 整数周期采样 |
| 频谱泄漏 | spectral leakage | 非相干采样的误差 |
| 最小二乘正弦拟合 | least-squares sine fit | 鲁棒的单频估计 |
| 开路/短路校准 | open/short calibration | 消除夹具寄生 |
| 负载校准 | load calibration | 消除增益/相位误差 |
| 四端开尔文连接 | 4-terminal Kelvin connection | 消除引线电阻 |
| 夹具 | fixture / test clip | 被测件连接装置 |
| 串联/并联等效 | series/parallel equivalent | 电容/电感的两种模型 |
| 损耗角正切 | dissipation factor（$D$）| 元件品质参数 |
| 品质因数 | quality factor（$Q$）| 电感常用 |
| 直流电阻 | DC resistance（DCR）| 电感线圈电阻 |
| 模拟开关 | analog switch | 信号切换 |
| 干簧继电器 | reed relay | 低接触电阻切换 |
| 印刷电路板 | PCB | — |
| 压摆率 | slew rate（SR）| 运放输出变化速度 |

## 附录 D　参考资料

### D.1 题目与官方参考设计

1. **题目原文**：《测量方向-RLC电桥测量装置》（电赛校队招新测量题，随本文档提供）。
2. **TI TIDA-060029**，*LCR Meter Analog Front-End Reference Design*，设计指南 **TIDUEU6B**（Rev. B，2020-09 首版、2021-12 更新）；中文版 **ZHCU794B**（2022-03）。
   工具页：https://www.ti.com/tool/TIDA-060029
   指南 PDF：https://www.ti.com/lit/pdf/TIDUEU6
   中文指南：https://www.ti.com.cn/cn/lit/pdf/ZHCU794
   → 本题说明中"可参考 TI《LCR 仪表模拟前端参考设计》"即指此设计。官方指标：1 Ω～10 MΩ、最高 100 kHz、阻抗精度 0.1%、使用 auto balancing（自动平衡电桥）法；公开器件为 OPA2810 + BUF634A，只有模拟前端。原理图编号 TIDM635、BOM TIDM636。

### D.2 阻抗测量理论与校准

3. **Keysight（原 Agilent）**，*Impedance Measurement Handbook: A Guide to Measurement Technology and Techniques*，**6th edition，2020**，文档号 **5950-3000**。
   PDF：https://literature.cdn.keysight.com/litweb/pdf/5950-3000.pdf
   → 第 1 章基础、第 2 章仪器、第 3 章夹具与电缆、第 4 章误差与补偿、第 5 章应用。本文第 4 章"自动平衡电桥"、第 8 章"开路/短路校准"的主要依据。
   中文全文译本（便于对照阅读）：https://www.eet-china.com/mp/a32503.html
4. **ADI UP3286**，*Precision Impedance Measurement Using Auto-Balancing Bridge Methodology*（应用笔记）。
   → 与 Keysight 手册互补的另一家原厂推导。
5. **HP Application Note 346-3**，*Using OPEN, SHORT, LOAD Correction*，文档号 **5091-6553E**。
   → 开路/短路/负载三端补偿公式的原始出处（本文第 8.3、8.4 节的公式来源）。
6. **四端开尔文连接**：https://en.wikipedia.org/wiki/Four-terminal_sensing

### D.3 器件与芯片手册

7. **AD5933** 数据手册（Rev. F）：https://www.analog.com/media/en/technical-documentation/data-sheets/AD5933.pdf
   → 本题禁用；阅读它只为理解"集成方案 vs 分立方案"的差别（内置 DDS + 12 位 ADC + 片上 DFT，上限 100 kHz、阻抗 1 kΩ～10 MΩ、系统精度 0.5%）。
8. **AD9833**（DDS，10 位 DAC，SPI 接口）：https://www.analog.com/en/products/ad9833.html
9. **OPA2810**（TI 参考设计选用运放，105 MHz、27 V、RRIO）：https://www.ti.com/product/OPA2810
10. **BUF634A**（210 MHz、250 mA 缓冲器）：https://www.ti.com/product/BUF634A
11. **STM32F407** 参考手册（双 ADC 同步模式、DAC、DMA）：ST 官网 RM0090。
    **STM32F103** 参考手册（双 ADC、无 DAC）：ST 官网 RM0008。

### D.4 可参考的公开作品（只作学习，不要照抄）

12. **ZLCR**（littlebadbay）：数字锁定放大器（DLIA）方案，AD9834 + 正交解调，1 Hz～100 kHz。
    https://github.com/littlebadbay/ZLCR
13. **MilanGb/STM32F103-LC-R-meter**：https://github.com/MilanGb/STM32F103-LC-R-meter
14. **"许老师电桥"**：国内流行的 100 kHz 自动平衡电桥方案，网上有大量固件与校准讨论。

### D.5 教材（补基础用）

15. 童诗白、华成英《模拟电子技术基础》（运放、虚地、反馈、稳定性）——第 4、10 章的基础。
16. 邱关源《电路》（正弦稳态、相量法、复阻抗）——第 2 章的基础。
17. 程佩青《数字信号处理》或 Oppenheim《离散时间信号处理》（DFT、频谱泄漏、加窗）——第 6 章的基础。

> **关于资料可信度**：本附录 1～14 项的编号、标题、URL 均经检索核对（2026-11 检索有效）；其中价格为国内经验价，非实时报价，购买前请自行核对。教材版本以手边能借到的为准，不必追求同一版次。


## 附录 E　自检题（答案在最后）

**第 1 题**　1 nF 电容在 10 kHz 时的阻抗模是多少？在 1 kHz 时呢？

**第 2 题**　自动平衡电桥中，运放的反相输入端为什么被称为"虚地"？它真的是地吗？

**第 3 题**　激励电压从 0.5 Vrms 变成 0.4 Vrms，读数会怎么变？为什么？

**第 4 题**　$R_f=10\ \text{k}\Omega$，测得 $|V_{in}|=1\ \text{V}$、$|V_o|=0.5\ \text{V}$、相位差 180°，$|Z_x|$ 是多少？

**第 5 题**　为什么测 10 μF 电容要用 100 Hz，而测 100 pF 要用 100 kHz？

**第 6 题**　短路校准和开路校准分别消除什么误差？如果不做短路校准，测 10 Ω 电阻会偏大还是偏小？

**第 7 题**　100 kHz、1 pF 的夹具杂散电容，对 10 pF 被测电容造成多大误差？方向如何？

**第 8 题**　两个通道的相位失配为 0.5°，对纯电容的容值测量影响多大？对 $D$ 值呢？

**第 9 题**　为什么不能用"快速交替采样"代替双 ADC 同步采样？100 kHz 下差多少度？

**第 10 题**　激励频率 100 kHz，采样率 1.024 MHz，采样 1024 点，包含多少个信号周期？这满足相干采样条件吗？

**第 11 题**　电感 100 μH 在 10 kHz 时 |Z| 是多少？如果要让误差 ≤2%，串联残余电阻最大允许多大？

**第 12 题**　为什么类型识别要用"双频幅度比"而不只看相角？

**第 13 题**　运放 GBW = 20 MHz，100 kHz 时 $R_f/Z_x=1$，相位误差大约多少度？对电容主值影响多大？

**第 14 题**　测出 100 kHz 时 100 pF 电容读数偏小 8%，最可能的原因是什么？怎么修？

**第 15 题**　如果只有两周时间，你会按什么顺序做？哪些可以砍？

---

### 自检题答案

**答 1**　$|Z|=1/(2\pi fC)$：10 kHz 时 $1/(2\pi\times10^4\times10^{-9})\approx15.9\ \text{k}\Omega$；1 kHz 时约 159 kΩ。

**答 2**　运放开环增益很大，负反馈保证 $V_-=V_+$；$V_+$ 接 2.5 V 基准，所以运放负端电位被"钉"在 2.5 V，对交流信号而言阻抗极低，像地一样。但它不是真的地——它是 2.5 V 的直流电位，且靠运放维持，一旦运放饱和就失效。

**答 3**　读数不变。因为测量的是 $V_{in}$ 与 $V_o$ 的比值（母公式），激励幅度同比例变化，比值不变。这正是比率测量的优点。

**答 4**　$|Z_x|=R_f\lvert V_{in}\rvert/\lvert V_o\rvert=10\ \text{k}\Omega\times1/0.5=20\ \text{k}\Omega$。相位差 180° 说明是纯电阻（$V_o$ 与 $V_{in}$ 反相）。

**答 5**　因为要把 $|Z|$ 塞进"合适窗口"（工程上取 100 Ω～100 kΩ，对应六档 $R_f$ = 10 Ω～1 MΩ）：10 μF 在 100 Hz 时 $|Z|\approx159\ \Omega$，再往高频就只剩十几欧，引线电阻占比太大；100 pF 在 100 kHz 时 15.9 kΩ，再往低频就是几 MΩ，电流太小、杂散主导。所以大电容用低频、小电容用高频，这就是"自动选频率"要做的事。

**答 6**　短路校准消除串联残余（引线电阻、接触电阻、开关导通电阻）；开路校准消除并联杂散（夹具电容）。不做短路校准，测 10 Ω 时会**偏大**（串联残余叠加在被测件上）。

**答 7**　1 pF 在 100 kHz 的阻抗是 1.59 MΩ，与 159 kΩ（10 pF）并联后约 145 kΩ：**测到的 $|Z|$ 偏小约 9%**，换算成电容就是 $C$ 读数**偏大约 10%**（10 pF 读成 11 pF）。这正是 10 pF 档必须先把夹具电容校掉的原因。

**答 8**　对纯电容：$\Delta C/C\approx\delta^2/2=(0.5^\circ=0.0087\ \text{rad})^2/2\approx3.8\times10^{-5}$，可忽略；对 $D$ 值：$D$ 会有一个约 0.0087 的虚假偏置（一阶影响）。

**答 9**　交替采样两通道有采样时刻差。若相隔 1 μs，100 kHz 信号一个周期 10 μs，相位差 $360^\circ\times1/10=36^\circ$，完全错误。必须同时启动两个 ADC。

**答 10**　周期数 $M=f_0N/f_s=10^5\times1024/1.024\times10^6=100$，是整数，满足相干采样条件。

**答 11**　$|Z|=2\pi\times10^4\times10^{-4}=6.28\ \Omega$。若要求串联残余造成的误差 ≤2%，则残余电阻 ≤0.126 Ω；实际接触+引线常达 0.1～0.5 Ω，所以**必须做短路校准**。

**答 12**　因为相角判据依赖相位校准精度，而相位误差在 100 kHz 可达 0.5° 以上；双频幅度比只依赖幅度，不受相位误差影响，更稳健。且它能区分"低 Q 电感"和"电阻"这种相角接近的情况。

**答 13**　$\varepsilon=(1+1)/A=2/(20\ \text{MHz}/100\ \text{kHz})=2/200=0.01$，相位误差约 $0.01\ \text{rad}=0.57^\circ$；对电容主值是二阶影响，约 $0.005\%$。

**答 14**　最可能是夹具并联杂散电容（1 pF 就能让电容读数偏大约 10%、$|Z|$ 偏小 9%），其次是 $R_f$ 的并联寄生电容。修法：做开路校准（存 $Z_{open}$）；检查 $R_f$ 上是否有过大的并联补偿电容；缩短引线。

**答 15**　顺序：① 1 kHz 单频测 R（双 ADC 同步 + DFT）→ ② 自动量程 R → ③ C/L 类型识别 + 1 kHz/10 kHz 两档频率 → ④ 开路/短路校准。砍掉：100 kHz 全部发挥项、1 MΩ 量程、显示 $D/Q$ 等非必要功能。原则是基本要求全达标，绝不为了发挥项牺牲基本要求。
