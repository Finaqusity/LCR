# RLC 电桥测量装置 参考资料检索与核实

> 对应题目：`测量方向-RLC电桥测量装置.docx`（自动平衡电桥法、+5 V 单电源、0.5 Vrms、100 Hz–100 kHz、禁 AD5933）。
> 检索日期：2026-11-09。标注约定：**【事实】** = 已在所附链接原文中读到；**【推断】** = 由事实推导但无原文佐证，需实测/复核。

## 1. TI《LCR 仪表模拟前端参考设计》（题目说明(4)所指）

1.1 标题与编号【事实】
- 英文：TIDA-060029 **"LCR Meter Analog Front-End Reference Design"**；中文官网译名「LCR 表模拟前端参考设计」。
- 设计指南：**TIDUEU6B**《LCR Meter Analog Front-End Reference Design (Rev. B)》，首版 2020-09，Rev. B 到 2021-12（文档页日期 2021-12-21）；中文版 **ZHCU794B**（2022-03-04）。
- 官方页：<https://www.ti.com/tool/TIDA-060029>（中文 <https://www.ti.com.cn/tool/cn/TIDA-060029>）；指南 PDF <https://www.ti.com/lit/pdf/TIDUEU6>、中文 <https://www.ti.com.cn/cn/lit/pdf/ZHCU794>。

1.2 公开指标【事实，取自 TI 工具页 Features】
- 阻抗 1 Ω–10 MΩ；最高频率 **100 kHz**；实测点 100 Hz / 1 kHz / 10 kHz / 100 kHz；阻抗精度 **0.1%**；强调 "inherently stable operation of signal chain"。
- 官方 Overview 明确写该设计使用 **auto balancing impedance measurement method**（自动平衡电桥法）。

1.3 框图与器件【事实 + 推断】
- 【事实】公开页面列出的设计器件只有两颗：**OPA2810**（双通道、27 V、105 MHz、轨到轨输入输出、FET 输入，<https://www.ti.com/product/OPA2810>）与 **BUF634A**（210 MHz、250 mA 高速缓冲器，<https://www.ti.com/product/BUF634A>）。
- 【推断，需核对原理图 TIDM635】OPA2810 作 I-V 转换/误差放大器（反相端即虚地 "L" 点），BUF634A 作激励输出缓冲；激励 DAC/DDS、双通道同步 ADC、MCU **不在该设计的器件清单里**，需自选（TI E2E 有帖 "TIDA-060029: ADC and DAC recommendations"，页面被 captcha 挡，只核实到标题）。
- 【事实，取自指南目录】2.3.1 指出旧架构的**电路稳定性问题**并加**补偿电容**；2.3.2 给出稳定性分析、"RG = RF Settings and Respective Impedance Ranges"（量程电阻与反馈电阻设置）、"Impedance Measurement Procedure"；校准小节依次为 Short Cal → Impedance Cal → 100k Setting Cal → Open Cal → Correction in ZX。
- 【事实】配套文件编号：原理图 TIDM635、BOM TIDM636、装配 TIDM637、PCB 图 TIDM638、CAD TIDM639、Gerber TIDCFV8。

1.4 与本题目关系【推断】
- 上限 100 kHz、I-V 转换 + 可切量程电阻、0.1% 精度目标与本题几乎同构，可直接当电路骨架参考；但它只是"模拟前端"，数字激励与采集要自己设计，这与题目不许用现成 LCR 模块并不冲突。

## 2. AD5933 的架构与禁用原因

2.1 架构【事实，ADI 产品页 + 数据手册】
- 定位："1 MSPS, 12-Bit Impedance Converter, Network Analyzer"；内部 = **DDS 激励（27 位频率分辨率）+ 12 位 DAC + 12 位 1 MSPS ADC + 片上 DFT**，直接输出阻抗实部/虚部。
- 关键限制：激励正弦最高 **100 kHz**；阻抗测量范围 **1 kΩ–10 MΩ**（100 Ω–1 kΩ 需外加电路）；**系统精度 0.5%**；I²C；2.7–5.5 V；16 脚 SSOP；内部温度传感器 ±2 °C。
- 数据手册 Rev F：<https://www.analog.com/media/en/technical-documentation/data-sheets/AD5933.pdf>；产品页 <https://www.analog.com/en/products/ad5933.html>。
- 【事实】ADI 自述 AD5933/AD5934 是 "roughly 15 years ago" 推出的第一代阻抗分析芯片：<https://www.analog.com/en/resources/technical-articles/jack-of-all-trades-in-impedance-measurement.html>

