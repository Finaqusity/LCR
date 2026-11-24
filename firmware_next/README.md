# 新固件独立工程

当前源码已接通复位启动、板级初始化、双 ADC/TIM/DMA、测量与校准调度、单键/OLED 和 Flash 双副本保存。没有读取或修改旧应用代码。

本机已在 Windows + `arm-none-eabi-gcc 14.2`（`D:\Tools\embedded\arm-gcc-14.2\bin`，可用 `-Toolchain` 覆盖）下实际构建出 `build-target/lcr-next.elf/.hex/.bin/.map`；电脑端 9 个 C 测试程序已运行通过，23 个源文件通过 Cortex-M4 / hard-float / `-Wall -Wextra -Werror` 编译。**这些证据不等同于 MCU 执行或整机精度验收；尚未上板运行。**

操作、构建、测试和剩余验收见 [整机固件操作与验收](../docs/rewrite/整机固件操作与验收.md)。硬件依据见 [硬件核对与实施记录](../docs/rewrite/硬件核对与实施记录.md)。

## 开发板适配：单键 + OLED 正向

- 板载按键接 **WK_UP / PA0**，高有效（按下为高，固件开内部下拉）；旧板 PB1/PB2 两个按键不再使用。
- **V1 改接 PC0（ADC1_IN10）**，因为 PA0 已被按键占用；V2 仍为 PA1（ADC2_IN1）。接线时需把 AFE 的 V1 输出从 PA0 移到 PC0。
- OLED 正向摆放 0°（`LCR_OLED_ROTATE_180=0`），发送时不再做 180° 像素反转。

| 场景 | 短按（单击） | 长按（800 ms） | 双击（300 ms 内两次） |
|---|---|---|---|
| 测量主页面 | 启动/停止连续测量 | 进入设置菜单并停止测量 | 切换 SERIES/PARALLEL |
| 设置菜单 | 下一项 | 修改/确认当前项 | 返回测量主页面 |
| 校准确认页 | 取消 | 开始校准 | — |
| 校准进行中 | 取消并保留原校准 | — | — |
| 提示/报错页 | 回菜单 | 回测量主页面 | — |

短按要等双击窗口结束才上报，因此单击有约 0.3 s 判定延迟；长按结束后不再产生短按；上电已按住的键不产生动作。串并联切换使用同一份 Z，不重新采样或切换继电器。停止后显示 HOLD。配置集中在 `include/lcr_config.h`；实际 C 帧缓冲预览见 [界面预览](../docs/rewrite/oled_preview.png)。

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
| `platform/` | 独立启动、主循环、时钟/串口/DDS、ADC/DMA、I²C/OLED、Flash |

普通测量默认在最小等待后比较两路直流、幅度和复数阻抗，连续两次稳定比较通过才发布；超过 1200 ms 仍不稳定则拒绝。原有 800 ms 固定等待可通过配置恢复，校准仍使用该保守等待。稳定度不证明模拟电路线性，首次多频搜索和换档过程也不保证 ≤1 s。

目前 double 数学运算尚未测得 MCU 耗时；最终优化必须保留参考模型误差检查。自动选频评分、稳定门限和信号质量阈值均需上板调整。

## 复现

在项目根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File tools/rewrite/check_host.ps1
powershell -ExecutionPolicy Bypass -File tools/rewrite/check_core.ps1
powershell -ExecutionPolicy Bypass -File tools/rewrite/build_firmware.ps1
```

`check_host.ps1` 使用本机 MSVC/Windows SDK 编译并执行 9 个主机测试，可通过 `-VsRoot` 和 `-WindowsSdkRoot` 覆盖路径，测试断言保持开启。`check_core.ps1` 使用 ARMCLANG 6.7 和 Keil CMSIS 5.0.1 核心头做逐源目标编译（未修改厂家文件）。`build_firmware.ps1` 默认使用 `D:\Tools\embedded\arm-gcc-14.2\bin`，可用 `-Toolchain` 指定其他 GNU 工具链 bin 目录；产物在 `firmware_next/build-target/`，与旧工程的 `firmware/build/` 分开。

本目录 CMake 仍只构建电脑端逻辑测试；本机 CMake/Ninja 的 MSVC 探测存在先前记录的 C1902/路径编码问题，不能称 CMake/CTest 已通过。

## 验证与限制

- 9 个主机测试：core、frontend、measure、session、cal_store、journal、ui、app、settling。
- 23 个目标编译文件：8 个逻辑模块、6 个平台文件、9 个测试入口，严格告警无错误。
- 覆盖原有 32 组 R/L/C 帧、32 组自动量程案例，以及整机单键、R/L/C 自动选频、三类校准、稳定判断和保存流程。
- 校准记录覆盖 1772 个字节位置的损坏与截断；双副本覆盖 445 个中断写入边界、损坏回退、序号回绕和读取失败。
- OLED 正向与 180°映射（8192 像素）均通过检查，界面预览取自 C 帧缓冲。
- ADC/I²C/Flash/启动代码只有编译证据，未在真实寄存器或 MCU 上执行。完整链接、代码尺寸和最终内存预算仍需验证。
- 物理短路校准保持禁用；开路补偿默认假定 Zs=0，不能称开路+短路校准已完成。
- 标准件校准用屏幕指定的标称电阻值；其误差、反馈模型适用性、噪声及模拟工作点需要实测。
- Flash 测试使用 NOR 模型，不能代替真实掉电、供电电压和寿命测试。保存只允许在停采集、激励软件状态静音后执行。

没有烧录或操作实物，实测精度和单次测量时间尚未验收。
