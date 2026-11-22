# RLC 电桥测量装置（电赛校队招新 · 测量方向）

一台基于**自动平衡电桥**的数字 RLC 表：AD9833 产生正弦激励，被测元件串在激励与 I-V 变换器之间；I-V 把流过元件的电流变成电压；两路同步采样后做单 bin DFT，得到复数阻抗 `Z = R + jX`，再换算 R / L / C、频率、品质因数与串并联模型。

> 硬件已定稿（4 层板 100×100 mm，已出 Gerber）。当前阶段：**固件重写**——换新实现者从零实现，交接依据见 [docs/固件重写交接说明.md](docs/固件重写交接说明.md)；此前的架构/固件实现已冻结，仅作参考。

## 文档索引

### 核心文档（`docs/`）

| 文档 | 内容 |
|---|---|
| [docs/固件重写交接说明.md](docs/固件重写交接说明.md) | **新实现者的交接依据**：测量原理、题目指标、硬件电路事实、引脚定义、外设限制与安全边界；不规定算法与代码结构 |
| [docs/硬件接口.md](docs/硬件接口.md) | **固件开发的唯一接口依据**：MCU 引脚、外设配置、AD9833 频率字、ADC/TIM2 同步采样、继电器映射、测量与校准流程、串口协议（与架构文档 §14.1 冲突处以架构文档为准） |
| [docs/软件架构.md](docs/软件架构.md) | 上一版实现的设计文档 v1.0（已冻结，供参考；新实现不必遵循，见固件重写交接说明）：分层与 C API 契约、寄存器级驱动、折叠 DFT、测量内核、校准、内存/Flash/时序、验收与里程碑 |
| [docs/软件协作交接说明.md](docs/软件协作交接说明.md) | **给队友 AI Agent 的交接说明**：必读顺序、权威性规则、工具链与验证门槛、Git 协作、任务队列 M0–M8、编码规范、已知坑 |
| [docs/软件任务板.md](docs/软件任务板.md) | 共享任务认领板（写作用域 / 验收标准 / 状态） |
| [docs/交接简报.md](docs/交接简报.md) | **群发版一页上手**：三分钟跑通、必读顺序、15 条定稿红线、可接任务、协作规则、给 AI Agent 的开场指令（[PDF](docs/交付物/交接简报.pdf)） |
| [docs/开发环境.md](docs/开发环境.md) | 工具链路径与版本、编译/烧录/串口三条命令、VS Code 调试、CubeMX 用法、构建坑 |
| [docs/GitHub同步.md](docs/GitHub同步.md) | 远端仓库创建、首次推送、日常 pull/commit/push、大文件与协作约定 |

### 设计与原理资料

| 文档 | 内容 |
|---|---|
| [docs/硬件设计思路.md](docs/硬件设计思路.md) | 讲解版硬件设计思路（答辩 / 报告素材）：信号链、量程、校准、器件选型 |
| [docs/硬件设计-六层板.md](docs/硬件设计-六层板.md) | 六层板设计细则（最终投的是四层板，本文为设计依据与存档） |
| [docs/硬件设计-分板与四层板.md](docs/硬件设计-分板与四层板.md) | 分板 / 四层板阶段方案记录（存档） |
| [docs/硬件设计-供电与时钟.md](docs/硬件设计-供电与时钟.md) | 电源树与时钟专题 |
| [docs/交付物/](docs/交付物/) | 导出的 docx/pdf 成品（原理详解 60 页、硬件设计思路、六层板、原理图、原始题目、交接简报），来源与重生成命令见 [docs/交付物/README.md](docs/交付物/README.md) |
| [docs/源稿/](docs/源稿/) | 原理详解的 markdown 源文（part1–6，导出 docx/pdf 的源；原在本地草稿区，已入库） |
| [docs/数据手册.md](docs/数据手册.md) | 核心器件数据手册索引（本地 `_ds/` PDF + 官方下载链接） |
| [docs/历史/](docs/历史/) | 已被取代的旧文档（仅存档） |
| [`build_pcb/gerber/`](build_pcb/gerber/) | 已送样的 Gerber（4 层，100×100 mm） |

## 快速开始（Windows PowerShell）

