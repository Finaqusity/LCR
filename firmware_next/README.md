# 新固件独立工程

当前源码已接通复位启动、板级初始化、双 ADC/TIM/DMA、测量与校准调度、单键/OLED 和 Flash 双副本保存。没有读取或修改旧应用代码。

本机已在 Windows + `arm-none-eabi-gcc 14.2`（`D:\Tools\embedded\arm-gcc-14.2\bin`，可用 `-Toolchain` 覆盖）下实际构建出 `build-target/lcr-next.elf/.hex/.bin/.map`；电脑端 9 个 C 测试程序已运行通过，23 个源文件通过 Cortex-M4 / hard-float / `-Wall -Wextra -Werror` 编译。**已在 ST-Link 上板验证启动、单键路径、量程继电器写入与内部 DAC 激励（AD9833 已失效移除）；模拟前端与精度验收仍未做。**

操作、构建、测试和剩余验收见 [整机固件操作与验收](../docs/rewrite/整机固件操作与验收.md)。硬件依据见 [硬件核对与实施记录](../docs/rewrite/硬件核对与实施记录.md)。

## 开发板适配：单键 + OLED 正向

- 板载按键接 **WK_UP / PA0**，高有效（按下为高，固件开内部下拉）；旧板 PB1/PB2 两个按键不再使用。
- **V1 改接 PC0（ADC1_IN10）**，因为 PA0 已被按键占用；V2 仍为 PA1（ADC2_IN1）。接线时需把 AFE 的 V1 输出从 PA0 移到 PC0。
- OLED 正向摆放 0°（`LCR_OLED_ROTATE_180=0`），发送时不再做 180° 像素反转。
- 主数值自动选最大可用字号：放得下时 3×（字符 18×30 px）并把单位放大到 2×，放不下回退 2×；数值行水平居中，标题/频率/校准行保持 1× 紧凑信息。

| 场景 | 短按（单击） | 长按（800 ms） | 双击（300 ms 内两次） |
|---|---|---|---|
| 测量主页面 | 启动/停止连续测量 | 进入设置菜单并停止测量 | 切换 SERIES/PARALLEL |
| 设置菜单 | 下一项 | 修改/确认当前项 | 返回测量主页面 |
| 校准确认页 | 取消 | 开始校准 | — |
| 校准进行中 | 取消并保留原校准 | — | — |
| 提示/报错页 | 重测（回测量页并立即开始） | 回设置菜单 | — |

短按要等双击窗口结束才上报，因此单击有约 0.3 s 判定延迟；长按结束后不再产生短按；上电已按住的键不产生动作。串并联切换使用同一份 Z，不重新采样或切换继电器。停止后显示 HOLD。配置集中在 `include/lcr_config.h`；实际 C 帧缓冲预览见 [界面预览](../docs/rewrite/oled_preview.png)。

## 激励源：内部 DAC（AD9833 已失效移除）

- 原 AD9833 的 MCLK 通路实测未到芯片、VOUT 恒 0 V（CAP=2.49 V 正常），芯片已从板上移除；**PA5（DAC 通道 2）接到原 AD9833 VOUT 节点（C1 靠芯片侧）**，由 STM32 内部 DAC1 通道 2 产生激励；PA4 在本核心板上对地 0 Ω（兼作板载 SPI Flash 片选网络）不可用，固件保持其为输入、不驱动。
- 激励实现：56 点正弦表（2048±372，约 0.6 Vpp）经 **TIM6 更新事件 + DMA1 Stream6 通道 7 循环** 送到 DAC1；更新率 = 56×f0，TIM6 分频 = 84 MHz/(56·f0) = 1500000/f0，因此 100 Hz / 1 kHz / 10 kHz 三档频率与 ADC 采样时钟（fs = 16·f0）**精确相干**。
- 静音时 TIM6 停止、DAC 输出中值 1.65 V（经 C1 隔直后对 AFE 无影响）；开始测量时启动，测量结束恢复静音。静音期间修改频率只更新 TIM6 ARR，下一次 begin 才启动波形。
- **100 kHz 需要 5.6 MSPS 更新率，超出 DAC 可用范围**：寄存器仍按 100 kHz 配置，但输出会明显衰减/失真，当前版本只承诺 100 Hz / 1 kHz / 10 kHz。
- 顶层仍调用 `lcr_board_dds_frequency()` / `lcr_board_dds_mute()`，测量、校准与拟合逻辑完全未改。

