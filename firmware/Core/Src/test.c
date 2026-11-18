/* 板上金标准回归（§8.6/§10.2）。自包含：合成整数波形与已知闭式解，
 * 与 double 参考逐项比对。容差：幅度 1e-4 相对、相位 1e-3°、阻抗 1e-4 相对、
 * 校准闭式解 1e-8 相对。仅 LCR_SELFTEST 时编译。 */

#include "test.h"
#include "board_config.h"
#include "dsp.h"
#include "adc.h"
#include "cal.h"
#include "meas.h"
#include "results.h"
#include "uart.h"

#include <math.h>
#include <string.h>

#define TOL_AMP   1e-4
#define TOL_PH    1e-3
#define TOL_Z     1e-4
#define TOL_CAL   1e-8
#define PI_D      3.14159265358979323846

static adc_block_t  t_blk;     /* 4 KB 放 .bss */
static dsp_result_t t_res;

static int rel_ok(double got, double exp, double tol)
{
    const double d = fabs(got - exp);
    const double scale = fabs(exp) > 1.0 ? fabs(exp) : 1.0;
    return (d / scale) <= tol;
}

/* 参考 DFT：直接 O(N) 求 bin M=N/S（double），与折叠算法对照。 */
static void ref_bin(int chan, int n, int s, double *re, double *im)
{
    const int m = n / s;
    double xr = 0.0, xi = 0.0;
    for (int i = 0; i < n; ++i) {
        const uint32_t v = (t_blk.cdr[i] >> (chan ? 16 : 0)) & 0xFFFFU;
        const double ang = -2.0 * PI_D * (double)m * (double)i / (double)n;
        xr += (double)v * cos(ang);
        xi += (double)v * sin(ang);
    }
    *re = xr; *im = xi;
}

static void synth(int n, int s, double a1, double p1, double a2, double p2)
{
    memset(&t_blk, 0, sizeof(t_blk));
    t_blk.n = (uint16_t)n;
    t_blk.s = (uint16_t)s;
    for (int i = 0; i < n; ++i) {
        const double th = 2.0 * PI_D * (double)i / (double)s;
        long v1 = lround(2048.0 + a1 * cos(th + p1));
        long v2 = lround(2048.0 + a2 * cos(th + p2));
        if (v1 < 0) { v1 = 0; } if (v1 > 4095) { v1 = 4095; }
        if (v2 < 0) { v2 = 0; } if (v2 > 4095) { v2 = 4095; }
        t_blk.cdr[i] = (uint32_t)v1 | ((uint32_t)v2 << 16);
    }
}

/* G1：S=16 与 S=12 单频正弦，DFT 基波幅度/相位/归一化。 */
static int g1(void)
{
    int fail = 0;
    const int cases[2][2] = { {1024, 16}, {1008, 12} };
    for (int c = 0; c < 2; ++c) {
        const int n = cases[c][0], s = cases[c][1];
        const double a1 = 1000.0, p1 = 0.5236;   /* 30° */
        const double a2 = 500.0,  p2 = -1.0472;  /* −60° */
        synth(n, s, a1, p1, a2, p2);
        dsp_analyze(&t_blk, &t_res);
        double rr, ri;
        ref_bin(0, n, s, &rr, &ri);
        const double peak_ref = 2.0 * sqrt(rr * rr + ri * ri) / (double)n;
        const double ph_ref = atan2(ri, rr) * 180.0 / PI_D;
        if (!rel_ok((double)t_res.vin.peak, peak_ref, TOL_AMP)) { ++fail; }
        if (fabs((double)t_res.vin.h[1].re - rr) > 1e-3 * (fabs(rr) + 1.0)) { ++fail; }
        const double ph_got = atan2((double)t_res.vin.h[1].im,
                                    (double)t_res.vin.h[1].re) * 180.0 / PI_D;
        if (fabs(ph_got - ph_ref) > TOL_PH) { ++fail; }
        ref_bin(1, n, s, &rr, &ri);
        const double peak2 = 2.0 * sqrt(rr * rr + ri * ri) / (double)n;
        if (!rel_ok((double)t_res.vo.peak, peak2, TOL_AMP)) { ++fail; }
    }
    uart_printf("G1 DFT(S16/S12) %s\r\n", fail ? "FAIL" : "PASS");
    return fail;
}

