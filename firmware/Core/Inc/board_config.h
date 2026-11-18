#ifndef LCR_BOARD_CONFIG_H
#define LCR_BOARD_CONFIG_H

/* ---------------------------------------------------------------------------
 * 全部硬件常量与决策表的唯一出处（架构文档 §3.1 / §8.4 / 协作说明 §8.6）。
 * 本文件刻意不依赖 CMSIS：GPIO 端口用索引表示，由 board.c 映射成 GPIO_TypeDef*，
 * 这样 dsp/cal/meas/results 可以在 PC 上直接编译跑金标准向量。
 * 依据：docs/软件架构.md §4–§9、§13.2、§14.1（D1–D30）。
 * ------------------------------------------------------------------------- */

#include <stdint.h>

/* ---- 版本 ---------------------------------------------------------------- */
#define CFG_FW_VERSION      "v0.2.0"
#define CFG_CAL_BLOB_VERSION 2U
#define CFG_PROTO_VERSION    1U

/* ---- 时钟树（§4.1、§13.2）---------------------------------------------- */
#define CFG_HSE_HZ          8000000U
#define CFG_HSI_HZ          16000000U
#define CFG_PLL_M           8U
#define CFG_PLL_N           336U
#define CFG_PLL_P           2U      /* RCC_PLLCFGR.PLLP 编码 0b00 = /2 */
#define CFG_PLL_Q           7U
#define CFG_SYSCLK_HZ       168000000U
#define CFG_HCLK_HZ         168000000U
#define CFG_PCLK1_HZ        42000000U
#define CFG_TIMCLK1_HZ      84000000U   /* APB1 定时器 ×2 */
#define CFG_PCLK2_HZ        84000000U
#define CFG_TIMCLK2_HZ      168000000U
#define CFG_ADCCLK_HZ       21000000U   /* PCLK2/4；D1：28 MHz 不存在 */
#define CFG_FLASH_WS        5U
#define CFG_HSE_TIMEOUT_MS  100U
#define CFG_SYSTICK_HZ      1000U

/* ---- GPIO 端口索引（board.c 映射）--------------------------------------- */
#define CFG_PORT_A          0U
#define CFG_PORT_B          1U
#define CFG_PORT_C          2U
#define CFG_PORT_D          3U
#define CFG_PORT_E          4U

/* ---- 引脚（§4.1–§4.6）--------------------------------------------------- */
#define CFG_LED_PORT        CFG_PORT_C
#define CFG_LED_PIN         13U     /* PC13，低电平点亮（不是 PD12） */
#define CFG_LED_ACTIVE_LOW  1U

#define CFG_UART_TX_PORT    CFG_PORT_A
#define CFG_UART_TX_PIN     9U
#define CFG_UART_RX_PORT    CFG_PORT_A
#define CFG_UART_RX_PIN     10U
#define CFG_UART_AF         7U
#define CFG_UART_BAUD       115200U

#define CFG_SPI_SCK_PORT    CFG_PORT_A
#define CFG_SPI_SCK_PIN     5U
#define CFG_SPI_MOSI_PORT   CFG_PORT_A
#define CFG_SPI_MOSI_PIN    7U
#define CFG_SPI_AF          5U
#define CFG_DDS_FSYNC_PORT  CFG_PORT_A
#define CFG_DDS_FSYNC_PIN   4U
/* SPI 模式：架构 §4.2/§13.2 定稿用模式 1（CPOL=0/CPHA=1），R7 标注待上板核对；
 * AD9833 在 SCLK 下降沿采样，模式 2（CPOL=1/CPHA=0）同样满足。
 * TODO(确认项3/R7)：示波器看 SCLK/SDATA/FSYNC 对齐后裁决，改这里即可。 */
#define CFG_DDS_SPI_MODE    1U
#define CFG_DDS_SPI_BR      1U      /* PCLK2/4 = 21 MHz */
#define CFG_DDS_POWERUP_MS  20U     /* AD9833 上电到可写 ≥20 ms */
#define CFG_MCO_PORT        CFG_PORT_A
#define CFG_MCO_PIN         8U      /* MCO1 = HSE /1 → AD9833 MCLK */
#define CFG_MCO_AF          0U

#define CFG_ADC_VIN_PORT    CFG_PORT_A
#define CFG_ADC_VIN_PIN     0U      /* ADC1_IN0 */
#define CFG_ADC_VO_PORT     CFG_PORT_A
#define CFG_ADC_VO_PIN      1U      /* ADC2_IN1 */

