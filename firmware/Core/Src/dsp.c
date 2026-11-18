/* S 相折叠单 bin DFT（§5.2–§5.7）。本文件不依赖 CMSIS，可在 PC 上跑金标准向量。
 *
 * 推导：N = M·S，基波在第 M 号 bin。
 *   X_{hM} = Σ_{n=0}^{N−1} x[n]·e^{−j2πhMn/N} = Σ_n x[n]·e^{−j2πhn/S}
 * 令 n = p + S·k（p=0..S−1, k=0..M−1），e^{−j2πh(p+Sk)/S} = e^{−j2πhp/S}·e^{−j2πhk}
 * 而 e^{−j2πhk} = 1 ⇒ 旋转因子只依赖 p。于是
 *   A_p = Σ_k x[p+Sk]（整数累加，O(N) 纯加法）
 *   X_h = Σ_p A_p·e^{−j2πhp/S}（S 点复 DFT，一次给出 h=1..S−1 全部谐波）
 * DC 在 h≥1 的 bin 上严格抵消（Σ_p e^{−j2πhp/S} = 0），无需加窗。
 * 归一化：峰值 = 2|X_h|/N，rms = √2|X_h|/N。 */

#include "dsp.h"
#include "board_config.h"

#include <math.h>

#define DSP_PI 3.14159265358979323846

/* ---- 复数工具 ------------------------------------------------------------ */
float cf_mag(cfloat_t a)
{
    return sqrtf(cf_mag2(a));
}

float cf_arg_deg(cfloat_t a)
{
    if ((a.re == 0.0f) && (a.im == 0.0f)) {
        return 0.0f;
    }
    float d = atan2f(a.im, a.re) * (180.0f / (float)DSP_PI);
    if (d <= -180.0f) {
        d += 360.0f;                 /* 归一化到 (−180°, 180°] */
    }
    return d;
}

cfloat_t cf_div(cfloat_t a, cfloat_t b)
{
    const float d = cf_mag2(b);
    if (d == 0.0f) {
        return cf_make(0.0f, 0.0f);
    }
    return cf_make((a.re * b.re + a.im * b.im) / d,
                   (a.im * b.re - a.re * b.im) / d);
}

cfloat_t cf_neg(cfloat_t a)
{
    return cf_make(-a.re, -a.im);
}

/* ---- 相位表（按 S 缓存，S=16/12 各算一次）-------------------------------
 * 放 .ccmram（§8.3，NOLOAD，运行期算）省 SRAM 总线带宽；host 编译时退回普通
 * .bss（Mach-O 不支持任意段名），保证 dsp.c 可在 PC 上跑金标准向量。 */
#if defined(__arm__)
#define DSP_CCM __attribute__((section(".ccmram"), used))
#else
#define DSP_CCM
#endif
static float    dsp_cos[ADC_S_MAX] DSP_CCM;
static float    dsp_sin[ADC_S_MAX] DSP_CCM;
static uint16_t dsp_tbl_s = 0U;

static void dsp_table(uint16_t s)
{
    if ((dsp_tbl_s == s) || (s == 0U) || (s > ADC_S_MAX)) {
        return;
    }
    for (uint16_t p = 0U; p < s; ++p) {
        const double ang = 2.0 * DSP_PI * (double)p / (double)s;
        dsp_cos[p] = (float)cos(ang);
        dsp_sin[p] = (float)sin(ang);
    }
    dsp_tbl_s = s;
}

/* ---- 单通道折叠 + S 点 DFT ----------------------------------------------- */
static void dsp_chan(const uint32_t *cdr, uint16_t n, uint16_t s,
                     uint32_t shift, chan_spectrum_t *out)
{
    int32_t a[ADC_S_MAX];
    for (uint16_t p = 0U; p < s; ++p) {
        a[p] = 0;
    }

    uint32_t sum = 0U;
    uint16_t p = 0U;
    for (uint16_t i = 0U; i < n; ++i) {
        const uint32_t v = (cdr[i] >> shift) & 0xFFFFU;
        sum += v;
        a[p] += (int32_t)v;          /* 最大 84×4095 = 343980，int32 足够 */
        ++p;
        if (p == s) {
            p = 0U;
        }
    }
    out->dc = (float)sum / (float)n;

    out->h[0] = cf_make(0.0f, 0.0f);
    for (uint16_t h = 1U; h < s; ++h) {
        float re = 0.0f;
        float im = 0.0f;
        uint16_t idx = 0U;
        for (uint16_t q = 0U; q < s; ++q) {
            const float av = (float)a[q];
            re += av * dsp_cos[idx];
            im -= av * dsp_sin[idx];
            idx = (uint16_t)(idx + h);
            if (idx >= s) {
                idx = (uint16_t)(idx - s);
            }
        }
        out->h[h] = cf_make(re, im);
    }
    for (uint16_t h = s; h < ADC_S_MAX; ++h) {
        out->h[h] = cf_make(0.0f, 0.0f);
    }
    out->peak = 2.0f * cf_mag(out->h[1]) / (float)n;
}

