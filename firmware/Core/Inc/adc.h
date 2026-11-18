#ifndef LCR_ADC_H
#define LCR_ADC_H

/* ADC1+ADC2 双重规则同步 + TIM2_TRGO + DMA2_Stream0 循环双缓冲（§4.4、§5.1）。
 * 定稿约束（D1–D5）：
 *   ADC 时钟 21 MHz（PCLK2/4），12bit+3cyc=15 ADCCLK=714 ns → ≤1.4 MSPS；
 *   触发源 TIM2_TRGO，EXTSEL=0b0110（TIM2 是 32 位，TIM8 装不下 ARR=52499）；
 *   100 kHz 用 S=12/fs=1.2 MSPS/N=1008（S=16 需 1.6 MSPS 超限，S=8 有折回污染）；
 *   F4 的 ADC 没有软件自校准位，只需 ADON=1 后等 tSTAB。
 * CDR 低 16 位 = ADC1(Vin)，高 16 位 = ADC2(Vo)。
 *
 * 勘误（待 Lead 确认后同步架构文档）：§4.4/§13.2 写的 DMA `DIR=01` 是
 * memory-to-peripheral；ADC→SRAM 必须是 `DIR=00`（DMA_PERIPH_TO_MEMORY，
 * 见 stm32f4xx_hal_dma.h:227-229）。本实现按 00 配置。 */

#include <stdint.h>
#include "board_config.h"

#define ADC_N_MAX 1024
#define ADC_S_MAX 16

/* adc_acquire / adc_acquire_avg 返回码 */
#define ADC_OK           0
#define ADC_ERR_CFG     (-1)   /* 配置非法（n/s/fs 越界或不整除） */
#define ADC_ERR_TIMEOUT (-2)   /* 等块超时 / 连续 3 次 OVR */
#define ADC_ERR_OVR     (-3)   /* 本块 ADC 溢出，已作废 */
#define ADC_ERR_AWD     (-4)   /* 模拟看门狗中止（削顶/短路保护） */
#define ADC_ERR_CLIP    (-5)   /* 时域削顶码（≤3 或 ≥4092），本块作废 */

typedef struct {
    uint32_t f0_hz;   /* 标称频率 */
    uint16_t s;       /* 每周期采样点数 */
    uint16_t n;       /* 块长（必须是 s 的整数倍） */
    uint32_t fs_hz;   /* 实际采样率 = 84e6/(ARR+1) */
} acq_cfg_t;

typedef struct {
    uint32_t cdr[ADC_N_MAX];
    uint16_t n, s;
    uint32_t f0_hz, fs_hz, ts_ms;
    uint16_t sat_count, dma_err;
} adc_block_t;

int  adc_init(void);
int  adc_acquire(const acq_cfg_t *cfg, adc_block_t *blk);
/* 连续采 blocks 块，dst 取最后一块的原始数据，sat_count/dma_err 为累计值。
 * 注意：块间平均必须按 §5.5 走"每块复比值再平均"（dsp_combine），
 * 不能对原始 CDR 平均——f_dds 与 fs 不是严格整数比，块间相位会漂移。
 * 本函数供自检/噪声统计用。 */
int  adc_acquire_avg(const acq_cfg_t *cfg, uint16_t blocks, adc_block_t *dst);
void adc_stop(void);

/* 10 Ω 档才开模拟看门狗（ADC1/ADC2 各监视自己的规则通道）；
 * 其它档 HTR/LTR 覆盖全量程，开着无意义且可能误报（R18）。 */
void     adc_awd_enable(int on);
int      adc_awd_tripped(void);
uint32_t adc_ovr_count(void);
void     adc_clear_errors(void);

/* 由频点表生成采集配置；n=0 表示用该频点的精测块长。 */
int      adc_make_cfg(uint32_t band, uint16_t n, acq_cfg_t *cfg);
uint32_t adc_fs_from_arr(uint32_t arr);

#endif /* LCR_ADC_H */