#define CFG_I2C_SCL_PORT    CFG_PORT_B
#define CFG_I2C_SCL_PIN     6U
#define CFG_I2C_SDA_PORT    CFG_PORT_B
#define CFG_I2C_SDA_PIN     7U
#define CFG_I2C_AF          4U

#define CFG_RELAY_PORT      CFG_PORT_D
#define CFG_RELAY_PIN_FIRST 0U      /* PD0…PD6 → ULN2803 IN1…IN7 */
#define CFG_RELAY_PIN_COUNT 7U
#define CFG_KEY_PORT        CFG_PORT_D
#define CFG_KEY_PIN         7U      /* 可选按键，默认不启用（R6） */
#define CFG_KEY_ENABLE      0U

/* ---- 频点 / 采样表（§5.1、§13.2；D2/D3/D4）------------------------------ */
#define CFG_FREQ_NUM        4U
#define CFG_BAND_100HZ      0U
#define CFG_BAND_1KHZ       1U
#define CFG_BAND_10KHZ      2U
#define CFG_BAND_100KHZ     3U

typedef struct {
    uint32_t f_nom;       /* 标称频率 Hz */
    uint32_t freqreg;     /* AD9833 28 位频率字（round(f·2^28/8e6)） */
    uint16_t s;           /* 每周期采样点数：16/16/16/12 */
    uint32_t arr;         /* TIM2 ARR，PSC=0 @84 MHz */
    uint16_t n_fine;      /* 精测块长 */
    uint16_t n_coarse;    /* 粗测块长（必须是 s 的整数倍） */
    uint16_t k_default;   /* 精测默认块数 */
} cfg_band_t;

/* fs = 84e6/(arr+1) = 1600 / 16000 / 160000 / 1200000 SPS
 * 100 kHz 用 S=12：S=16 需要 1.6 MSPS > ADC 上限 1.4 MSPS；
 * S=8 会把 7f/9f 折回基波（~0.86% 偏置），故取 12。 */
#define CFG_BANDS_INITIALIZER { \
    {   100U,    3355U, 16U, 52499U, 1024U, 64U,  1U }, \
    {  1000U,   33554U, 16U,  5249U, 1024U, 64U,  8U }, \
    { 10000U,  335544U, 16U,   524U, 1024U, 64U, 16U }, \
    {100000U, 3355443U, 12U,    69U, 1008U, 96U, 32U }  \
}

#define CFG_ADC_N_MAX       1024U
#define CFG_ADC_S_MAX       16U
#define CFG_ADC_FS_MAX      1400000U   /* 12bit+3cyc = 15 ADCCLK = 714 ns */
#define CFG_ADC_SAMPLE_CYC  3U
#define CFG_ADC_CONV_CYC    15U

/* ---- 量程（§4.3、§6.3、§6.4）------------------------------------------- */
#define CFG_RANGE_NUM       6U
#define CFG_RANGE_DEFAULT   3U      /* 10 kΩ：PD3 常闭，上电即在位 */
#define CFG_RANGE_CAL_PIN   6U      /* PD6 = CAL 开关（K78/K7，不吸合=正常） */

/* Rf 名义值（Ω）。实际绝对值由双负载校准的 k 吸收。 */
#define CFG_RF_NOM_INITIALIZER { 10.0f, 100.0f, 1000.0f, 10000.0f, 100000.0f, 1000000.0f }

/* 每档反馈并联电容 Cf（F）。D28：只有 100 kΩ/1 MΩ 档贴了 Cc（1–2.2 pF），
 * 10 Ω–10 kΩ 档无。取 2.2 pF 作名义值，实际由负载校准按频吸收。
 * TODO(确认项9)：各档 Cc 实际贴装值待 BOM 核对。 */
#define CFG_CF_NOM_INITIALIZER { 0.0f, 0.0f, 0.0f, 0.0f, 2.2e-12f, 2.2e-12f }

/* 量程-频率合法表 [range][band]：1=可用，2=仅校准后可用，0=禁止。
 * §6.3：100 kΩ@100 kHz 与 1 MΩ@10 kHz 只能校准后使用；1 MΩ@100 kHz 禁止（D11）。 */