/* G4：两通道复比值 = 幅度比 e^{j相位差}。 */
static int g4(void)
{
    synth(1024, 16, 1000.0, 0.5236, 500.0, -1.0472);
    dsp_analyze(&t_blk, &t_res);
    const cfloat_t r = dsp_ratio(&t_res);
    const double mag_exp = 2.0;                 /* 1000/500 */
    const double ph_exp = (0.5236 - (-1.0472)) * 180.0 / PI_D;  /* 90° */
    int fail = 0;
    if (!rel_ok((double)cf_mag(r), mag_exp, TOL_AMP)) { ++fail; }
    if (fabs((double)cf_arg_deg(r) - ph_exp) > TOL_PH) { ++fail; }
    uart_printf("G4 ratio %s\r\n", fail ? "FAIL" : "PASS");
    return fail;
}

/* G5：母公式 Zx=−Zf_eff·(m1/m2)/α，含 Rf∥Cf。 */
static int g5(void)
{
    const uint32_t f = 100000U;
    const range_t r = RANGE_100K;
    const cfloat_t zf = cal_zf_eff(f, r);
    const cfloat_t alpha = cf_make(1.0f, 0.0f);
    const cfloat_t zx = cf_make(33000.0f, -12000.0f);
    /* ratio = −Zx·α/Zf */
    const cfloat_t ratio = cf_neg(cf_div(cf_mul(zx, alpha), zf));
    const cfloat_t zm = cf_neg(cf_div(cf_mul(zf, ratio), alpha));
    int fail = 0;
    if (!rel_ok((double)zm.re, (double)zx.re, TOL_Z)) { ++fail; }
    if (!rel_ok((double)zm.im, (double)zx.im, TOL_Z)) { ++fail; }
    uart_printf("G5 mother-formula %s\r\n", fail ? "FAIL" : "PASS");
    return fail;
}

/* ---- double 复数（G6 合成用）---- */
typedef struct { double re, im; } td_t;
static td_t td(double re, double im) { td_t z = { re, im }; return z; }
static td_t td_add(td_t a, td_t b) { return td(a.re + b.re, a.im + b.im); }
static td_t td_mul(td_t a, td_t b) { return td(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re); }
static td_t td_div(td_t a, td_t b)
{
    const double d = b.re * b.re + b.im * b.im;
    return td((a.re * b.re + a.im * b.im) / d, (a.im * b.re - a.re * b.im) / d);
}
static td_t td_par(td_t a, td_t b) { return td_div(td_mul(a, b), td_add(a, b)); }

/* G6：双负载闭式解回收 Zs/k/Zp（<1e-8）+ 反解回收 Zx。 */
static int g6(void)
{
    const td_t k = td(1.02, 0.01);
    const td_t zs = td(0.5, 0.02);
    const td_t zp = td(3000.0, -4000.0);
    const double r1 = 1000.0, r2 = 10000.0;
    const td_t zopen = td_add(zs, td_mul(k, zp));
    const td_t zm1 = td_add(zs, td_mul(k, td_par(td(r1, 0.0), zp)));
    const td_t zm2 = td_add(zs, td_mul(k, td_par(td(r2, 0.0), zp)));

    double kr, ki, sr, si, pr, pi;
    const int rc = cal_solve_two_load(zm1.re, zm1.im, zm2.re, zm2.im,
                                      zopen.re, zopen.im, r1, r2,
                                      &kr, &ki, &sr, &si, &pr, &pi);
    int fail = 0;
    if (rc != 0) { ++fail; }
    if (!rel_ok(kr, k.re, TOL_CAL) || !rel_ok(ki, k.im, TOL_CAL)) { ++fail; }
    if (!rel_ok(sr, zs.re, TOL_CAL) || !rel_ok(si, zs.im, TOL_CAL)) { ++fail; }
    if (!rel_ok(pr, zp.re, TOL_CAL) || !rel_ok(pi, zp.im, TOL_CAL)) { ++fail; }

    double xr, xi;
    const int rc2 = cal_solve_inverse(zm1.re, zm1.im, sr, si, kr, ki, pr, pi, 1,
                                      &xr, &xi);
    if (rc2 != 0) { ++fail; }
    if (!rel_ok(xr, r1, TOL_Z) || !rel_ok(xi, 0.0, TOL_Z)) { ++fail; }
    uart_printf("G6 two-load/inverse %s\r\n", fail ? "FAIL" : "PASS");
    return fail;
}

