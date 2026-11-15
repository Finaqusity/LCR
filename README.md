# RLC 电桥测量装置（电赛校队招新 · 测量方向）

一台基于**自动平衡电桥**的数字 RLC 表：AD9833 产生正弦激励，被测元件串在激励与 I-V 变换器之间；I-V 把流过元件的电流变成电压；两路同步采样后做单 bin DFT，得到复数阻抗 `Z = R + jX`，再换算 R / L / C、频率、品质因数与串并联模型。

> 硬件已定稿（4 层板 100×100 mm，已出 Gerber），当前阶段：**写固件**。

## 文档索引

| 文档 | 内容 |
|---|---|
| [docs/硬件接口.md](docs/硬件接口.md) | **固件开发的唯一接口依据**：MCU 引脚、外设配置、AD9833 频率字、ADC/TIM8 同步采样、继电器映射、测量与校准流程、串口协议（注意：与架构文档 §14.1 冲突处以架构文档为准） |
| [docs/软件架构.md](docs/软件架构.md) | **软件架构 v1.0（权威）**：分层与 C API 契约、寄存器级驱动、折叠 DFT、测量内核、三级校准、内存/Flash/时序、验收与里程碑、与 `硬件接口.md` 的 30 条差异汇总 |
| [docs/软件协作交接说明.md](docs/软件协作交接说明.md) | **给队友 AI Agent 的交接说明**：必读顺序、权威性规则、工具链与验证门槛、Git 协作、任务队列 M0–M8、编码规范、已知坑 |
| [docs/软件任务板.md](docs/软件任务板.md) | 共享任务认领板（写作用域 / 验收标准 / 状态） |
| [docs/开发环境.md](docs/开发环境.md) | 工具链路径与版本、编译/烧录/串口三条命令、VS Code 调试、构建坑 |
| [docs/GitHub同步.md](docs/GitHub同步.md) | 远端仓库创建、首次推送、日常 pull/commit/push、大文件与协作约定 |
| [RLC电桥测量原理详解.docx](RLC电桥测量原理详解.docx) / [.pdf](RLC电桥测量原理详解.pdf) | 原理详解（13 章 + 附录，60 页）——"为什么这么设计" |
| [硬件设计-六层板.docx](硬件设计-六层板.docx) / [.pdf](硬件设计-六层板.pdf) | 叠层、电源、接地、分区、最小系统、布线规则、工艺与检查清单 |
| [测量部分原理图示意.docx](测量部分原理图示意.docx) / [.pdf](测量部分原理图示意.pdf) | AFE 四张原理图（信号源 / 前端 / 六档 / 双通道）+ BOM |
| [模拟部分原理图集.pdf](模拟部分原理图集.pdf) | S0–S5 六页总览，画图/接线时对照用 |
| `build_pcb/gerber/` | 已送样的 Gerber（4 层，100×100 mm） |

## 快速开始（Windows PowerShell）

```powershell
# 0. 接好 DAPmini（CMSIS-DAP：SWDIO/SWCLK/GND，可选 NRST），确认能识别
powershell -ExecutionPolicy Bypass -File tools\flash.ps1 -List

# 1. 编译（默认 Debug；加 -Release 出优化版，-Clean 先清空）
powershell -ExecutionPolicy Bypass -File tools\build.ps1
#    产物：firmware\build\lcr-fw.hex / .bin / .map（以及无扩展名的 ELF）

# 2. 烧录并复位（pyOCD，目标 stm32f407vetx）
powershell -ExecutionPolicy Bypass -File tools\flash.ps1

# 3. 看串口（USART1 → CH340，115200 8N1）
python tools\monitor.py --list
python tools\monitor.py --port COM5
```

烧录成功、串口连上后，当前 **Day1 骨架固件**应输出类似：

```
========================================
  LCR Meter Firmware v0.1.0 (build ...)
  MCU: STM32F407VET6 @ 16 MHz (HSI, 待 M0 换成 168 MHz)
========================================
$ tick=500
$ tick=1000
```

输入 `?` 看当前命令表、`v` 看版本（命令会随 M0 起逐步接入）。**注意**：现在烧进去的只是心跳/串口骨架，测量功能按 [docs/软件架构.md](docs/软件架构.md) 与 [docs/软件任务板.md](docs/软件任务板.md) 的 M0–M8 推进；骨架里 LED 暂用 PD12，而实物 LED 在 PC13（M0-1 会修正，测试以串口为准）。

工具链已装好（`D:\Tools\embedded\arm-gcc-14.2`、Python 里的 cmake/ninja/pyocd），构建告警与坑见 [docs/开发环境.md](docs/开发环境.md)。

## 硬件要点

- 主控：STM32F407VET6（512 KB Flash / 128 KB RAM + 64 KB CCM），HSE 8 MHz → SYSCLK 168 MHz
- AFE：9 V 单电源、参考 VREF_AFE = 3.3 V；激励链 AD9833 → OPA2810 → BUF634A；I-V = OPA2810 + BUF634A
- 六档反馈电阻：10 Ω / 100 Ω / 1 kΩ / 10 kΩ（默认）/ 100 kΩ / 1 MΩ，G6K-2F-Y 继电器切换
- 采样：ADC 21 MHz；`S = fs/f0` 取 16（100 Hz/1 kHz/10 kHz，N=1024）或 12（100 kHz，fs=1.2 MSPS、N=1008）；TIM2_TRGO（EXTSEL=0b0110）触发 ADC1/ADC2 双规则同步 + 循环 DMA（缓冲在 SRAM，不能进 CCM）
- 串口：CH340 + Type-C，115200 8N1，ASCII 行协议
- 夹具：4×BNC 四端开尔文（F+ / S+ / F− / S−）

## 目录

```
firmware/      固件（CMake + arm-none-eabi-gcc，裸机 CMSIS）
tools/         build.ps1 / flash.ps1 / monitor.py
docs/          硬件接口、开发环境、GitHub 同步、硬件设计 markdown
build_sch/     原理图与设计图的生成脚本和 PNG
build_pcb/     Gerber 与 PCB 渲染
*.docx/.pdf    交付文档
```

## 状态

- [x] 原理详解文档（60 页）
- [x] 硬件设计 + 原理图 + 六层板文档（实际做成 4 层）
- [x] 网表 5 轮复审，Gerber 已出并审过
- [x] 软件架构文档 v1.0（14 章：分层/契约/驱动/DFT/测量内核/校准/内存/测试/里程碑/风险/差异）
- [x] AI Agent 协作交接说明 + 共享任务板
- [ ] 固件：时钟/USART/LED 骨架 ✅ → M0 时钟+链接脚本+测试设施 → AD9833/继电器 → ADC/TIM2/DMA/DFT → 量程与校准 → OLED 与协议
- [ ] 焊接、上电自检、指标实测（24 项验收矩阵）