### 激励后端选择（DAC / AD9833）

- 默认 `LCR_EXCITATION_DAC=1`：内部 DAC1 通道 2(PA5) + TIM6 + DMA，构建并烧录 `lcr-next.hex`。
- 若恢复外部 AD9833：FSYNC=PA4、SCLK=PA5、MOSI=PA7、MCLK=PA8（MCO1 8 MHz）；此时 PA4 必须从 C1 断开。
  执行 `tools\rewrite\build_firmware.ps1 -Ad9833` 构建 `lcr-ad9833.hex` 并烧录该镜像。
- 两种后端共用 `lcr_board_dds_frequency()` / `lcr_board_dds_mute()`，测量与校准代码不变。

## 无激励保护

- 每帧测量在拟合后先检查 V1 基波峰值；低于 `LCR_EXCITATION_MIN_PEAK_CODES`（默认 25 码，约 20 mV 峰值）时
  判为“无激励”，进入 `LCR STATUS` 提示页（`NO EXCITATION` / `CHECK TP1 / DRIVE`），**不显示测量数值**。
- 阈值在 `include/lcr_config.h`；改变激励电平或前端增益后需重新评估该阈值。

## 模块

| 模块 | 作用 |
|---|---|
| `lcr_core` | 正弦拟合、复数阻抗、反馈/夹具模型 |
| `lcr_frontend` | 静音、先断后合、机械等待、恢复与取消 |
| `lcr_measure` | 完整帧、校准作用域、R/L/C 串并联参数、Q/D |
| `lcr_session` | 异步采集令牌、超时、自动量程、稳定性判断、连续状态复用 |
| `lcr_app` | 自动选频、单键菜单、连续测量、三类校准工作副本与保存 |
| `lcr_ui` | 单键消抖/长按/双击、显示格式、帧缓冲与 180°可选映射 |
| `lcr_cal_store` | 24 槽校准记录、显式编码、版本/身份/CRC 检查 |
| `lcr_journal` | 双副本、非活动副本写入、读回、最后提交和恢复 |
| `platform/` | 独立启动、主循环、时钟/串口/DAC 激励、ADC/DMA、I²C/OLED、Flash |

普通测量默认在最小等待后比较两路直流、幅度和复数阻抗，连续两次稳定比较通过才发布；超过 1200 ms 仍不稳定则拒绝。原有 800 ms 固定等待可通过配置恢复，校准仍使用该保守等待。稳定度不证明模拟电路线性，首次多频搜索和换档过程也不保证 ≤1 s。

目前 double 数学运算尚未测得 MCU 耗时；最终优化必须保留参考模型误差检查。自动选频评分、稳定门限和信号质量阈值均需上板调整。

## 复现

