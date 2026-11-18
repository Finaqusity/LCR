/* 宿主侧单元测试（§11.1）：用本机 clang/gcc 编译 dsp.c + results.c，
 * 跑金标准 G1/G2/G3/G4/G8/G9。纯算法、无硬件依赖。
 *
 * 构建运行（macOS/Linux）：
 *   cc -std=gnu11 -O2 -I../Core/Inc test_dsp.c ../Core/Src/dsp.c \
 *      ../Core/Src/results.c -lm -o /tmp/test_dsp && /tmp/test_dsp
 * 退出码 0=全过。容差：幅度 1e-4 相对、相位 1e-3°、阻抗 1e-4 相对。 */

#include "dsp.h"
#include "results.h"
#include "board_config.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define PI_D 3.14159265358979323846
#define TOL_AMP 1e-4
#define TOL_PH  1e-3
#define TOL_Z   1e-4

static adc_block_t blk;
static dsp_result_t res;
static int g_fail;

static void check(int cond, const char *name)
{
    if (!cond) {
        printf("  FAIL: %s\n", name);
        ++g_fail;
    }
}

static int rel_ok(double got, double exp, double tol)
{
    const double scale = fabs(exp) > 1.0 ? fabs(exp) : 1.0;
    return fabs(got - exp) / scale <= tol;
}

/* 合成两通道整数波形：chan1=A1·cos(2πn/S+p1)，chan2=A2·cos(2πn/S+p2)，DC=2048。
 * 可叠加 h2/h3 谐波与额外 DC。 */
static void synth(int n, int s, double a1, double p1, double a2, double p2,
                  double h2, double h3, double dc)
{
    memset(&blk, 0, sizeof(blk));
    blk.n = (uint16_t)n;
    blk.s = (uint16_t)s;
    for (int i = 0; i < n; ++i) {
        const double th = 2.0 * PI_D * (double)i / (double)s;
        double v1 = dc + a1 * cos(th + p1) + h2 * a1 * cos(2.0 * th) + h3 * a1 * cos(3.0 * th);
        double v2 = dc + a2 * cos(th + p2);
        long c1 = lround(v1), c2 = lround(v2);
        if (c1 < 0) { c1 = 0; } if (c1 > 4095) { c1 = 4095; }
        if (c2 < 0) { c2 = 0; } if (c2 > 4095) { c2 = 4095; }
        blk.cdr[i] = (uint32_t)c1 | ((uint32_t)c2 << 16);
    }
}

/* 参考 DFT bin M=N/S（double，直接 O(N)）。 */
static void ref_bin(int chan, double *re, double *im)
{
    const int n = blk.n, s = blk.s, m = n / s;
    double xr = 0.0, xi = 0.0;
    for (int i = 0; i < n; ++i) {
        const uint32_t v = (blk.cdr[i] >> (chan ? 16 : 0)) & 0xFFFFU;
        const double ang = -2.0 * PI_D * (double)m * (double)i / (double)n;
        xr += (double)v * cos(ang);
        xi += (double)v * sin(ang);
    }
    *re = xr; *im = xi;
}

static void g1(void)
{
    printf("G1 DFT fundamental S=16/S=12\n");
    const int cs[2][2] = { {1024, 16}, {1008, 12} };
    for (int c = 0; c < 2; ++c) {
        synth(cs[c][0], cs[c][1], 1000.0, 0.5236, 500.0, -1.0472, 0.0, 0.0, 2048.0);
        dsp_analyze(&blk, &res);
        double rr, ri;
        ref_bin(0, &rr, &ri);
        const double peak = 2.0 * sqrt(rr * rr + ri * ri) / (double)blk.n;
        const double ph = atan2(ri, rr) * 180.0 / PI_D;
        check(rel_ok((double)res.vin.peak, peak, TOL_AMP), "vin peak");
        const double gph = atan2((double)res.vin.h[1].im, (double)res.vin.h[1].re) * 180.0 / PI_D;
        check(fabs(gph - ph) <= TOL_PH, "vin phase");
        ref_bin(1, &rr, &ri);
        const double peak2 = 2.0 * sqrt(rr * rr + ri * ri) / (double)blk.n;
        check(rel_ok((double)res.vo.peak, peak2, TOL_AMP), "vo peak");
    }
}