> 推荐用 VS Code 直接开发：打开本仓库 → `Ctrl+Shift+B` 编译（默认 `build (Debug)`）→ 接 DAPmini 后 `F5` 调试（Cortex-Debug + pyOCD）。命令行方式如下。

```powershell
# 0. 接好 DAPmini（CMSIS-DAP：SWDIO/SWCLK/GND，可选 NRST），确认能识别
powershell -ExecutionPolicy Bypass -File tools\flash.ps1 -List

# 1. 编译（默认 Debug；加 -Release 出优化版，-Clean 先清空）
powershell -ExecutionPolicy Bypass -File tools\build.ps1
#    产物：firmware\build\lcr-fw.elf / .hex / .bin / .map

# 2. 烧录并复位（pyOCD，目标 stm32f407vetx）
powershell -ExecutionPolicy Bypass -File tools\flash.ps1

# 3. 看串口（USART1 → CH340，115200 8N1；脚本会自动找可用 python）
powershell -ExecutionPolicy Bypass -File tools\monitor.ps1 --list
powershell -ExecutionPolicy Bypass -File tools\monitor.ps1 --port COM5
```

> 直接调 `python tools\monitor.py` 会命中 Windows Store 的 `python` 占位别名而报 "Python was not found"，请用 `tools\monitor.ps1`（详见 [docs/开发环境.md](docs/开发环境.md) §1/§3）。

烧录成功、串口连上后，当前 **Day1 骨架固件**应输出类似：

```
========================================
  LCR Meter Firmware v0.1.0 (build ...)
  MCU: STM32F407VET6 @ 16 MHz (HSI, 待 M0 换成 168 MHz)
========================================
$ tick=500
$ tick=1000
```

输入 `?` 看当前命令表、`v` 看版本。**注意**：master 上现在只是 Day1 心跳/串口**骨架**（LED 暂用 PD12，实物在 PC13，测试以串口为准）；旧版完整实现在分支 `feat/abb-rlc-firmware`（含 ADC/DFT/测量/校准/协议，已评审并冻结，见 [Issue #1](https://github.com/Finaqusity/LCR/issues/1)），新实现从零重写，以 [docs/固件重写交接说明.md](docs/固件重写交接说明.md) 为唯一交接依据。

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
tools/         build.ps1 / flash.ps1 / monitor.ps1 / monitor.py / cubemx.ps1 / export_pdf.ps1 / verify/budget.py
.vscode/       共享的 VS Code 配置（编译任务、Cortex-Debug 调试、IntelliSense）
docs/          核心文档（硬件接口、软件架构、协作交接、任务板、开发环境、GitHub 同步）
docs/源稿/     原理详解 markdown 源文（part1–6）
docs/交付物/   导出的 docx/pdf 成品与原始题目（来源见该目录 README.md）
docs/历史/     过时文档存档
build_sch/     原理图与设计图的生成脚本和 PNG
build_pcb/     Gerber 与 PCB 渲染
_ds/           本机数据手册 PDF（gitignore，不入库；索引见 docs/数据手册.md）
```

## 状态

- [x] 原理详解文档（60 页）
- [x] 硬件设计 + 原理图 + 六层板文档（实际做成 4 层）
- [x] 网表 5 轮复审，Gerber 已出并审过
- [x] 软件架构文档 v1.0（14 章：分层/契约/驱动/DFT/测量内核/校准/内存/测试/里程碑/风险/差异）——**上一版实现的设计，已冻结，新实现不必遵循**
- [x] AI Agent 协作交接说明 + 共享任务板
- [x] 仓库整理：交付物归入 `docs/交付物/`、Drivers 裁剪 38.5→6.1 MB、原理源文与数值核算脚本入库
- [x] 固件重写交接说明 `docs/固件重写交接说明.md`（测量原理、指标、引脚、硬件限制；不规定算法与代码结构）
- [x] 固件骨架（时钟/USART/LED）已入库；旧版完整实现在分支 `feat/abb-rlc-firmware`（评审见 [Issue #1](https://github.com/Finaqusity/LCR/issues/1)，已冻结）
- [ ] 固件重写：由新实现者按交接说明从零实现（原 M0–M8 计划作废）
- [ ] 焊接、上电自检、指标实测（24 项验收矩阵）
