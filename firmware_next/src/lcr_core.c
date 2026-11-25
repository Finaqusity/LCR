#include "lcr_core.h"

#include <float.h>
#include <math.h>

static const double tau = 6.283185307179586476925286766559;
const lcr_frequency lcr_frequencies[4] = {
    {100u, 52500u, 128u}, {1000u, 5250u, 1024u},
    {10000u, 525u, 1024u}, {100000u, 70u, 1200u}
};
/* Range relay masks, indexed 10R, 100R, 1K, 10K, 100K, 1M.
 * Bit n = PDn, high = relay energized (break-before-make; K78 cal = PD6).
 * 10R drives PD0 and PD7 together: those two coils are wired directly and
 * must switch in the same write. */
const uint8_t lcr_range_masks[6] = {0x81u, 0x02u, 0x20u, 0x04u, 0x08u, 0x10u};

static int finite_complex(lcr_complex z) {
    return isfinite(z.re) && isfinite(z.im);
}

static lcr_complex multiply(lcr_complex a, lcr_complex b) {
    lcr_complex out = {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re};
    return out;
}

static lcr_status divide(lcr_complex a, lcr_complex b, lcr_complex *out) {
    const double denominator = b.re * b.re + b.im * b.im;
    if (!out || !finite_complex(a) || !finite_complex(b) ||
        !isfinite(denominator) || denominator < DBL_MIN) return LCR_BAD_INPUT;
    lcr_complex value = {(a.re * b.re + a.im * b.im) / denominator,
                         (a.im * b.re - a.re * b.im) / denominator};
    if (!finite_complex(value)) return LCR_BAD_INPUT;
    *out = value;
    return LCR_OK;
}

uint32_t lcr_dds_word(uint32_t hz) {
    /* Invalid or out-of-Nyquist requests are rejected as word zero. */
    if (!hz || hz >= 4000000u) return 0u;
    return (uint32_t)((((uint64_t)hz << 28) + 4000000u) / 8000000u);
}

double lcr_sample_omega(const lcr_frequency *frequency) {
    if (!frequency || !frequency->timer_divisor) return 0.0;
    return tau * (double)lcr_dds_word(frequency->nominal_hz) *
           frequency->timer_divisor / (268435456.0 * 10.5);
}

static lcr_status solve(double a[3][4], double x[3]) {
    for (unsigned col = 0; col < 3u; ++col) {
        unsigned pivot = col;
        for (unsigned row = col + 1u; row < 3u; ++row)
            if (fabs(a[row][col]) > fabs(a[pivot][col])) pivot = row;
        if (fabs(a[pivot][col]) < 1e-10) return LCR_SINGULAR;
        for (unsigned j = 0; j < 4u; ++j) {
            const double t = a[pivot][j];
            a[pivot][j] = a[col][j]; a[col][j] = t;
        }
        const double scale = a[col][col];
        for (unsigned j = 0; j < 4u; ++j) a[col][j] /= scale;
        for (unsigned row = 0; row < 3u; ++row) {
            if (row == col) continue;
            const double factor = a[row][col];
            for (unsigned j = 0; j < 4u; ++j) a[row][j] -= factor * a[col][j];
        }
    }
    for (unsigned i = 0; i < 3u; ++i) x[i] = a[i][3];
    return LCR_OK;
}

lcr_status lcr_fit_adc(const uint16_t *samples, size_t count, size_t stride,
                       double omega, lcr_fit *out) {
    if (!samples || !out || count < 8u || count > 65535u || !stride ||
        (count - 1u) > SIZE_MAX / stride || !isfinite(omega) ||
        omega <= 0.0 || omega >= tau / 2.0) return LCR_BAD_INPUT;
    double augmented[3][4] = {{0}};
    double beta[3];
    uint16_t low = 4095u, high = 0u;
    const double step_c = cos(omega), step_s = sin(omega);
    double cosine = 1.0, sine = 0.0;
    for (size_t i = 0u; i < count; ++i) {
        const uint16_t value = samples[i * stride];
        if (value > 4095u) return LCR_BAD_INPUT;
        if (value < low) low = value;
        if (value > high) high = value;
        const double basis[3] = {1.0, cosine, sine};
        for (unsigned row = 0u; row < 3u; ++row) {
            for (unsigned col = 0u; col < 3u; ++col)
                augmented[row][col] += basis[row] * basis[col];
            augmented[row][3] += basis[row] * value;
        }
        const double next = cosine * step_c - sine * step_s;
        sine = sine * step_c + cosine * step_s;
        cosine = next;
    }
    lcr_status status = solve(augmented, beta);
    if (status != LCR_OK) return status;
    double energy = 0.0;
    cosine = 1.0; sine = 0.0;
    for (size_t i = 0u; i < count; ++i) {
        const double error = samples[i * stride] - beta[0] - beta[1] * cosine - beta[2] * sine;
        energy += error * error;
        const double next = cosine * step_c - sine * step_s;
        sine = sine * step_c + cosine * step_s;
        cosine = next;
    }
    lcr_fit value = {{beta[1], -beta[2]}, beta[0], sqrt(energy / (double)count),
                     low, high, (uint8_t)(low <= 8u || high >= 4087u)};
    if (!finite_complex(value.phasor) || !isfinite(value.dc) || !isfinite(value.residual_rms))
        return LCR_BAD_INPUT;
    *out = value;
    return LCR_OK; /* Clipping is metadata; raw diagnostics can still inspect this fit. */
}

