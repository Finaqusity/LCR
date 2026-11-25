#include "lcr_core.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static uint16_t a[8192], b[8192], interleaved[16384];

static void waveform(uint16_t *buffer, unsigned count, double omega,
                     double re, double im, double dc) {
    for (unsigned n = 0u; n < count; ++n) {
        double value = dc + re * cos(n * omega) - im * sin(n * omega);
        if (value < 0.0) value = 0.0;
        if (value > 4095.0) value = 4095.0;
        buffer[n] = (uint16_t)(value + 0.5);
    }
}

int main(void) {
    for (unsigned k = 0u; k < 4u; ++k) {
        const lcr_frequency *f = &lcr_frequencies[k];
        const double omega = lcr_sample_omega(f);
        lcr_fit v1, v2;
        lcr_complex z, alpha = {1.0, 0.0}, zf = {1000.0, 0.0};
        /* Independent circuit: Z=500-j600, V1=600; V2=-1000*V1/Z. */
        waveform(a, f->sample_pairs, omega, 600.0, 0.0, 2100.0);
        waveform(b, f->sample_pairs, omega, -600000.0 * 500.0 / 610000.0,
                 -600000.0 * 600.0 / 610000.0, 1900.0);
        assert(lcr_fit_adc(a, f->sample_pairs, 1u, omega, &v1) == LCR_OK);
        assert(lcr_fit_adc(b, f->sample_pairs, 1u, omega, &v2) == LCR_OK);
        for (unsigned n = 0u; n < f->sample_pairs; ++n) {
            interleaved[2u * n] = a[n];
            interleaved[2u * n + 1u] = b[n];
        }
        lcr_fit packed1, packed2;
        assert(lcr_fit_adc(interleaved, f->sample_pairs, 2u, omega, &packed1) == LCR_OK);
        assert(lcr_fit_adc(interleaved + 1u, f->sample_pairs, 2u, omega, &packed2) == LCR_OK);
        assert(hypot(packed1.phasor.re - v1.phasor.re, packed1.phasor.im - v1.phasor.im) < 1e-12);
        assert(hypot(packed2.phasor.re - v2.phasor.re, packed2.phasor.im - v2.phasor.im) < 1e-12);
        assert(lcr_impedance(&v1, &v2, zf, alpha, &z) == LCR_OK);
        assert(hypot(z.re - 500.0, z.im + 600.0) / hypot(500.0, 600.0) < 0.005);
        /* Calibration convention independently tested using exact phasors. */
        v1.phasor.re = 620.0; v1.phasor.im = 40.0;
        v2.phasor.re = -600000.0 * 500.0 / 610000.0;
        v2.phasor.im = -600000.0 * 600.0 / 610000.0;
        alpha.re = 620.0 / 600.0; alpha.im = 40.0 / 600.0;
        assert(lcr_impedance(&v1, &v2, zf, alpha, &z) == LCR_OK);
        assert(hypot(z.re - 500.0, z.im + 600.0) < 1e-9);
        v2.clipped = 1u;
        assert(lcr_impedance(&v1, &v2, zf, alpha, &z) == LCR_CLIPPED);
        v2.clipped = 0u; v2.phasor.re = 0.0; v2.phasor.im = 0.0;
        assert(lcr_impedance(&v1, &v2, zf, alpha, &z) == LCR_WEAK_SIGNAL);
        assert(lcr_open_admittance(&v1, &v2, zf, alpha, &z) == LCR_OK);
        assert(z.re == 0.0 && z.im == 0.0);
        printf("frequency %u: OK\n", (unsigned)f->nominal_hz);
    }
    lcr_complex result, one = {1.0, 0.0}, zero = {0.0, 0.0};
    assert(lcr_compensate(one, zero, one, &result) == LCR_SINGULAR);
    assert(lcr_feedback(1000.0, 0.0, 1000.0, &result) == LCR_OK);
    assert(fabs(result.re - 1000.0) < 1e-9 && result.im == 0.0);
    assert(lcr_fit_adc(a, 4u, 1u, 0.4, 0) == LCR_BAD_INPUT);
    assert(lcr_dds_word(0u) == 0u && lcr_dds_word(4000000u) == 0u);
    assert(lcr_dds_word(100u) == 3355u);
    assert(lcr_dds_word(1000u) == 33554u);
    assert(lcr_dds_word(10000u) == 335544u);
    assert(lcr_dds_word(100000u) == 3355443u);
    lcr_fit fitted;
    assert(lcr_fit_adc(a, 16u, 0u, 0.4, &fitted) == LCR_BAD_INPUT);
    assert(lcr_fit_adc(a, 16u, SIZE_MAX, 0.4, &fitted) == LCR_BAD_INPUT);
    assert(lcr_fit_adc(a, 16u, 1u, NAN, &fitted) == LCR_BAD_INPUT);
    assert(lcr_fit_adc(a, 16u, 1u, 0.0, &fitted) == LCR_BAD_INPUT);
    a[0] = 4096u;
    assert(lcr_fit_adc(a, 16u, 1u, 0.4, &fitted) == LCR_BAD_INPUT);
    /* Noninteger cycles with independent DC and initial phase. */
    waveform(a, 137u, 0.400202885807617, 500.0, 200.0, 1700.0);
    assert(lcr_fit_adc(a, 137u, 1u, 0.400202885807617, &fitted) == LCR_OK);
    assert(hypot(fitted.phasor.re - 500.0, fitted.phasor.im - 200.0) < 0.15);
    /* Exact feedback and fixture networks use independent circuit equations. */
    const double wc = 6.2831853071795864769 * 100000.0 * 2.2e-12;
    assert(lcr_feedback(1e6, 2.2e-12, 100000.0, &result) == LCR_OK);
    const double norm = 1e-12 + wc * wc;
    assert(hypot(result.re - 1e-6 / norm, result.im + wc / norm) < 1e-8);
    const lcr_complex zs = {0.12, 0.02}, yo = {1e-9, 7.539822368615504e-7};
    /* DUT=500-j600; total parallel Y = 1/Zdut + Yo. */
    const double yr = 500.0 / 610000.0 + yo.re;
    const double yi = 600.0 / 610000.0 + yo.im;
    const lcr_complex measured = {zs.re + yr / (yr * yr + yi * yi),
                                 zs.im - yi / (yr * yr + yi * yi)};
    assert(lcr_compensate(measured, zs, yo, &result) == LCR_OK);
    assert(hypot(result.re - 500.0, result.im + 600.0) < 1e-9);
    assert(lcr_feedback(0.0, 0.0, 1000.0, &result) == LCR_BAD_INPUT);
    assert(lcr_compensate(one, zero, one, 0) != LCR_OK);
    puts("lcr_core: all C tests passed");
    return 0;
}