2.2 为什么本题禁用、且它达不到 100 kHz / 10 pF 指标【推断，需实测对比】
- 100 kHz 正是其上限，无余量；10 pF@100 kHz 阻抗约 159 kΩ、100 pF 约 15.9 kΩ，虽落在 1 kΩ–10 MΩ 内，但 0.5% 系统精度 + 内部 12 位 ADC/DFT + 固定内部 I-V 结构，撑不起"10 pF 误差 5%"和"R 0.5%"。
- 它把 DDS+I-V+ADC+DFT 全部集成并直接给出阻抗，属于题目明令禁止的"集成阻抗测量芯片"；题目要求的是"运放把流过被测件的电流变成电压"这一分立自动平衡电桥结构。
- 【推断】它更适合当"对照组"：说明集成方案在量程切换自由度、校准自由度、相位精度上为何不如分立自平衡电桥。

## 3. 自动平衡电桥法：权威出处、原理、校准、精度

3.1 必读手册【事实】
- Keysight/Agilent《Impedance Measurement Handbook: A Guide to Measurement Technology and Techniques》，**6th edition，2020**，文档号 **5950-3000**。
- PDF：<https://literature.cdn.keysight.com/litweb/pdf/5950-3000.pdf>（直连常被反爬挡住，浏览器可开）；下载页 <https://connectlp.keysight.com/EMO_Impedance-Measurement-Handbook>。
- 章节：1 Basics / 2 Instruments / 3 **Fixturing and Cabling** / 4 **Measurement Error and Compensation** / 5 Applications。
- 中文全文译本（内容与手册一致，便于引用中文原句）：<https://www.eet-china.com/mp/a32503.html>

3.2 定义与要点【事实，引自 3.1 的中文译本原文】
- "流过 DUT 的电流也流过电阻器 Rr。'L' 点的电位保持为 0 V（从而称为'虚地'）。I-V 转换放大器使 Rr 上的电流与 DUT 的电流保持平衡。测量高端电压和 Rr 上的电压，即可计算出 DUT 的阻抗值。"
- "常规 LCR 表的低频范围一般低于 100 kHz，可使用简单的运算放大器作为它的 I-V 转换器。由于受到放大器性能的限制，这类仪器在高频时的精度较差。"
- 该法"最准确，基本测试精度 0.05%"，最佳区间 f < 110 MHz；射频 I-V 法基本精度 0.8%。
- ADI 专文：UP3286《Precision Impedance Measurement Using Auto-Balancing Bridge Methodology》<https://ez.analog.com/precision-technology-signal-chains/precision-narrow-bandwidth/a/ultra-precision-faqs/c/all-ultra-precision-tech-notes/UP3286/precision-impedance-measurement-using-auto-balancing-bridge-methodology>

3.3 为什么需要多个量程电阻 Rr【推断，需实测】
- 复阻抗 Zx = Vx/Ir，而 Ir = Vr/Rr，故 **Zx = Rr·(Vx/Vr)**：Rr 只改变增益、不改变相位关系，所以"测 Vx 与 Vr 的比值"即可得 Zx。
- Rr 与被测阻抗同量级时 Vr 接近满量程、SNR 最好；Rr 过小 → Vr 被噪声/失调淹没；Rr 过大 → 运放输出饱和、环路带宽下降。故 10 Ω–1 MΩ 需 5–7 档（TIDA-060029 的 "RG = RF Settings and Respective Impedance Ranges" 即讲此事）。

3.4 校准公式【事实，出自 Keysight 手册第 4 章 / HP AN 346-3】
- 开路补偿（消除并联杂散，适合高阻抗/小电容）：**Y_dut = Y_meas − Y_open**，即 Z_dut = 1/(1/Z_meas − 1/Z_open)。
- 短路补偿（消除串联残余，适合低阻抗）：**Z_dut = Z_meas − Z_short**。
- 开路-短路-负载三端补偿（高精度）：**Z_dut = Z_std · (Z_s − Z_xm)(Z_sm − Z_o) / [(Z_xm − Z_o)(Z_s − Z_sm)]**；Z_xm=实测 DUT，Z_s=短路读数，Z_o=开路读数，Z_sm=标准件读数，Z_std=标准件真值（公式转述见 BYU 论文 <https://physics.byu.edu/docs/thesis/1646>）。
- 原始文献：HP Application Note 346-3《Using OPEN, SHORT, LOAD Correction》（文档号 5091-6553E），镜像 PDF <http://bee.mif.pg.gda.pl/ciasteczkowypotwor/HP/Publikacje/5091-6553E.pdf>。
- 【推断】本题"开路校准 + 短路校准"对应前两式；发挥部分要 0.5%，建议再加"标准电阻负载校准"（即三端式）。

