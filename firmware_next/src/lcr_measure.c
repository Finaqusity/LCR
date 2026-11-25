#include "lcr_measure.h"
#include "lcr_config.h"

#include <math.h>
#include <string.h>

static const double tau = 6.283185307179586476925286766559;
const double lcr_nominal_rf[LCR_RANGE_COUNT] = {10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0};

static int finite_complex(lcr_complex z) {
    return isfinite(z.re) && isfinite(z.im);
}

void lcr_calibration_nominal(lcr_calibration *out, unsigned frequency, unsigned range) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->frequency_index = frequency;
    out->range_index = range;
    out->alpha.re = 1.0;
    if (range < LCR_RANGE_COUNT) out->rf_ohm = lcr_nominal_rf[range];
}

lcr_status lcr_measure_config_check(const lcr_measure_config *config) {
    if (!config || config->frequency_index >= LCR_FREQUENCY_COUNT ||
        config->range_index >= LCR_RANGE_COUNT || !isfinite(config->hse_hz) ||
        config->hse_hz < 7900000.0 || config->hse_hz > 8100000.0) return LCR_BAD_INPUT;
    const lcr_calibration *cal = config->calibration;
    if (!cal) return LCR_OK;
    if (cal->frequency_index != config->frequency_index || cal->range_index != config->range_index ||
        (cal->flags & ~(uint32_t)LCR_CAL_ALL)) return LCR_BAD_INPUT;
    if ((cal->flags & LCR_CAL_CHANNEL) && (!finite_complex(cal->alpha) ||
        hypot(cal->alpha.re, cal->alpha.im) < 1e-12)) return LCR_BAD_INPUT;
    if ((cal->flags & LCR_CAL_FEEDBACK) && (!isfinite(cal->rf_ohm) ||
        !isfinite(cal->cf_f) || cal->rf_ohm <= 0.0 || cal->cf_f < 0.0)) return LCR_BAD_INPUT;
    if ((cal->flags & LCR_CAL_FIXTURE) &&
        (!finite_complex(cal->zs) || !finite_complex(cal->yo))) return LCR_BAD_INPUT;
    return LCR_OK;
}

lcr_status lcr_parameters_from_z(lcr_complex z, double hz, lcr_parameters *out) {
    if (!out) return LCR_BAD_INPUT;
    memset(out, 0, sizeof(*out));
    if (!finite_complex(z) || !isfinite(hz) || hz <= 0.0) return LCR_BAD_INPUT;
    const double magnitude = hypot(z.re, z.im), omega = tau * hz;
    const double norm = magnitude * magnitude;
    if (!isfinite(magnitude) || !isfinite(norm) || !isfinite(omega) ||
        magnitude < 1e-15 || omega <= 0.0) return LCR_BAD_INPUT;
    lcr_parameters p;
    memset(&p, 0, sizeof(p));
    p.rs_ohm = z.re;
    p.xs_ohm = z.im;
    p.magnitude_ohm = magnitude;
    p.phase_deg = atan2(z.im, z.re) * 360.0 / tau;
    p.negative_resistance = z.re < 0.0;
    /* Provisional phase heuristic. Small negative ESR can be below resolution;
       retain its sign, omit passive loss parameters, but still describe L/C. */
    if (z.re >= -0.02 * magnitude) {
        if (z.re >= 0.0 && fabs(z.im) <= 0.02 * z.re) p.kind = LCR_COMPONENT_R;
        else p.kind = z.im > 0.0 ? LCR_COMPONENT_L : LCR_COMPONENT_C;
    }
    if (z.im > 0.0) {
        p.ls_h = z.im / omega;
        p.lp_h = norm / z.im / omega;
        p.available |= LCR_PARAM_LS | LCR_PARAM_LP;
    } else if (z.im < 0.0) {
        p.cs_f = -(1.0 / z.im) / omega;
        p.cp_f = -z.im / norm / omega;
        p.available |= LCR_PARAM_CS | LCR_PARAM_CP;
    }
    if (z.re > 0.0) {
        p.rp_ohm = norm / z.re;
        p.q = fabs(z.im) / z.re;
        p.available |= LCR_PARAM_RP | LCR_PARAM_Q;
    }
    if (z.re >= 0.0 && z.im != 0.0) {
        p.d = z.re / fabs(z.im);
        p.available |= LCR_PARAM_D;
    }
    if (!isfinite(p.ls_h) || !isfinite(p.lp_h) || !isfinite(p.cs_f) ||
        !isfinite(p.cp_f) || !isfinite(p.rp_ohm) || !isfinite(p.q) || !isfinite(p.d))
        return LCR_BAD_INPUT;
    *out = p;
    return LCR_OK;
}

