#include "lcr_measure.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const double tau = 6.283185307179586476925286766559;
static uint16_t samples[2400];

static lcr_complex mul(lcr_complex a, lcr_complex b) {
    const lcr_complex z = {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re};
    return z;
}
static lcr_complex inv(lcr_complex a) {
    const double norm = a.re * a.re + a.im * a.im;
    const lcr_complex z = {a.re / norm, -a.im / norm};
    return z;
}
static uint16_t code(double value) {
    if (value < 0.0) return 0u;
    if (value > 4095.0) return 4095u;
    return (uint16_t)(value + 0.5);
}

static void circuit(unsigned frequency, lcr_complex dut, const lcr_calibration *cal) {
    const lcr_frequency *f = &lcr_frequencies[frequency];
    const double hz = 8000000.0 * lcr_dds_word(f->nominal_hz) / 268435456.0;
    /* Independent forward circuit: Zm = Zs + 1/(1/Zdut + Yo). */
    lcr_complex y = inv(dut);
    y.re += cal->yo.re; y.im += cal->yo.im;
    lcr_complex zm = inv(y);
    zm.re += cal->zs.re; zm.im += cal->zs.im;
    const lcr_complex yf = {1.0 / cal->rf_ohm, tau * hz * cal->cf_f};
    const lcr_complex zf = inv(yf), physical_v1 = {600.0, 80.0};
    lcr_complex v2 = mul(mul(zf, physical_v1), inv(zm));
    v2.re = -v2.re; v2.im = -v2.im;
    const lcr_complex v1 = mul(cal->alpha, physical_v1);
    /* Direct trigonometry deliberately differs from production recurrence. */
    const double omega = tau * hz / (84000000.0 / f->timer_divisor);
    for (unsigned n = 0u; n < f->sample_pairs; ++n) {
        samples[2u * n] = code(2100.0 + v1.re * cos(n * omega) - v1.im * sin(n * omega));
        samples[2u * n + 1u] = code(1900.0 + v2.re * cos(n * omega) - v2.im * sin(n * omega));
    }
}

static void check_parameters(void) {
    lcr_parameters p;
    const lcr_complex r = {1234.0, 0.0}, l = {4.0, 30.0}, c = {4.0, -30.0};
    assert(lcr_parameters_from_z(r, 1000.0, &p) == LCR_OK);
    assert(p.kind == LCR_COMPONENT_R && p.rp_ohm == 1234.0 && p.q == 0.0);
    assert(!(p.available & (LCR_PARAM_LS | LCR_PARAM_CS | LCR_PARAM_D)));
    assert(lcr_parameters_from_z(l, 1000.0, &p) == LCR_OK);
    assert(p.kind == LCR_COMPONENT_L);
    assert(fabs(p.ls_h - 30.0 / (tau * 1000.0)) < 1e-15);
    assert(fabs(p.lp_h - 916.0 / (tau * 1000.0 * 30.0)) < 1e-15);
    assert(fabs(p.rp_ohm - 229.0) < 1e-12 && fabs(p.q - 7.5) < 1e-12);
    assert(fabs(p.d - 4.0 / 30.0) < 1e-15);
    assert(lcr_parameters_from_z(c, 1000.0, &p) == LCR_OK);
    assert(p.kind == LCR_COMPONENT_C);
    assert(fabs(p.cs_f - 1.0 / (tau * 1000.0 * 30.0)) < 1e-15);
    assert(fabs(p.cp_f - 30.0 / (tau * 1000.0 * 916.0)) < 1e-15);
    const lcr_complex ideal_l = {0.0, 10.0}, negative = {-4.0, 30.0}, zero = {0.0, 0.0};
    assert(lcr_parameters_from_z(ideal_l, 1000.0, &p) == LCR_OK);
    assert(p.kind == LCR_COMPONENT_L && !(p.available & LCR_PARAM_Q) && p.d == 0.0);
    assert(lcr_parameters_from_z(negative, 1000.0, &p) == LCR_OK);
    assert(p.kind == LCR_COMPONENT_UNKNOWN && !(p.available & (LCR_PARAM_Q | LCR_PARAM_D | LCR_PARAM_RP)));
    const lcr_complex unresolved_esr = {-0.01, -30.0};
    assert(lcr_parameters_from_z(unresolved_esr, 1000.0, &p) == LCR_OK);
    assert(p.kind == LCR_COMPONENT_C && p.negative_resistance);
    assert(!(p.available & (LCR_PARAM_Q | LCR_PARAM_D | LCR_PARAM_RP)) && p.rs_ohm == -0.01);
    assert(lcr_parameters_from_z(zero, 1000.0, &p) == LCR_BAD_INPUT);
    assert(p.available == 0u);
    assert(lcr_parameters_from_z(r, NAN, &p) == LCR_BAD_INPUT);
}