void dsp_analyze(const adc_block_t *blk, dsp_result_t *out)
{
    if ((blk == 0) || (out == 0) || (blk->n == 0U)) {
        return;
    }
    const uint16_t s = blk->s;
    const uint16_t n = blk->n;
    dsp_table(s);

    dsp_chan(blk->cdr, n, s, 0U, &out->vin);    /* CDR 低 16 位 = ADC1 = Vin */
    dsp_chan(blk->cdr, n, s, 16U, &out->vo);    /* CDR 高 16 位 = ADC2 = Vo  */

    const float v1 = cf_mag(out->vin.h[1]);
    const float v2 = cf_mag(out->vo.h[1]);
    out->h2_ratio = (v1 > 0.0f) ? (cf_mag(out->vin.h[2]) / v1) : 0.0f;
    /* S=12 时 9f 折回 h3，h3 被污染 → 只有 S=16 才把 h3 当质量指标（§5.6） */
    out->h3_ratio = ((s == 16U) && (v1 > 0.0f)) ? (cf_mag(out->vin.h[3]) / v1) : 0.0f;
    out->sat_count = blk->sat_count;

    uint8_t q = 0U;
    if ((fabsf(out->vin.dc - (float)CFG_ADC_DC_NOM) > (float)CFG_ADC_DC_TOL) ||
        (fabsf(out->vo.dc - (float)CFG_ADC_DC_NOM) > (float)CFG_ADC_DC_TOL)) {
        q |= DSP_Q_DC_WARN;
    }
    if (blk->sat_count != 0U) {
        q |= DSP_Q_CLIP;
    }
    /* DDS 的谐波两通道共有，在比值里抵消；真正告警的是两通道之差 */
    const float h2_vin = (v1 > 0.0f) ? (cf_mag(out->vin.h[2]) / v1) : 0.0f;
    const float h2_vo = (v2 > 0.0f) ? (cf_mag(out->vo.h[2]) / v2) : 0.0f;
    if (fabsf(h2_vin - h2_vo) > CFG_H2_WARN) {
        q |= DSP_Q_H2_WARN;
    }
    /* Vo 峰值低于 ~1 mVrms（1.76 码）→ 信号过低 */
    if (out->vo.peak < 2.0f) {
        q |= DSP_Q_SIG_LOW;
    }
    out->quality = q;
}

void dsp_block_stats(const adc_block_t *blk, uint16_t *min_v, uint16_t *max_v)
{
    uint16_t lo = 0xFFFFU;
    uint16_t hi = 0U;
    if ((blk != 0) && (blk->n != 0U)) {
        for (uint16_t i = 0U; i < blk->n; ++i) {
            const uint32_t w = blk->cdr[i];
            const uint16_t a = (uint16_t)(w & 0xFFFFU);
            const uint16_t b = (uint16_t)(w >> 16);
            if (a < lo) { lo = a; }
            if (a > hi) { hi = a; }
            if (b < lo) { lo = b; }
            if (b > hi) { hi = b; }
        }
    }
    if (min_v != 0) { *min_v = lo; }
    if (max_v != 0) { *max_v = hi; }
}

/* ---- 比值与块间平均（§5.5 定稿：先比值再平均）--------------------------- */
cfloat_t dsp_ratio(const dsp_result_t *r)
{
    return cf_div(r->vin.h[1], r->vo.h[1]);
}

static float dsp_wrap_deg(float d)
{
    while (d > 180.0f) { d -= 360.0f; }
    while (d <= -180.0f) { d += 360.0f; }
    return d;
}

cfloat_t dsp_ratio_mean_arr(const cfloat_t *ratios, uint16_t k,
                            float *spread_lin, float *spread_ph)
{
    if (spread_lin != 0) { *spread_lin = 1.0e9f; }
    if (spread_ph != 0) { *spread_ph = 1.0e9f; }
    if ((ratios == 0) || (k == 0U)) {
        return cf_make(0.0f, 0.0f);
    }

    cfloat_t sum = cf_make(0.0f, 0.0f);
    for (uint16_t i = 0U; i < k; ++i) {
        sum = cf_add(sum, ratios[i]);
    }
    const cfloat_t mean = cf_scale(sum, 1.0f / (float)k);
    const float mean_mag = cf_mag(mean);
    if (mean_mag == 0.0f) {
        return mean;
    }

    /* 幅度散差：std(|R_b|)/|R̄| */
    float mag_sum = 0.0f;
    for (uint16_t i = 0U; i < k; ++i) {
        mag_sum += cf_mag(ratios[i]);
    }
    const float mag_mean = mag_sum / (float)k;
    float var = 0.0f;
    float ph_max = 0.0f;
    const float mean_arg = cf_arg_deg(mean);
    for (uint16_t i = 0U; i < k; ++i) {
        const float dm = cf_mag(ratios[i]) - mag_mean;
        var += dm * dm;
        const float dp = fabsf(dsp_wrap_deg(cf_arg_deg(ratios[i]) - mean_arg));
        if (dp > ph_max) { ph_max = dp; }
    }
    if (spread_lin != 0) {
        *spread_lin = sqrtf(var / (float)k) / mean_mag;
    }
    if (spread_ph != 0) {
        *spread_ph = ph_max;
    }
    return mean;
}