static lcr_status process(const uint16_t *pairs, size_t count,
                          const lcr_measure_config *config, lcr_measurement *out) {
    lcr_status status = lcr_measure_config_check(config);
    if (status != LCR_OK || !pairs) return LCR_BAD_INPUT;
    const lcr_frequency *frequency = &lcr_frequencies[config->frequency_index];
    if (count < 8u || count > frequency->sample_pairs) return LCR_BAD_INPUT;
    out->frequency_index = config->frequency_index;
    out->range_index = config->range_index;
    out->actual_hz = config->hse_hz * lcr_dds_word(frequency->nominal_hz) / 268435456.0;
    const double omega = lcr_sample_omega(frequency);
    status = lcr_fit_adc(pairs, count, 2u, omega, &out->v1);
    if (status != LCR_OK) return status;
    status = lcr_fit_adc(pairs + 1, count, 2u, omega, &out->v2);
    if (status != LCR_OK) return status;
    /* V1 monitors the source: with no excitation there is nothing to show. */
    if (hypot(out->v1.phasor.re, out->v1.phasor.im) < LCR_EXCITATION_MIN_PEAK_CODES)
        return LCR_NO_EXCITATION;
    double rf = lcr_nominal_rf[config->range_index], cf = 0.0;
    lcr_complex alpha = {1.0, 0.0}, zf;
    const lcr_calibration *cal = config->calibration;
    if (cal) {
        out->calibration_flags = cal->flags;
        out->calibration_revision = cal->revision;
        if (cal->flags & LCR_CAL_CHANNEL) alpha = cal->alpha;
        if (cal->flags & LCR_CAL_FEEDBACK) { rf = cal->rf_ohm; cf = cal->cf_f; }
    }
    status = lcr_feedback(rf, cf, out->actual_hz, &zf);
    if (status != LCR_OK) return status;
    status = lcr_impedance_ex(&out->v1, &out->v2, zf, alpha, 1, &out->raw_z);
    if (status != LCR_OK && status != LCR_WEAK_SIGNAL) return status;
    const lcr_status signal_status = status; /* LCR_OK or LCR_WEAK_SIGNAL */
    out->corrected_z = out->raw_z;
    if (cal && (cal->flags & LCR_CAL_FIXTURE)) {
        const lcr_status compensated = lcr_compensate(out->raw_z, cal->zs, cal->yo, &out->corrected_z);
        if (compensated != LCR_OK) return compensated;
    }
    const lcr_status parameter_status =
        lcr_parameters_from_z(out->corrected_z, out->actual_hz, &out->parameters);
    if (parameter_status != LCR_OK) return parameter_status;
    return signal_status;
}

lcr_status lcr_measure_frame(const uint16_t *pairs, size_t count,
                            const lcr_measure_config *config, lcr_measurement *out) {
    if (!out) return LCR_BAD_INPUT;
    memset(out, 0, sizeof(*out));
    out->status = process(pairs, count, config, out);
    out->valid = out->status == LCR_OK || out->status == LCR_WEAK_SIGNAL;
    return out->status;
}

static int usable_fit(const lcr_fit *fit) {
    return finite_complex(fit->phasor) && isfinite(fit->dc) &&
           fit->dc > 8.0 && fit->dc < 4087.0 && isfinite(fit->residual_rms) &&
           fit->residual_rms >= 0.0;
}

lcr_range_choice lcr_choose_range(const lcr_measurement *frame, uint8_t visited) {
    lcr_range_choice choice = {LCR_RANGE_BAD_FRAME, 0u};
    if (!frame || frame->range_index >= LCR_RANGE_COUNT) return choice;
    choice.next_range = frame->range_index;
    if (frame->status != LCR_OK && frame->status != LCR_CLIPPED &&
        frame->status != LCR_WEAK_SIGNAL) return choice;
    if (!usable_fit(&frame->v1) || !usable_fit(&frame->v2) || frame->v1.clipped) return choice;
    const double amplitude1 = hypot(frame->v1.phasor.re, frame->v1.phasor.im);
    const double amplitude2 = hypot(frame->v2.phasor.re, frame->v2.phasor.im);
    if (amplitude1 < 3.0 || amplitude1 < 14.14213562373095 * frame->v1.residual_rms) return choice;
    const double headroom = fmin(frame->v2.dc - 32.0, 4063.0 - frame->v2.dc);
    const double ceiling = fmin(1400.0, 0.85 * headroom);
    int direction = 0;
    if (frame->v2.clipped || amplitude2 > ceiling) direction = -1;
    else if (amplitude2 < 80.0) direction = 1;
    else if (amplitude2 < 14.14213562373095 * frame->v2.residual_rms) return choice;
    else if (frame->valid) {
        choice.action = LCR_RANGE_ACCEPT;
        return choice;
    } else return choice;
    const int next = (int)frame->range_index + direction;
    choice.action = LCR_RANGE_LIMIT;
    if (next < 0 || next >= (int)LCR_RANGE_COUNT || (visited & (1u << (unsigned)next))) return choice;
    choice.action = LCR_RANGE_CHANGE;
    choice.next_range = (unsigned)next;
    return choice;
}