3.5 测量精度与相位精度的关系【事实 + 推断】
- 【事实】手册把 |Z|、θ、D、Q 的精度分别规定（第 4 章），D/Q 精度主要取决于相位精度。
- 【推断，需实测】对接近纯电抗的元件（小 D），C/L 值误差≈幅值误差（一阶），相位误差主要体现为 D 误差（δθ ≈ δD）；但 100 kHz 下 10 pF/5% 的真正瓶颈通常在**相位与夹具杂散**，不在幅值。

## 4. 国内常见器件与参考单价

> 立创商城/淘宝多为 JS 渲染或需登录，未取到可核实的实时价；标【未核实】者为经验价，务必现场核对。

4.1 激励源（DDS/DAC）
- AD9833（0–12.5 MHz、SPI、需 25 MHz 外部时钟）：模块约 10–20 元（旁证："一块十几块的 AD9833 模块" <http://mp.weixin.qq.com/s?__biz=MzI5NzQxNzU0Ng==&mid=2247493258&idx=1&sn=7054eafcfc1449fbb356e08206674b43>）。
- 【推断】10–100 kHz 也可用 DAC + 查表正弦（如 STM32 内置 12 位 DAC），成本更低、频率/相位可精确控制；但需外部低通滤波与幅度稳定。

4.2 MCU
- STM32F103C8T6（72 MHz、双 12 位 ADC 可同步采样、无 DAC）：最小系统板约 8–15 元【未核实】。
- STM32F407VET6（168 MHz、3×12 位 ADC、2×12 位 DAC、FPU 便于做正交解调/DFT）：核心板约 35–60 元【未核实】；【推断】要做 100 kHz 且 1 s 内出结果，F407 更稳。

4.3 运放 / 缓冲
- **OPA2810**：105 MHz、27 V、RRIO、FET 输入（TIDA-060029 选用）；约 20–40 元【未核实】。
- **OPA320 / OPA2320**：20 MHz GBW、轨到轨输出、1.8–5.5 V、1.5 mA、40 µV 失调、65 mA 输出 <https://i-ic.com/Products/OPA320AIDBVT.html>；双路 OPA2320 约 8–20 元【未核实】。适合 ≤10 kHz 档位的 I-V 转换。
- **OPA656 / ADA4817**：FET 输入、GBW 分别约 230 MHz / 410 MHz【未核实，请查数据手册】，需 ±5 V 供电、非轨到轨；约 30–60 元【未核实】。→ 与本题 +5 V 单电源冲突，用它们就得先做负压【推断】。
- **BUF634A**：210 MHz、250 mA 缓冲器，可作激励输出级；约 15–30 元【未核实】。
- **TLV3201**：不是运放，是 40 ns 响应、1.2 mV 内滞回的比较器 <https://www.ti.com/lit/ds/symlink/tlv3201-q1.pdf>；可做过零检测/方波整形，约 3–6 元【未核实】。

4.4 模拟开关与基准
- CD4053（3×2 通道，导通电阻百 Ω 级）：约 1–2 元【未核实】；【推断】100 kHz 下更推荐低电荷注入的 ADG 系列（ADG1408 约 15–30 元【未核实】）。
- LM4040（2.5 V/4.096 V 并联基准，0.1% 档）：约 2–4 元【未核实】；【推断】+5 V 单电源下可选 REF3025/REF3030（3.0 V 串联基准）约 4–8 元【未核实】。

4.5 其他
- 量程电阻需 0.1%、低温漂（10 ppm/°C 级）金属膜电阻：【推断】10 kΩ 0.1% 约 1–2 元/只。
- 小电容夹具：双面 PCB 保护环 + 屏蔽盒【推断】；杂散 1 pF 对 10 pF 就是 10% 量级误差。

## 5. 100 kHz 及以上的常见工程坑