#define CFG_RANGE_FREQ_LEGAL_INITIALIZER { \
    { 1, 1, 1, 1 }, \
    { 1, 1, 1, 1 }, \
    { 1, 1, 1, 1 }, \
    { 1, 1, 1, 1 }, \
    { 1, 1, 1, 2 }, \
    { 1, 1, 2, 0 }  \
}

/* ρ = Rf/|Zx| 的窗口（§6.4）。10 Ω 档受 BUF634A 低侧 headroom 限制取 1.4。 */
#define CFG_RHO_TARGET      0.3f
#define CFG_RHO_MIN         0.12f
#define CFG_RHO_GOOD_LO     0.2f
#define CFG_RHO_GOOD_HI     0.8f
#define CFG_RHO_MAX_INITIALIZER { 1.4f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f }
#define CFG_RANGE_RETRY_MAX 3U

/* 双负载校准的标准电阻对（Ω），§7.4。1 MΩ 档用 1 M/10 M。 */
#define CFG_LOAD_STD_INITIALIZER { \
    {     10.0f,     100.0f }, \
    {    100.0f,    1000.0f }, \
    {   1000.0f,   10000.0f }, \
    {  10000.0f,  100000.0f }, \
    { 100000.0f, 1000000.0f }, \
    {1000000.0f,10000000.0f }  \
}
#define CFG_LOAD_CHECK_STD  4990.0f   /* 检查件 4.99 kΩ，误差应 <0.1% */

/* ---- 继电器时序（§4.3）-------------------------------------------------- */
#define CFG_RELAY_MUTE_MS   5U      /* 静音后等待 */
#define CFG_RELAY_SETTLE_MS 10U     /* 触点吸合稳定 */
#define CFG_RELAY_POST_MS   5U      /* 断常闭档后等待 */
#define CFG_RELAY_UNMUTE_MS 1U      /* 解除静音到可采样 */
#define CFG_RELAY_MAX_COILS 2U      /* 互锁：同时最多 2 只（D14/R17） */

/* ---- ADC 电平 / 质量门限（§5.6、D6）------------------------------------- */
#define CFG_ADC_VREF        3.3f
#define CFG_ADC_FULLSCALE   4096U
#define CFG_ADC_LSB_V       (3.3f / 4096.0f)   /* 0.8057 mV */
#define CFG_VBIAS_V         1.65f
#define CFG_ADC_DC_NOM      2048U
#define CFG_ADC_DC_TOL      80U
#define CFG_ADC_CLIP_LO     3U
#define CFG_ADC_CLIP_HI     4092U
#define CFG_ADC_SAT_MAX     0U      /* 出现任一削顶码即作废该块 */
#define CFG_ADC_NOISE_MAX   1.5f    /* 自检 σ 上限（LSB） */
#define CFG_VIN_RMS_MIN     0.3f    /* 自检 DDS 幅度窗口（Vrms） */
#define CFG_VIN_RMS_MAX     0.7f
#define CFG_VO_RMS_MIN      0.001f  /* 开路数据有效下限 1 mVrms */
#define CFG_H2_WARN         0.005f  /* Vin 二次谐波 >0.5% 告警 */
#define CFG_SPREAD_WARN     0.005f
#define CFG_SPREAD_FAIL     0.02f
#define CFG_SPREAD_EARLY    0.001f  /* 提前停止：幅度散差 */
#define CFG_SPREAD_PH_EARLY 0.02f   /* 提前停止：相位散差（度） */
#define CFG_COHERENCE_TOL   0.02f   /* |M_act − M| 上限（bin） */

/* 10 Ω 档模拟看门狗阈值（§4.4、R18）。0.9×最小可用摆幅，待 M1 实测标定。
 * TODO(确认项/R18)：阈值需上板用已知负载标定。 */
#define CFG_AWD_HTR         3538U
#define CFG_AWD_LTR         558U
#define CFG_AWD_RANGE       0U      /* 只在 10 Ω 档启用 */

/* ---- 类型判别 / 参数（§6.5、§6.6）--------------------------------------- */
#define CFG_TYPE_R_DEG      5.7f    /* |φ| < 5.7° → R（D9：不是 10°） */
#define CFG_TYPE_CL_DEG     84.0f   /* φ < −84° → C；φ > +84° → L */
#define CFG_SLOPE_C_MAX     0.2f    /* |Z|(10k)/|Z|(1k) < 0.2 → C */
#define CFG_SLOPE_L_MIN     5.0f    /* > 5 → L */
#define CFG_SLOPE_R_LO      0.5f
#define CFG_SLOPE_R_HI      2.0f

