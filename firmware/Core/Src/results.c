/* L3 结果：参数提取 + 定点格式化（§6.6、§8.4、§8.5）。
 * 不依赖 CMSIS / 不依赖 float printf：所有数字用整数手工排版。
 * 4 位有效数字的统一口径：先把 |v| 规格成 iv4∈[1000,9999] 与十进制指数 e，
 * 使 v = iv4·10^(e−3)；串口按 e 决定普通小数/科学计数，OLED 按工程前缀渲染。 */

#include "results.h"
#include "error.h"

#include <math.h>
#include <string.h>

#define RES_PI 3.14159265358979323846

/* ---- 参数提取（§6.6）--------------------------------------------------- */
elem_type_t results_type_of(float phi_deg)
{
    if (fabsf(phi_deg) < CFG_TYPE_R_DEG) {
        return ETYPE_R;
    }
    if (phi_deg < -CFG_TYPE_CL_DEG) {
        return ETYPE_C;
    }
    if (phi_deg > CFG_TYPE_CL_DEG) {
        return ETYPE_L;
    }
    return ETYPE_UNKNOWN;
}

elem_type_t results_type_of_slope(float slope)
{
    if (slope < CFG_SLOPE_C_MAX) {
        return ETYPE_C;
    }
    if (slope > CFG_SLOPE_L_MIN) {
        return ETYPE_L;
    }
    if ((slope >= CFG_SLOPE_R_LO) && (slope <= CFG_SLOPE_R_HI)) {
        return ETYPE_R;
    }
    return ETYPE_UNKNOWN;
}

uint8_t results_confidence(int type_clear, uint8_t cal_level, float spread)
{
    int c = 3;
    if (type_clear == 0) {
        --c;
    }
    if (cal_level < 3U) {
        --c;
    }
    if (!(spread < CFG_SPREAD_EARLY)) {
        --c;
    }
    return (uint8_t)((c < 0) ? 0 : c);
}

const char *results_unit(elem_type_t t)
{
    switch (t) {
    case ETYPE_C: return "F";
    case ETYPE_L: return "H";
    default:      return "ohm";
    }
}

static float res_value(elem_type_t t, cfloat_t zx, float w, float zmag)
{
    switch (t) {
    case ETYPE_C:
        return (w > 0.0f && zx.im < 0.0f) ? (-1.0f / (w * zx.im)) : 0.0f;
    case ETYPE_L:
        return (w > 0.0f) ? (zx.im / w) : 0.0f;
    case ETYPE_R:
        return zx.re;
    default:
        return zmag;      /* 类型未定：给 |Z| 作最可能值，由 meas 标注 err/conf */
    }
}

void results_set_type(meas_result_t *res, elem_type_t t, uint32_t f_mhz)
{
    if (res == 0) {
        return;
    }
    const float w = (float)(2.0 * RES_PI * (double)f_mhz / 1000.0);
    res->type = t;
    res->value = res_value(t, res->zx, w, res->z_mag);
}

void results_from_zx(meas_result_t *res, cfloat_t zx, uint32_t f_mhz)
{
    if (res == 0) {
        return;
    }
    res->zx = zx;
    res->rs = zx.re;
    res->xs = zx.im;
    res->z_mag = cf_mag(zx);
    res->z_phase_deg = cf_arg_deg(zx);

    const float ax = fabsf(zx.im);
    const float ar = fabsf(zx.re);
    /* D/Q 只在有抗/有阻部分时有意义；分母为 0 时给 0 而不是 inf（§8.5 规则 5） */
    res->d = (ax > 0.0f) ? (ar / ax) : 0.0f;
    res->q = (ar > 0.0f) ? (ax / ar) : 0.0f;

    /* C/L 换算必须用 DDS 实际频率（mHz），不是标称整数 Hz */
    results_set_type(res, results_type_of(res->z_phase_deg), f_mhz);
}

/* ---- 整数排版基础设施 --------------------------------------------------- */
typedef struct {
    char    *p;
    uint32_t cap;
    uint32_t n;
    int      ovf;
} sbuf_t;

static void sb_char(sbuf_t *b, char c)
{
    if (b->n + 1U < b->cap) {
        b->p[b->n] = c;
    } else {
        b->ovf = 1;
    }
    ++b->n;
}

static void sb_str(sbuf_t *b, const char *s)
{
    while (*s != '\0') {
        sb_char(b, *s++);
    }
}

static void sb_u32(sbuf_t *b, uint32_t v)
{
    char t[10];
    uint32_t k = 0U;
    do {
        t[k++] = (char)('0' + (v % 10U));
        v /= 10U;
    } while (v != 0U);
    while (k > 0U) {
        sb_char(b, t[--k]);
    }
}

