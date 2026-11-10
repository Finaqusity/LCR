# RLC 电桥测量装置（电赛校队招新 · 测量方向）

一台基于**自动平衡电桥**的数字 RLC 表：正弦激励被测元件，用 I-V 变换器把流过元件的电流变成电压，双通道同步采样后做 DFT，得到复数阻抗 $Z=R+jX$，再换算成 R / L / C 与频率、串并联模型。

## 目录

| 路径 | 内容 |
|---|---|
| `RLC电桥测量原理详解.docx` / `.pdf` | 原理详解（13 章 + 附录 A–E，60 页）；`src/part*.md` 是源码，`build_docx.py` 生成 docx |
| `docs/环境搭建.md` | 工具链、编译、烧录、调试、串口、踩坑记录 |
| `firmware/` | 固件（Cortex-M4，CMake 工程，当前是最小可跑骨架） |
| `tools/` | `build.ps1` 编译、`flash.ps1` 烧录、`monitor.py` 串口、`_fetch_arm_gcc.ps1` 下工具链 |
| `figs/` | 文档插图（matplotlib 生成，`make_figs.py`） |
| `compute/` `verify/` | 指标/误差/量程/时序的数值核算脚本与报告 |
| `research/` | 器件与参考资料调研笔记 |

## 快速开始（Windows，PowerShell）

```powershell
# 1. 编译
powershell -ExecutionPolicy Bypass -File .\tools\build.ps1

# 2. 烧录（DAPmini / CMSIS-DAP，pyOCD）
powershell -ExecutionPolicy Bypass -File .\tools\flash.ps1

# 3. 看串口（USART1：PA9/PA10，115200 8N1）
python .\tools\monitor.py --list
python .\tools\monitor.py --port COM5
```

工具链位置与详细步骤见 [docs/环境搭建.md](docs/环境搭建.md)。全部便携安装，无需管理员权限、不改系统 PATH。

## 硬件（当前）

- 主控：STM32F407VET6（512 KB Flash / 192 KB RAM，双 ADC，100 kHz 相干采样够用）
- 调试器：DAPmini（CMSIS-DAP，HID 免驱）
- 固件骨架：SysTick、LED（PD12）、USART1 中断接收环形缓冲、`uart_printf`、启动横幅与命令回显
- 串口协议（规划）：启动横幅 `# LCR fw=...`、数据帧 `$ seq,ms,f_Hz,gear,|Z|,phi,type,D`、命令 `help / freq / raw on / cal open|short / dump / reboot`

## 团队协作（3 人）

- `main` 保护，短分支 + PR；提交前缀 `feat: / fix: / docs: / test: / chore:`
- 接口先写进 `docs/`（引脚、串口协议、数据结构），代码再跟上
- 每周打 tag（如 `v0.3-basic-R`）；赛前冻结代码，`git tag v-final` + `git bundle` 备份

## 状态

- [x] 原理文档（60 页，含 8 张插图、15 道自检题）
- [x] 命令行工具链（gcc 14.2.1 + CMake + Ninja + pyOCD）在 D 盘可用
- [x] 固件骨架编译通过（Debug：text 17.2 KB / data 468 B / bss 628 B）
- [ ] 板子实机烧录与串口联通
- [ ] 激励源 + 缓冲 + I-V + 双 ADC 同步采样
- [ ] DFT 求 |Z|、φ，自动量程，开路/短路校准
