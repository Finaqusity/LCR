#ifndef LCR_DSP_H
#define LCR_DSP_H

/* 相干采样 + S 相折叠单 bin DFT（§5.2–§5.7）。
 * fs = S·f0、N = M·S ⇒ 基波位于 DFT 的第 M 号 bin，旋转因子 e^{-j2πn/S} 只依赖
 * n mod S ⇒ 先把 N 点按相位折叠成 S 个整数累加器（O(N) 纯加法），再做 S 点复 DFT，
 * 一次得到基波与 h=2..S−1 全部谐波。矩形窗，不加窗、无泄漏。
 * 归一化唯一口径：峰值 = 2|X_h|/N，rms = √2|X_h|/N，相位 = atan2(Im,Re)。 */

#include <stdint.h>
#include "adc.h"

typedef struct { float re, im; } cfloat_t;

typedef struct {
    cfloat_t h[ADC_S_MAX];   /* h[1]=基波（峰值口径的复系数），h[0]  unused */
    float    dc;             /* 均值（码值） */
    float    peak;           /* |h[1]| 折算的峰值（码值）= 2|X_1|/N */
} chan_spectrum_t;

/* quality 位 */
#define DSP_Q_DC_WARN    0x01U   /* DC 偏离 2048±80 */
#define DSP_Q_CLIP       0x02U   /* 出现削顶码 */
#define DSP_Q_H2_WARN    0x04U   /* 两通道二次谐波之差 >0.5% */
#define DSP_Q_SPREAD     0x08U   /* 块间离散度超限 */
#define DSP_Q_COHERENCE  0x10U   /* |M_act − M| > 0.02 bin */
#define DSP_Q_SIG_LOW    0x20U   /* Vo 幅度过低 */

typedef struct {
    chan_spectrum_t vin, vo;
    float h2_ratio, h3_ratio;    /* |h2|/|h1|、|h3|/|h1|（h3 仅 S=16 有效） */
    uint16_t sat_count;
    uint8_t quality;
} dsp_result_t;

void dsp_analyze(const adc_block_t *blk, dsp_result_t *out);
void dsp_block_stats(const adc_block_t *blk, uint16_t *min_v, uint16_t *max_v);
/* 谱平均（供显示/自检）+ 块间离散度。spread 取幅度散差与相位散差的较大者（%）。 */
int  dsp_combine(const dsp_result_t *blks, uint16_t k, dsp_result_t *avg, float *spread);

/* ---- 以下为新增接口（协作说明 §7 允许新增，不改已有签名/语义）---- */

/* 单块复比值 m1/m2 = Vin/Vo（峰值口径，归一化在比值里抵消）。 */
cfloat_t dsp_ratio(const dsp_result_t *r);
/* §5.5 定稿口径：先算每块复比值 R_b，再平均 R̄=(1/K)ΣR_b。
 * spread_lin = std(|R_b|)/|R̄|（相对），spread_ph = max|arg R_b − arg R̄|（度）。
 * 返回 R̄。k=0 或 Vo 为 0 时返回 0 并置 spread 为极大值。 */
cfloat_t dsp_ratio_mean(const dsp_result_t *blks, uint16_t k,
                        float *spread_lin, float *spread_ph);
/* 同上，但输入是已算好的每块复比值数组（省 RAM，工作区 ≤4 KB）。 */
cfloat_t dsp_ratio_mean_arr(const cfloat_t *ratios, uint16_t k,
                            float *spread_lin, float *spread_ph);
/* 相干性检查：M_act = N·f_dds/fs，返回 |M_act − M|（bin）。 */
float dsp_coherence_err(uint16_t n, uint16_t s, uint32_t fs_hz, uint32_t f_dds_mhz);

/* ---- 复数工具（测量链路 float；校准求解另用 double，见 cal.c）---- */
static inline cfloat_t cf_make(float re, float im) { cfloat_t r = { re, im }; return r; }
static inline cfloat_t cf_add(cfloat_t a, cfloat_t b) { return cf_make(a.re + b.re, a.im + b.im); }
static inline cfloat_t cf_sub(cfloat_t a, cfloat_t b) { return cf_make(a.re - b.re, a.im - b.im); }
static inline cfloat_t cf_scale(cfloat_t a, float k) { return cf_make(a.re * k, a.im * k); }
static inline cfloat_t cf_mul(cfloat_t a, cfloat_t b)
{
    return cf_make(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
}
static inline float cf_mag2(cfloat_t a) { return a.re * a.re + a.im * a.im; }
float cf_mag(cfloat_t a);
float cf_arg_deg(cfloat_t a);          /* 归一化到 (−180°, 180°] */
cfloat_t cf_div(cfloat_t a, cfloat_t b);
cfloat_t cf_neg(cfloat_t a);

#endif /* LCR_DSP_H */