5.1 夹具杂散电容
- 手册第 3 章专讲夹具与电缆（杂散电容、残余电感、屏蔽/保护），第 4 章讲误差与补偿；10 pF 在 100 kHz 时 |Z|≈159 kΩ，几 pF 杂散即造成 10%+ 误差 → 必须开路校准 + 屏蔽/保护环。
5.2 四端（Kelvin）接法
- 题目说明(3)允许两端或四端；四端把电流激励线与电压检测线分开，消除引线/接触电阻，低阻档（10 Ω）尤其必要。参考 <https://en.wikipedia.org/wiki/Four-terminal_sensing>。
5.3 地环路与共模
- 【推断，需实测】+5 V 单电源需 2.5 V 虚地；激励、I-V 转换、ADC 三处地回流若共用一段走线，会经地阻抗串扰并在 100 kHz 引起误差 → 星形/单点接地、模拟地与数字地分开、输入端保护环；手册第 3 章为通用依据。
5.4 通道间采样偏斜（skew）
- 【推断，需实测（可直接算）】若 Vx 与 Vr 非同一时刻采样，相位误差 Δθ = 360°·f·Δt：100 kHz 下 100 ns → 3.6°，1 µs → 36°。必须用双 ADC 同步模式（如 STM32 双 ADC 规则组同步），或同一 ADC 交替采样再做时间校正。
5.5 运放有限 GBW 造成的 I-V 误差
- 【事实】TIDA-060029 指南专门有"电路稳定性问题 → 补偿电容 → 稳定性分析"章节（见 1.3）。
- 【事实】开源项目 ZLCR 作者直言"运放构成的自平衡电桥不适合工作在 100 kHz 以上频率" <https://github.com/littlebadbay/ZLCR>。
- 【推断】100 kHz 时 I-V 转换器环路增益下降 → 幅值误差与相位滞后；RF 上并几 pF 做相位补偿、选 GBW ≥ 10×f 的运放。
5.6 单电源虚地的电源噪声
- 【推断】虚地缓冲运放的噪声/输出阻抗会直接叠加到激励与参考上；建议低噪声基准 + 大容量去耦 + 独立缓冲；同时用"Vx/Vr 比值"测量，可抵消激励幅度与共模漂移——这正是自动平衡电桥相对其他方法的优势之一。

## 6. 可参考的公开作品（只作学习，不要抄）

1. **ZLCR**（littlebadbay）：简单硬件 + **DLIA 数字锁定放大器**（正交解调）替代模拟检波，AD9834 DDS，1 Hz–100 kHz、1 mΩ–10 MΩ；>100 kHz 改用"数字平衡电桥 + 欠采样" <https://github.com/littlebadbay/ZLCR>。
2. **MilanGb/STM32F103-LC-R-meter**：仓库标题 "Easy precision hoby LC R meter"（仅核实标题，方案未核实）<https://github.com/MilanGb/STM32F103-LC-R-meter>。
3. **"许老师电桥"**：国内流行多年的 100 kHz 自动平衡电桥方案（原 12C5A60S2，后移植 STM32），网上有大量固件/校准讨论 <https://pre.fangzhenxiu.com/post/12921702/>；B 站演示「（电赛训练）基于许老师电桥和STM32VET6的高精度RLC测量仪」<https://www.bilibili.com/video/BV1XP411r7ER/>（页面需人机验证）。

## 未能核实的点

1. **TIDA-060029 官方框图文字**（激励源型号、双通道同步 ADC 型号、MCU 型号）未取到：TI PDF 无法解析、HTML 只返回目录、E2E 帖被 captcha 挡；1.3 的器件分工是推断。
2. 手册 6th ed. 的**确切出版日期**只有二手引文（2020）；PDF 直连被反爬拦截，未能逐字核对第 4 章公式编号与页码。
3. 三端（OPEN-SHORT-LOAD）公式仅从 BYU 论文转述取得，未在手册原文中逐字确认。
4. **立创商城/淘宝实时单价**未取到（JS 渲染/需登录）；除 AD9833 模块"十几块"的旁证外，第 4 节价格均为经验值，已标【未核实】。
5. 开源项目 MilanGb、x893/rlc-meter-balmer、B 站视频的具体电路方案未核实（抓取失败或需验证码）。
6. 相位精度与 |Z| 精度的官方换算表述未取到原文；3.5 的关系为推断。
