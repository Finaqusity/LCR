# 新固件独立工程

当前源码已接通复位启动、板级初始化、双 ADC/TIM/DMA、测量与校准调度、两键/OLED 和 Flash 双副本保存。没有读取或修改旧应用代码。

**当前没有已验证的可烧录镜像。** 本机 GNU 编译器和 ARM 链接器仍在启动阶段报 WinError 623，完整构建未通过。电脑端 9 个 C 测试程序已运行通过，23 个源文件已通过 Cortex-M4 / hard-float 目标编译；这些证据不等同于 MCU 执行或整机精度验收。

操作、构建、测试和剩余验收见 [整机固件操作与验收](../docs/rewrite/整机固件操作与验收.md)。硬件依据见 [硬件核对与实施记录](../docs/rewrite/硬件核对与实施记录.md)。

## 两键和 OLED

- 丝印 PB1（MCU PB1）：短按切换串联/并联模型；长按切换详情。
- 丝印 PB2（MCU PB0）：短按启动/停止连续测量；长按进入设置/返回。
- 菜单中 PB1 下一项、PB2 确认。校准有单独确认页，取消/失败保留原校准。
- OLED 默认 SSD1306 128×64 I²C，0x3C/0x3D，PB6/PB7。所有页面在发送时统一旋转 180°。
- 串并联切换使用同一份 Z，不重新采样或切换继电器。停止后显示 HOLD。
- 配置集中在 `include/lcr_config.h`；实际 C 帧缓冲预览见 [界面预览](../docs/rewrite/oled_preview.png)。

## 模块

| 模块 | 作用 |
|---|---|
| `lcr_core` | 正弦拟合、复数阻抗、反馈/夹具模型 |
| `lcr_frontend` | 静音、先断后合、机械等待、恢复与取消 |
| `lcr_measure` | 完整帧、校准作用域、R/L/C 串并联参数、Q/D |
| `lcr_session` | 异步采集令牌、超时、自动量程、稳定性判断、连续状态复用 |
| `lcr_app` | 自动选频、两键菜单、连续测量、三类校准工作副本与保存 |
| `lcr_ui` | 消抖、显示格式、帧缓冲和 180°映射 |
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

前两条已在本机通过。第三条已验证失败分支：工具启动失败时退出码 1，不生成新镜像；成功链接分支尚未验证。可通过 `-Toolchain` 指定可用 GNU 工具链的 bin 目录。预期产物在 `firmware_next/build-target/`，与旧工程的 `firmware/build/` 分开。

`check_host.ps1` 使用本机 MSVC/Windows SDK，可通过 `-VsRoot` 和 `-WindowsSdkRoot` 覆盖路径，测试断言保持开启。`check_core.ps1` 使用 ARMCLANG 6.7 和 Keil CMSIS 5.0.1 核心头，配合仓库厂家 STM32F407 设备头。没有修改厂家文件或伪造编译器版本。

本目录 CMake 仍只构建电脑端逻辑测试；本机 CMake/Ninja 的 MSVC 探测存在先前记录的 C1902/路径编码问题，不能称 CMake/CTest 已通过。

## 验证与限制

- 9 个主机测试：core、frontend、measure、session、cal_store、journal、ui、app、settling。
- 23 个目标编译文件：8 个逻辑模块、6 个平台文件、9 个测试入口，严格告警无错误。
- 覆盖原有 32 组 R/L/C 帧、32 组自动量程案例，以及整机两键、R/L/C 自动选频、三类校准、稳定判断和保存流程。
- 校准记录覆盖 1772 个字节位置的损坏与截断；双副本覆盖 445 个中断写入边界、损坏回退、序号回绕和读取失败。
- OLED 全部 8192 像素的 180°映射通过检查，界面预览取自 C 帧缓冲。
- ADC/I²C/Flash/启动代码只有编译证据，未在真实寄存器或 MCU 上执行。完整链接、代码尺寸和最终内存预算仍需验证。
- 物理短路校准保持禁用；开路补偿默认假定 Zs=0，不能称开路+短路校准已完成。
- 标准件校准用屏幕指定的标称电阻值；其误差、反馈模型适用性、噪声及模拟工作点需要实测。
- Flash 测试使用 NOR 模型，不能代替真实掉电、供电电压和寿命测试。保存只允许在停采集、激励软件状态静音后执行。

没有烧录或操作实物，实测精度和单次测量时间尚未验收。