int main(void) {
    check_parameters();
    unsigned cases = 0u;
    for (unsigned f = 0u; f < LCR_FREQUENCY_COUNT; ++f) {
        for (unsigned range = 0u; range < LCR_RANGE_COUNT; ++range) {
            lcr_calibration cal;
            lcr_calibration_nominal(&cal, f, range);
            const lcr_complex z = {lcr_nominal_rf[range], 0.0};
            const lcr_measure_config config = {f, range, 8000000.0, 0};
            lcr_measurement result;
            circuit(f, z, &cal);
            assert(lcr_measure_frame(samples, lcr_frequencies[f].sample_pairs, &config, &result) == LCR_OK);
            assert(result.valid && result.calibration_flags == 0u);
            assert(result.parameters.kind == LCR_COMPONENT_R);
            assert(hypot(result.corrected_z.re - z.re, result.corrected_z.im) / z.re < 0.003);
            ++cases;
        }
        const double hz = 8000000.0 * lcr_dds_word(lcr_frequencies[f].nominal_hz) / 268435456.0;
        const double capacitance[4] = {10e-6, 1e-6, 10e-9, 10e-12};
        const double inductance[4] = {100e-3, 10e-3, 1e-3, 10e-6};
        const lcr_complex components[2] = {{0.3, -1.0 / (tau * hz * capacitance[f])},
                                          {0.3, tau * hz * inductance[f]}};
        for (unsigned kind = 0u; kind < 2u; ++kind) {
            const lcr_complex z = components[kind];
            unsigned range = 0u;
            while (range < 5u && hypot(z.re, z.im) > 3.0 * lcr_nominal_rf[range]) ++range;
            lcr_calibration cal;
            lcr_calibration_nominal(&cal, f, range);
            const lcr_measure_config config = {f, range, 8000000.0, &cal};
            lcr_measurement result;
            circuit(f, z, &cal);
            assert(lcr_measure_frame(samples, lcr_frequencies[f].sample_pairs, &config, &result) == LCR_OK);
            if (result.parameters.kind != (kind ? LCR_COMPONENT_L : LCR_COMPONENT_C))
                fprintf(stderr, "component f=%u kind=%u measured Z=%.12g%+.12gj\n",
                        f, kind, result.corrected_z.re, result.corrected_z.im);
            assert(result.parameters.kind == (kind ? LCR_COMPONENT_L : LCR_COMPONENT_C));
            const double measured = kind ? result.parameters.ls_h : result.parameters.cs_f;
            const double expected = kind ? inductance[f] : capacitance[f];
            assert(fabs(measured / expected - 1.0) < 0.005);
            ++cases;
        }
    }
    lcr_calibration cal;
    lcr_calibration_nominal(&cal, 3u, 2u);
    cal.flags = LCR_CAL_ALL; cal.revision = 42u;
    cal.alpha.re = 1.04 * cos(0.04); cal.alpha.im = 1.04 * sin(0.04);
    cal.rf_ohm = 1002.0; cal.cf_f = 2.2e-12;
    cal.zs.re = 0.12; cal.zs.im = 0.02;
    cal.yo.re = 1e-9; cal.yo.im = tau * 100000.0 * 1.2e-12;
    const lcr_complex dut = {500.0, -600.0};
    lcr_measure_config config = {3u, 2u, 8000000.0, &cal};
    lcr_measurement result;
    circuit(3u, dut, &cal);
    assert(lcr_measure_frame(samples, 1200u, &config, &result) == LCR_OK);
    assert(result.calibration_flags == LCR_CAL_ALL && result.calibration_revision == 42u);
    assert(hypot(result.corrected_z.re - 500.0, result.corrected_z.im + 600.0) / hypot(500.0, 600.0) < 0.003);
    const double nominal_hz = result.actual_hz, nominal_c = result.parameters.cs_f;
    config.hse_hz = 8000160.0;
    assert(lcr_measure_frame(samples, 1200u, &config, &result) == LCR_OK);
    assert(fabs(result.actual_hz / nominal_hz - 1.00002) < 1e-12);
    /* No-calibration path isolates conversion from frequency-dependent correction. */
    config.calibration = 0;
    config.hse_hz = 8000000.0;
    assert(lcr_measure_frame(samples, 1200u, &config, &result) == LCR_OK);
    const double c0 = result.parameters.cs_f;
    config.hse_hz = 8000160.0;
    assert(lcr_measure_frame(samples, 1200u, &config, &result) == LCR_OK);
    assert(fabs(result.parameters.cs_f / c0 - 1.0 / 1.00002) < 1e-12);
    assert(nominal_c > 0.0);
    config.calibration = &cal;
    assert(lcr_measure_frame(samples, 1199u, &config, &result) == LCR_BAD_INPUT && !result.valid);
    cal.range_index = 3u;
    assert(lcr_measure_frame(samples, 1200u, &config, &result) == LCR_BAD_INPUT && !result.valid);
    cal.range_index = 2u; cal.alpha.re = NAN;
    assert(lcr_measure_frame(samples, 1200u, &config, &result) == LCR_BAD_INPUT);
    cal.flags = 0u; /* Unflagged coefficients are deliberately ignored. */
    assert(lcr_measure_frame(samples, 1200u, &config, &result) == LCR_OK);
    cal.flags = 8u;
    assert(lcr_measure_frame(samples, 1200u, &config, &result) == LCR_BAD_INPUT);
    printf("lcr_measure: %u R/L/C frames, equivalent models, calibration and invalidation passed\n", cases);
    return 0;
}