static void sb_i32(sbuf_t *b, int32_t v)
{
    if (v < 0) {
        sb_char(b, '-');
        sb_u32(b, (uint32_t)(-(int64_t)v));
    } else {
        sb_u32(b, (uint32_t)v);
    }
}

static void sb_hex8(sbuf_t *b, uint32_t v)
{
    static const char hx[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; --i) {
        sb_char(b, hx[(v >> (i * 4)) & 0xFU]);
    }
}

/* 把 4 位有效数字尾数 iv4（1000..9999）按 shift 渲染：
 * shift ≥ 0 → 整数后补零；shift < 0 → 小数点左移 |shift| 位。 */
static void sb_mant(sbuf_t *b, uint32_t iv4, int shift)
{
    char d[4];
    d[0] = (char)('0' + (iv4 / 1000U) % 10U);
    d[1] = (char)('0' + (iv4 / 100U) % 10U);
    d[2] = (char)('0' + (iv4 / 10U) % 10U);
    d[3] = (char)('0' + iv4 % 10U);

    if (shift >= 0) {
        for (int i = 0; i < 4; ++i) {
            sb_char(b, d[i]);
        }
        for (int i = 0; i < shift; ++i) {
            sb_char(b, '0');
        }
        return;
    }
    const int point = 4 + shift;      /* 小数点前的位数 */
    if (point <= 0) {
        sb_str(b, "0.");
        for (int i = 0; i < -point; ++i) {
            sb_char(b, '0');
        }
        for (int i = 0; i < 4; ++i) {
            sb_char(b, d[i]);
        }
    } else {
        for (int i = 0; i < point; ++i) {
            sb_char(b, d[i]);
        }
        sb_char(b, '.');
        for (int i = point; i < 4; ++i) {
            sb_char(b, d[i]);
        }
    }
}

/* 规格化：v = sign · iv4 · 10^(e−3)，iv4∈[1000,9999]。返回符号（0/±1）。 */
static int sig4(float v, uint32_t *iv4, int *e)
{
    if (!(v == v) || (v == 0.0f)) {        /* NaN 也按 0 处理，绝不输出 inf/nan */
        *iv4 = 0U;
        *e = 0;
        return 0;
    }
    const int sign = (v < 0.0f) ? -1 : 1;
    double a = fabs((double)v);
    if (!isfinite(a)) {
        *iv4 = 0U;
        *e = 0;
        return 0;
    }
    int ex = 0;
    while (a >= 10.0) { a /= 10.0; ++ex; }
    while (a < 1.0)   { a *= 10.0; --ex; }
    uint32_t iv = (uint32_t)(a * 1000.0 + 0.5);
    if (iv >= 10000U) {                    /* 9.9996 → 10.00 进位 */
        iv = 1000U;
        ++ex;
    }
    if (iv < 1000U) {
        iv = 1000U;
    }
    *iv4 = iv;
    *e = ex;
    return sign;
}

/* 4 位有效数字、基本单位：[1e-3,1e4) 普通小数，之外 d.dddE±xx。 */
static void sb_sig(sbuf_t *b, float v)
{
    uint32_t iv4 = 0U;
    int e = 0;
    const int sign = sig4(v, &iv4, &e);
    if (iv4 == 0U) {
        sb_str(b, "0.000");
        return;
    }
    if (sign < 0) {
        sb_char(b, '-');
    }
    if ((e >= -3) && (e <= 3)) {
        sb_mant(b, iv4, e - 3);
    } else {
        sb_mant(b, iv4, -3);
        sb_char(b, 'E');
        if (e < 0) {
            sb_char(b, '-');
            sb_u32(b, (uint32_t)(-e));
        } else {
            sb_char(b, '+');
            sb_u32(b, (uint32_t)e);
        }
    }
}

int results_format_sig(float v, char *buf, uint32_t cap)
{
    sbuf_t b = { buf, cap, 0U, 0 };
    sb_sig(&b, v);
    sb_char(&b, '\0');
    return b.ovf ? -1 : (int)b.n;
}

int results_format_eng(float v, char *buf, uint32_t cap)
{
    static const char pfx[8][3] = { "p", "n", "u", "m", "", "k", "M", "G" };
    sbuf_t b = { buf, cap, 0U, 0 };
    uint32_t iv4 = 0U;
    int e = 0;
    const int sign = sig4(v, &iv4, &e);
    if (iv4 == 0U) {
        sb_str(&b, "0.000");
    } else {
        if (sign < 0) {
            sb_char(&b, '-');
        }
        int q = e / 3;
        if ((e < 0) && ((e % 3) != 0)) {
            --q;                           /* 向下取整，保证尾数 ∈ [1,1000) */
        }
        int idx = q + 4;
        if (idx < 0) { idx = 0; }
        if (idx > 7) { idx = 7; }
        sb_mant(&b, iv4, e - 3 - 3 * q);   /* shift ∈ {−3,−2,−1} */
        sb_str(&b, pfx[idx]);
    }
    sb_char(&b, '\0');
    return b.ovf ? -1 : (int)b.n;
}

