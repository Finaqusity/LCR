/* 宿主侧单元测试：校准闭式解 + 母公式 + 选档（§11.1 G5/G6/G7）。
 * 用 host/stm32f4xx.h 垫片编译 cal.c/meas.c，硬件函数被 -dead_strip 丢弃。
 *
 *   cc -std=gnu11 -O2 -Ihost -I../Core/Inc test_cal.c \
 *      ../Core/Src/cal.c ../Core/Src/meas.c ../Core/Src/dsp.c \
 *      -lm -Wl,-dead_strip -o /tmp/test_cal && /tmp/test_cal
 */
#include "cal.h"
#include "meas.h"
#include "dsp.h"
#include "board_config.h"

#include <math.h>
#include <stdio.h>

#define TOL_Z   1e-4
#define TOL_CAL 1e-8

static int g_fail;
static void check(int cond, const char *name)
{
    if (!cond) { printf("  FAIL: %s\n", name); ++g_fail; }
}
static int rel_ok(double got, double exp, double tol)
{
    const double scale = fabs(exp) > 1.0 ? fabs(exp) : 1.0;
    return fabs(got - exp) / scale <= tol;
}

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

static void g6(void)
{
    printf("G6 two-load closed form + inverse\n");
    const td_t k = td(1.02, 0.01), zs = td(0.5, 0.02), zp = td(3000.0, -4000.0);
    const double r1 = 1000.0, r2 = 10000.0;
    const td_t zopen = td_add(zs, td_mul(k, zp));
    const td_t zm1 = td_add(zs, td_mul(k, td_par(td(r1, 0.0), zp)));
    const td_t zm2 = td_add(zs, td_mul(k, td_par(td(r2, 0.0), zp)));
    double kr, ki, sr, si, pr, pi;
    const int rc = cal_solve_two_load(zm1.re, zm1.im, zm2.re, zm2.im,
                                      zopen.re, zopen.im, r1, r2,
                                      &kr, &ki, &sr, &si, &pr, &pi);
    check(rc == 0, "two-load rc==0");
    check(rel_ok(kr, k.re, TOL_CAL) && rel_ok(ki, k.im, TOL_CAL), "recover k");
    check(rel_ok(sr, zs.re, TOL_CAL) && rel_ok(si, zs.im, TOL_CAL), "recover zs");
    check(rel_ok(pr, zp.re, TOL_CAL) && rel_ok(pi, zp.im, TOL_CAL), "recover zp");
    double xr, xi;
    const int rc2 = cal_solve_inverse(zm1.re, zm1.im, sr, si, kr, ki, pr, pi, 1, &xr, &xi);
    check(rc2 == 0, "inverse rc==0");
    check(rel_ok(xr, r1, TOL_Z) && rel_ok(xi, 0.0, TOL_Z), "recover Zx=r1");
}

static void g5(void)
{
    printf("G5 mother formula Zm=-Zf*(m1/m2)/alpha\n");
    const uint32_t f = 100000U;
    const range_t r = RANGE_100K;
    const cfloat_t zf = cal_zf_eff(f, r);
    const cfloat_t alpha = cf_make(1.0f, 0.0f);
    const cfloat_t zx = cf_make(33000.0f, -12000.0f);
    const cfloat_t ratio = cf_neg(cf_div(cf_mul(zx, alpha), zf));
    const cfloat_t zm = cal_raw_impedance(f, r, ratio);
    check(rel_ok((double)zm.re, (double)zx.re, TOL_Z), "Zm.re==Zx.re");
    check(rel_ok((double)zm.im, (double)zx.im, TOL_Z), "Zm.im==Zx.im");
    /* Zf_eff 解析核对：Rf/(1+j2πfRfCf)，名义值取自 board_config.h */
    static const float rf_tab[CFG_RANGE_NUM] = CFG_RF_NOM_INITIALIZER;
    static const float cf_tab[CFG_RANGE_NUM] = CFG_CF_NOM_INITIALIZER;
    const double rf = (double)rf_tab[RANGE_100K], cf = (double)cf_tab[RANGE_100K];
    const double w = 2.0 * 3.14159265358979323846 * (double)f * rf * cf;
    const double zre = rf / (1.0 + w * w), zim = -rf * w / (1.0 + w * w);
    check(rel_ok((double)zf.re, zre, 1e-3) && rel_ok((double)zf.im, zim, 1e-3), "Zf_eff analytic");
}

static void g7(void)
{
    printf("G7 range pick @1kHz\n");
    struct { float z; int exp; } cs[] = {
        { 30.0f, 0 }, { 3000.0f, 2 }, { 10000.0f, 3 },
        { 300000.0f, 4 }, { 3000000.0f, 5 }
    };
    for (unsigned i = 0U; i < sizeof(cs) / sizeof(cs[0]); ++i) {
        const int got = meas_pick_range(cs[i].z, CFG_BAND_1KHZ);
        char nm[48];
        snprintf(nm, sizeof(nm), "pick %g -> %d (got %d)", (double)cs[i].z, cs[i].exp, got);
        check(got == cs[i].exp, nm);
    }
}

int main(void)
{
    g6(); g5(); g7();
    printf(g_fail ? "HOST CAL TESTS FAIL (%d)\n" : "HOST CAL TESTS PASS\n", g_fail);
    return g_fail ? 1 : 0;
}