static void g2(void)
{
    printf("G2 harmonics h2/h3\n");
    synth(1024, 16, 1000.0, 0.3, 500.0, -0.7, 0.05, 0.02, 2048.0);
    dsp_analyze(&blk, &res);
    check(rel_ok((double)res.h2_ratio, 0.05, 2e-3), "h2 ratio ~0.05");
    check(rel_ok((double)res.h3_ratio, 0.02, 2e-3), "h3 ratio ~0.02");
}

static void g3(void)
{
    printf("G3 DC rejection\n");
    synth(1024, 16, 1000.0, 0.9, 500.0, 0.2, 0.0, 0.0, 2500.0);
    dsp_analyze(&blk, &res);
    double rr, ri;
    ref_bin(0, &rr, &ri);
    const double peak = 2.0 * sqrt(rr * rr + ri * ri) / (double)blk.n;
    check(rel_ok((double)res.vin.peak, peak, TOL_AMP), "peak unaffected by DC");
    check(fabs((double)res.vin.dc - 2500.0) < 1.0, "dc ~2500");
}

static void g4(void)
{
    printf("G4 complex ratio\n");
    synth(1024, 16, 1000.0, 0.5236, 500.0, -1.0472, 0.0, 0.0, 2048.0);
    dsp_analyze(&blk, &res);
    const cfloat_t r = dsp_ratio(&res);
    /* 金标准 = 同一量化码的 double DFT 之比（量化误差对两通道一致，比值里抵消）。 */
    double r1r, r1i, r2r, r2i;
    ref_bin(0, &r1r, &r1i);
    ref_bin(1, &r2r, &r2i);
    const double d = r2r * r2r + r2i * r2i;
    const double er = (r1r * r2r + r1i * r2i) / d;
    const double ei = (r1i * r2r - r1r * r2i) / d;
    const double emag = sqrt(er * er + ei * ei);
    const double eph = atan2(ei, er) * 180.0 / PI_D;
    check(rel_ok((double)cf_mag(r), emag, TOL_AMP), "ratio mag vs ref");
    check(fabs((double)cf_arg_deg(r) - eph) <= TOL_PH, "ratio phase vs ref");
}

static void g8(void)
{
    printf("G8 type boundaries\n");
    check(results_type_of(5.6f) == ETYPE_R, "5.6 -> R");
    check(results_type_of(5.8f) == ETYPE_UNKNOWN, "5.8 -> ?");
    check(results_type_of(-83.9f) == ETYPE_UNKNOWN, "-83.9 -> ?");
    check(results_type_of(-84.1f) == ETYPE_C, "-84.1 -> C");
    check(results_type_of(83.9f) == ETYPE_UNKNOWN, "83.9 -> ?");
    check(results_type_of(84.1f) == ETYPE_L, "84.1 -> L");
    check(results_type_of_slope(0.1f) == ETYPE_C, "slope0.1 -> C");
    check(results_type_of_slope(6.0f) == ETYPE_L, "slope6 -> L");
    check(results_type_of_slope(1.0f) == ETYPE_R, "slope1 -> R");
    check(results_type_of_slope(0.3f) == ETYPE_UNKNOWN, "slope0.3 -> ?");
}

static void g9(void)
{
    printf("G9 parameter extraction\n");
    meas_result_t r;
    results_from_zx(&r, cf_make(1000.0f, 0.0f), 1000000U);
    check(r.type == ETYPE_R && rel_ok((double)r.value, 1000.0, TOL_Z), "R 1000");
    results_from_zx(&r, cf_make(0.0f, -1000.0f), 1000000U);
    const double cexp = 1.0 / (2.0 * PI_D * 1000.0 * 1000.0);
    check(r.type == ETYPE_C && rel_ok((double)r.value, cexp, TOL_Z), "C 159nF");
    results_from_zx(&r, cf_make(0.0f, 1000.0f), 1000000U);
    const double lexp = 1000.0 / (2.0 * PI_D * 1000.0);
    check(r.type == ETYPE_L && rel_ok((double)r.value, lexp, TOL_Z), "L 159mH");
    results_from_zx(&r, cf_make(1000.0f, -1000.0f), 1000000U);
    check(rel_ok((double)r.d, 1.0, TOL_Z), "D=1 for R=|X|");
    check(rel_ok((double)r.q, 1.0, TOL_Z), "Q=1 for R=|X|");
}

int main(void)
{
    g1(); g2(); g3(); g4(); g8(); g9();
    printf(g_fail ? "HOST TESTS FAIL (%d)\n" : "HOST TESTS PASS\n", g_fail);
    return g_fail ? 1 : 0;
}