int results_format_fixed(float v, uint8_t dec, char *buf, uint32_t cap)
{
    static const uint32_t p10[10] = {
        1U, 10U, 100U, 1000U, 10000U, 100000U, 1000000U, 10000000U, 100000000U, 1000000000U
    };
    sbuf_t b = { buf, cap, 0U, 0 };
    if (dec > 9U) {
        dec = 9U;
    }
    if (!(v == v) || !isfinite((double)v)) {
        v = 0.0f;
    }
    if (v < 0.0f) {
        sb_char(&b, '-');
    }
    const double a = fabs((double)v);
    const uint64_t iv = (uint64_t)(a * (double)p10[dec] + 0.5);
    const uint64_t ip = iv / p10[dec];
    const uint64_t fp = iv % p10[dec];
    sb_u32(&b, (uint32_t)ip);
    if (dec > 0U) {
        sb_char(&b, '.');
        /* 小数部分补前导零到 dec 位 */
        uint32_t div = p10[dec - 1U];
        uint64_t rem = fp;
        for (uint8_t i = 0U; i < dec; ++i) {
            sb_char(&b, (char)('0' + (uint32_t)(rem / div)));
            rem %= div;
            div /= 10U;
        }
    }
    sb_char(&b, '\0');
    return b.ovf ? -1 : (int)b.n;
}

/* ---- 结果行（§8.5）----------------------------------------------------- */
int results_format_line(const meas_result_t *res, uint32_t time_ms,
                        char *buf, uint32_t cap)
{
    if ((res == 0) || (buf == 0) || (cap == 0U)) {
        return -1;
    }
    sbuf_t b = { buf, cap, 0U, 0 };
    char t[24];

    sb_str(&b, "Z,");
    sb_sig(&b, res->z_mag);
    sb_char(&b, ',');
    results_format_fixed(res->z_phase_deg, 1U, t, sizeof(t));
    sb_str(&b, t);
    sb_str(&b, ",R=");
    sb_sig(&b, res->rs);
    sb_str(&b, ",X=");
    sb_sig(&b, res->xs);
    sb_str(&b, ",type=");
    sb_char(&b, (char)res->type);
    sb_str(&b, ",val=");
    sb_sig(&b, res->value);
    sb_str(&b, ",unit=");
    sb_str(&b, results_unit(res->type));
    sb_str(&b, ",D=");
    results_format_fixed(res->d, 3U, t, sizeof(t));
    sb_str(&b, t);
    sb_str(&b, ",Q=");
    results_format_fixed(res->q, 3U, t, sizeof(t));
    sb_str(&b, t);
    sb_str(&b, ",f=");
    sb_u32(&b, res->f_hz);
    sb_str(&b, ",range=");
    sb_u32(&b, (uint32_t)res->range);
    sb_str(&b, ",cal=");
    sb_u32(&b, (uint32_t)res->cal_level);
    sb_str(&b, ",conf=");
    sb_u32(&b, (uint32_t)res->confidence);
    sb_str(&b, ",spread=");
    results_format_fixed(res->spread * 100.0f, 3U, t, sizeof(t));
    sb_str(&b, t);
    sb_str(&b, ",h2=");
    results_format_fixed(res->h2 * 100.0f, 3U, t, sizeof(t));
    sb_str(&b, t);
    sb_str(&b, ",t=");
    sb_u32(&b, time_ms);
    sb_str(&b, ",err=");
    sb_i32(&b, meas_err_code(res->err));
    sb_str(&b, ",diag=");
    sb_hex8(&b, res->diag32);
    sb_str(&b, "\r\n");
    sb_char(&b, '\0');
    return b.ovf ? -1 : (int)b.n;
}

int results_format_main(const meas_result_t *res, char *buf, uint32_t cap)
{
    if ((res == 0) || (buf == 0) || (cap == 0U)) {
        return -1;
    }
    sbuf_t b = { buf, cap, 0U, 0 };
    char t[24];
    sb_char(&b, (char)res->type);
    sb_char(&b, '=');
    results_format_eng(res->value, t, sizeof(t));
    sb_str(&b, t);
    switch (res->type) {
    case ETYPE_C: sb_str(&b, "F"); break;
    case ETYPE_L: sb_str(&b, "H"); break;
    default:      sb_str(&b, "R"); break;   /* ohm 用 R 表示，OLED 字库无 Ω */
    }
    sb_char(&b, '\0');
    return b.ovf ? -1 : (int)b.n;
}