cfloat_t dsp_ratio_mean(const dsp_result_t *blks, uint16_t k,
                        float *spread_lin, float *spread_ph)
{
    if ((blks == 0) || (k == 0U)) {
        if (spread_lin != 0) { *spread_lin = 1.0e9f; }
        if (spread_ph != 0) { *spread_ph = 1.0e9f; }
        return cf_make(0.0f, 0.0f);
    }
    /* 只保留每块复比值（8 B/块），不缓存整谱，省 RAM（§8.3 工作区 ≤4 KB）。
     * 块数上限 32（100 kHz 精测 K=32），栈上 256 B 足够。 */
    cfloat_t ratios[32];
    const uint16_t kk = (k > 32U) ? 32U : k;
    for (uint16_t i = 0U; i < kk; ++i) {
        ratios[i] = dsp_ratio(&blks[i]);
    }
    return dsp_ratio_mean_arr(ratios, kk, spread_lin, spread_ph);
}

int dsp_combine(const dsp_result_t *blks, uint16_t k, dsp_result_t *avg, float *spread)
{
    if ((blks == 0) || (avg == 0) || (k == 0U)) {
        return -1;
    }
    float sl = 0.0f;
    float sp = 0.0f;
    (void)dsp_ratio_mean(blks, k, &sl, &sp);

    /* 谱平均只用于显示/谐波质量；阻抗结算走 dsp_ratio_mean（§5.5） */
    for (uint16_t h = 0U; h < ADC_S_MAX; ++h) {
        cfloat_t sv = cf_make(0.0f, 0.0f);
        cfloat_t so = cf_make(0.0f, 0.0f);
        for (uint16_t i = 0U; i < k; ++i) {
            sv = cf_add(sv, blks[i].vin.h[h]);
            so = cf_add(so, blks[i].vo.h[h]);
        }
        avg->vin.h[h] = cf_scale(sv, 1.0f / (float)k);
        avg->vo.h[h] = cf_scale(so, 1.0f / (float)k);
    }
    float dv = 0.0f, do_ = 0.0f, pv = 0.0f, po = 0.0f;
    uint32_t sat = 0U;
    uint8_t q = 0U;
    for (uint16_t i = 0U; i < k; ++i) {
        dv += blks[i].vin.dc;
        do_ += blks[i].vo.dc;
        pv += blks[i].vin.peak;
        po += blks[i].vo.peak;
        sat += blks[i].sat_count;
        q |= blks[i].quality;
    }
    avg->vin.dc = dv / (float)k;
    avg->vo.dc = do_ / (float)k;
    avg->vin.peak = pv / (float)k;
    avg->vo.peak = po / (float)k;
    avg->sat_count = (uint16_t)((sat > 65535U) ? 65535U : sat);

    const float v1 = cf_mag(avg->vin.h[1]);
    avg->h2_ratio = (v1 > 0.0f) ? (cf_mag(avg->vin.h[2]) / v1) : 0.0f;
    avg->h3_ratio = (v1 > 0.0f) ? (cf_mag(avg->vin.h[3]) / v1) : 0.0f;

    if ((k >= 2U) && (sl > CFG_SPREAD_WARN)) {
        q |= DSP_Q_SPREAD;
    }
    avg->quality = q;
    if (spread != 0) {
        *spread = sl;      /* 相对值：0.005 = 0.5% */
    }
    return 0;
}

float dsp_coherence_err(uint16_t n, uint16_t s, uint32_t fs_hz, uint32_t f_dds_mhz)
{
    if ((s == 0U) || (fs_hz == 0U)) {
        return 1.0e9f;
    }
    /* M_act = N·f_dds/fs，f_dds 用 mHz 保精度 */
    const float m_act = ((float)n * (float)f_dds_mhz) / (1000.0f * (float)fs_hz);
    const float m_ideal = (float)n / (float)s;
    return fabsf(m_act - m_ideal);
}