/* ---- 校准（§7）---------------------------------------------------------- */
#define CFG_ALPHA_GAIN_TOL  0.02f   /* |α−1| < 2% */
#define CFG_ALPHA_PH_TOL    1.0f    /* |arg α| < 1° */
#define CFG_OPEN_RHO_LO     1.0f    /* |Zm_open|/Rf ∈ [1,100] */
#define CFG_OPEN_RHO_HI     100.0f
#define CFG_CAL_STALE_PF    0.2f    /* 夹具杂散变化 >0.2 pF → cal_stale */
#define CFG_LOAD_COND_EPS   1e-3    /* |W1−W2| 相对下限 */
#define CFG_LOAD_ZP_NEAR    0.1f    /* |Zp−X| > 0.1|Zp| 才可反解 */
#define CFG_LOAD_ZP_INF     100.0f  /* |Zp| > 100·max → 视为 ∞ */
#define CFG_CAL_REPEAT      3U
#define CFG_CAL_SETTLE_MS   20U     /* 解除静音到 CAL 切换前 */
#define CFG_CAL_BLOCKS      2U      /* α 每频点块数 */

/* ---- 时间预算（§9.2、§9.3）---------------------------------------------- */
#define CFG_MEAS_TIMEOUT_MS 950U
#define CFG_BOOT_MAX_MS     350U
#define CFG_AUTO_MEAS_MS    1000U   /* 上电自动测量后的重复周期，0=只测一次 */
#define CFG_UI_REFRESH_MS   250U
#define CFG_LED_BLINK_MS    500U
#define CFG_IWDG_TIMEOUT_MS 2000U

/* ---- Flash 分区（§7.6、§8.2；D17）--------------------------------------- */
#define CFG_FLASH_BASE      0x08000000U
#define CFG_CODE_LIMIT      0x08040000U   /* 代码区上限 256 KB */
#define CFG_CAL_A_ADDR      0x08040000U   /* sector6，128 KB */
#define CFG_CAL_B_ADDR      0x08060000U   /* sector7，128 KB */
#define CFG_CAL_A_SNB       6U
#define CFG_CAL_B_SNB       7U
#define CFG_CAL_SECTOR_SIZE 0x20000U
#define CFG_CAL_SLOT_SIZE   1536U
#define CFG_CAL_SLOTS       85U           /* 128 KB / 1536 B = 85 */
#define CFG_CAL_BLOB_SIZE   1240U
#define CFG_CAL_MAGIC       0x4C435232U   /* "LCR2" */

/* ---- 内存（§8.1、§8.3）-------------------------------------------------- */
#define CFG_SRAM_BASE       0x20000000U
#define CFG_SRAM_SIZE       0x20000U
#define CFG_STACK_SIZE      0x1000U
#define CFG_STACK_LIMIT     0x2001F000U
#define CFG_STACK_MAGIC     0xDEADBEEFU
#define CFG_STACK_NEAR_B    1024U         /* 余量 <1 KB → STACK_NEAR */
#define CFG_CCM_BASE        0x10000000U
#define CFG_CCM_SIZE        0x10000U

/* ---- 串口 / 显示（§4.5、§4.6、§8.5）------------------------------------- */
#define CFG_UART_RX_SIZE    256U
#define CFG_UART_TX_SIZE    512U
#define CFG_UART_LINE_MAX   96U
#define CFG_LOG_SIZE        1024U
#define CFG_OLED_WIDTH      128U
#define CFG_OLED_HEIGHT     64U
#define CFG_OLED_FB_SIZE    1024U
#define CFG_OLED_ADDR_LO    0x3CU
#define CFG_OLED_ADDR_HI    0x3DU
#define CFG_I2C_TIMEOUT_MS  10U
#define CFG_RAW_SYNC0       0xA5U
#define CFG_RAW_SYNC1       0x5AU

/* ---- 未校准回退值（§7.6）------------------------------------------------ */
#define CFG_FALLBACK_ZS_RE  0.1f    /* 串联残余 0.1 Ω */
#define CFG_FALLBACK_ZS_IM  0.0f
#define CFG_FALLBACK_KP_RE  1.0f
#define CFG_FALLBACK_KP_IM  0.0f

#endif /* LCR_BOARD_CONFIG_H */