lcr_status lcr_feedback(double rf, double cf, double hz, lcr_complex *out) {
    if (!isfinite(rf) || !isfinite(cf) || !isfinite(hz) || rf <= 0 || cf < 0 || hz <= 0)
        return LCR_BAD_INPUT;
    const lcr_complex one = {1.0, 0.0}, admittance = {1.0 / rf, tau * hz * cf};
    return divide(one, admittance, out);
}

static lcr_status quality(const lcr_fit *value, int allow_weak) {
    if (!value || !finite_complex(value->phasor) || !isfinite(value->dc) ||
        !isfinite(value->residual_rms) || value->residual_rms < 0) return LCR_BAD_INPUT;
    if (value->clipped) return LCR_CLIPPED;
    const double rms = hypot(value->phasor.re, value->phasor.im) / 1.4142135623730950488;
    if (!allow_weak && (rms < 2.0 || rms < 10.0 * value->residual_rms)) return LCR_WEAK_SIGNAL;
    return LCR_OK;
}

lcr_status lcr_impedance_ex(const lcr_fit *v1, const lcr_fit *v2,
                            lcr_complex zf, lcr_complex alpha, int allow_weak,
                            lcr_complex *out) {
    lcr_status status = quality(v1, allow_weak);
    if (status != LCR_OK) return status;
    status = quality(v2, allow_weak);
    if (status != LCR_OK) return status;
    if (!finite_complex(zf) || !finite_complex(alpha) ||
        hypot(zf.re, zf.im) < 1e-12 || hypot(alpha.re, alpha.im) < 1e-12)
        return LCR_BAD_INPUT;
    lcr_complex ratio;
    status = divide(v1->phasor, v2->phasor, &ratio);
    if (status != LCR_OK) return status;
    lcr_complex numerator = multiply(zf, ratio);
    numerator.re = -numerator.re; numerator.im = -numerator.im;
    return divide(numerator, alpha, out);
}

lcr_status lcr_impedance(const lcr_fit *v1, const lcr_fit *v2,
                         lcr_complex zf, lcr_complex alpha, lcr_complex *out) {
    return lcr_impedance_ex(v1, v2, zf, alpha, 0, out);
}

lcr_status lcr_open_admittance(const lcr_fit *v1, const lcr_fit *v2,
                               lcr_complex zf, lcr_complex alpha, lcr_complex *out) {
    lcr_status status = quality(v1, 0);
    if (status != LCR_OK) return status;
    status = quality(v2, 1);
    if (status != LCR_OK) return status;
    if (!finite_complex(zf) || !finite_complex(alpha) ||
        hypot(zf.re, zf.im) < 1e-12 || hypot(alpha.re, alpha.im) < 1e-12)
        return LCR_BAD_INPUT;
    lcr_complex numerator = multiply(alpha, v2->phasor);
    numerator.re = -numerator.re; numerator.im = -numerator.im;
    return divide(numerator, multiply(zf, v1->phasor), out);
}

lcr_status lcr_compensate(lcr_complex zm, lcr_complex zs,
                          lcr_complex yo, lcr_complex *out) {
    if (!finite_complex(zm) || !finite_complex(zs) || !finite_complex(yo)) return LCR_BAD_INPUT;
    const lcr_complex delta = {zm.re - zs.re, zm.im - zs.im};
    const lcr_complex product = multiply(yo, delta);
    const lcr_complex denominator = {1.0 - product.re, -product.im};
    if (hypot(denominator.re, denominator.im) < 1e-9) return LCR_SINGULAR;
    return divide(delta, denominator, out);
}