/* G7：ρ 表驱动选档（1 kHz 全档合法）。 */
static int g7(void)
{
    struct { float z; int exp; } cs[] = {
        { 30.0f, 0 }, { 3000.0f, 2 }, { 10000.0f, 3 },
        { 300000.0f, 4 }, { 3000000.0f, 5 }
    };
    int fail = 0;
    for (unsigned i = 0U; i < sizeof(cs) / sizeof(cs[0]); ++i) {
        const int got = meas_pick_range(cs[i].z, CFG_BAND_1KHZ);
        if (got != cs[i].exp) { ++fail; }
    }
    uart_printf("G7 range-pick %s\r\n", fail ? "FAIL" : "PASS");
    return fail;
}

/* G8：类型判别边界 + 双频斜率。 */
static int g8(void)
{
    int fail = 0;
    if (results_type_of(5.6f) != ETYPE_R) { ++fail; }
    if (results_type_of(5.8f) != ETYPE_UNKNOWN) { ++fail; }
    if (results_type_of(-83.9f) != ETYPE_UNKNOWN) { ++fail; }
    if (results_type_of(-84.1f) != ETYPE_C) { ++fail; }
    if (results_type_of(83.9f) != ETYPE_UNKNOWN) { ++fail; }
    if (results_type_of(84.1f) != ETYPE_L) { ++fail; }
    if (results_type_of_slope(0.1f) != ETYPE_C) { ++fail; }
    if (results_type_of_slope(6.0f) != ETYPE_L) { ++fail; }
    if (results_type_of_slope(1.0f) != ETYPE_R) { ++fail; }
    if (results_type_of_slope(0.3f) != ETYPE_UNKNOWN) { ++fail; }
    uart_printf("G8 type-boundary %s\r\n", fail ? "FAIL" : "PASS");
    return fail;
}

/* G9：参数提取 Rs/Xs/D/Q/Cs/Ls。 */
static int g9(void)
{
    int fail = 0;
    meas_result_t r;
    results_from_zx(&r, cf_make(1000.0f, 0.0f), 1000000U);   /* 纯 R @1 kHz */
    if (r.type != ETYPE_R || !rel_ok((double)r.value, 1000.0, TOL_Z)) { ++fail; }
    if (!rel_ok((double)r.rs, 1000.0, TOL_Z) || fabs((double)r.xs) > 1e-3) { ++fail; }

    results_from_zx(&r, cf_make(0.0f, -1000.0f), 1000000U);  /* 纯 C @1 kHz */
    const double cexp = 1.0 / (2.0 * PI_D * 1000.0 * 1000.0);
    if (r.type != ETYPE_C || !rel_ok((double)r.value, cexp, TOL_Z)) { ++fail; }

    results_from_zx(&r, cf_make(0.0f, 1000.0f), 1000000U);   /* 纯 L @1 kHz */
    const double lexp = 1000.0 / (2.0 * PI_D * 1000.0);
    if (r.type != ETYPE_L || !rel_ok((double)r.value, lexp, TOL_Z)) { ++fail; }
    uart_printf("G9 param-extract %s\r\n", fail ? "FAIL" : "PASS");
    return fail;
}

/* G10：频率字四舍五入 + f_dds 反算。 */
static int g10(void)
{
    const cfg_band_t bands[CFG_FREQ_NUM] = CFG_BANDS_INITIALIZER;
    int fail = 0;
    for (int b = 0; b < (int)CFG_FREQ_NUM; ++b) {
        const double reg = (double)bands[b].f_nom * 268435456.0 / 8000000.0;
        const uint32_t expect = (uint32_t)llround(reg);
        if (expect != bands[b].freqreg) { ++fail; }
        const double f_act = (double)bands[b].freqreg * 8000000.0 / 268435456.0;
        if (fabs(f_act - (double)bands[b].f_nom) / (double)bands[b].f_nom > 2e-4) { ++fail; }
    }
    uart_printf("G10 freqword %s\r\n", fail ? "FAIL" : "PASS");
    return fail;
}

int test_run_golden(void)
{
    uart_write("# GOLDEN regression start\r\n");
    int fail = 0;
    fail += g1();
    fail += g4();
    fail += g5();
    fail += g6();
    fail += g7();
    fail += g8();
    fail += g9();
    fail += g10();
    uart_printf("# GOLDEN %s total_fail=%d\r\n", fail ? "FAIL" : "PASS", fail);
    return fail;
}