在项目根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File tools/rewrite/check_host.ps1
powershell -ExecutionPolicy Bypass -File tools/rewrite/check_core.ps1
powershell -ExecutionPolicy Bypass -File tools/rewrite/build_firmware.ps1
# 外部 AD9833 后端（恢复芯片时）：
powershell -ExecutionPolicy Bypass -File tools/rewrite/build_firmware.ps1 -Ad9833
```

`check_host.ps1` 使用本机 MSVC/Windows SDK 编译并执行 9 个主机测试，可通过 `-VsRoot` 和 `-WindowsSdkRoot` 覆盖路径，测试断言保持开启。`check_core.ps1` 使用 ARMCLANG 6.7 和 Keil CMSIS 5.0.1 核心头做逐源目标编译（未修改厂家文件）。`build_firmware.ps1` 默认使用 `D:\Tools\embedded\arm-gcc-14.2\bin`，可用 `-Toolchain` 指定其他 GNU 工具链 bin 目录；产物在 `firmware_next/build-target/`，与旧工程的 `firmware/build/` 分开。

本目录 CMake 仍只构建电脑端逻辑测试；本机 CMake/Ninja 的 MSVC 探测存在先前记录的 C1902/路径编码问题，不能称 CMake/CTest 已通过。

## TP1 波形自检（模拟前端 bring-up）

用于在示波器上确认激励链（DAC1/PA5 → C1 → OPA2810 → BUF634A → TP1）是否工作。该镜像不启动测量、不吸合任何量程继电器，只保持 1 kHz 连续正弦，同时每 250 ms 采一帧把 V1/V2 幅度/直流/残差写到 `lcr_wave_diag_data`（SWD 可读）：

```powershell
powershell -ExecutionPolicy Bypass -File tools/rewrite/build_firmware.ps1 -WaveTest
python -m pyocd flash -t stm32f407vgtx --erase sector firmware_next/build-target/lcr-wave.hex
```

预期：TP1 上 1 kHz 正弦、约 0.5 Vrms（幅度由 VR1 决定），直流偏置 3.3 V，示波器用 AC 耦合、200 µs/div、10× 探头与通道档位一致。PC13 LED 1 Hz 闪烁；UART 打印 `LCR WAVE TEST: TP1 1 kHz, relays released; ADC diag on`。若 TP1 无波形，先确认 PA5 确已接到 C1 靠芯片侧、AFE 的 +9 V/3.3 VA/VDDA/VREF_AFE 正常，再查 OPA2810/BUF634A 工作点。ADC 侧 V1/V2 必须按 `V1→PC0、V2→PA1、GND 共地` 接线后，`lcr_wave_diag_data` 才会给出有效读数（否则只看到偏置与噪声，status=4/LCR_WEAK_SIGNAL）。

检查完模拟前端后，重新构建普通镜像（不带 `-WaveTest`）并烧回 `lcr-next.hex`。该镜像已在 ST-Link 上验证：`SystemCoreClock=168 MHz`、静音时 `TIM6 ARR=1499/CEN=0`、`DAC CR=0x1005/DOR=2048`（中值）、`dds_muted=1`；测量期间 `TIM6 CEN=1`、DMA1 Stream5 `EN=1` 且 `NDTR` 循环、DAC `DOR` 在正弦表范围内变化、`dds_muted=0`；无 HardFault。

## 验证与限制

- 9 个主机测试：core、frontend、measure、session、cal_store、journal、ui、app、settling。
- 23 个目标编译文件：8 个逻辑模块、6 个平台文件、9 个测试入口，严格告警无错误。
- 覆盖原有 32 组 R/L/C 帧、32 组自动量程案例，以及整机单键、R/L/C 自动选频、三类校准、稳定判断和保存流程。
- 校准记录覆盖 1772 个字节位置的损坏与截断；双副本覆盖 445 个中断写入边界、损坏回退、序号回绕和读取失败。
- OLED 正向与 180°映射（8192 像素）均通过检查，界面预览取自 C 帧缓冲。
- 板级启动、时钟、单键、量程继电器、ADC/DMA 采集与 DAC 激励已在 ST-Link + 实物开发板上执行；I²C/OLED、Flash 保存与测量精度仍缺实物证据。完整链接、代码尺寸和最终内存预算已可复现，但尚未做长期稳定性测试。
- 激励 100 kHz 仅做了寄存器配置，DAC 更新率不足（5.6 MSPS），当前不作为可用档位。
- 无激励保护：主机测试覆盖平信号被拒（`LCR_NO_EXCITATION`）；实物上断开激励时应显示 `NO EXCITATION` 提示页。
- 物理短路校准保持禁用；开路补偿默认假定 Zs=0，不能称开路+短路校准已完成。
- 标准件校准用屏幕指定的标称电阻值；其误差、反馈模型适用性、噪声及模拟工作点需要实测。
- Flash 测试使用 NOR 模型，不能代替真实掉电、供电电压和寿命测试。保存只允许在停采集、激励软件状态静音后执行。

没有烧录或操作实物，实测精度和单次测量时间尚未验收。

- 抗噪协议（2026-11-25）：1 kHz / 10 kHz 帧长提升到 **1024 对**（100 Hz 128、100 kHz 1200）；`quality()` 判 WEAK 的帧不再被丢弃（`valid=1`，`status=LCR_WEAK_SIGNAL` 原样保留给上层标低置信）；settling 只看 **V1 直流与幅度**，V2 的帧间噪声交给复比值平均。
- 实测（2 kΩ 薄膜电阻、1 kHz、1 kΩ 档、未校准）：`status=OK`、`valid=1`、`|Z|=1996.93 Ω`、`φ=+0.535°`、`kind=R`；单帧 |Z| 标准差 2.70%（256 点）→ 1.28%（1024 点），8 帧平均约 0.42%。
- 注意：V2（PA1）节点接触/布线仍偏噪（残差约 130 码，V1 约 21 码），已定位为模拟节点问题；建议重焊 PA1 飞线两端，或把 V2 挪到 PA2（固件 `ADC2->SQR3=2` + PA2 模拟）。
