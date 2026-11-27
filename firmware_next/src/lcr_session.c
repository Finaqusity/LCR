#include "lcr_session.h"
#include <stdio.h>

#include <string.h>
#include <math.h>
#include "lcr_config.h"

static uint32_t next_token(lcr_session *s) {
    ++s->token_counter;
    if (!s->token_counter) ++s->token_counter;
    return s->token_counter;
}

static void finish(lcr_session *s, lcr_session_error error, lcr_session_state terminal) {
    s->error = error;
    s->active_token = next_token(s);
    s->io.capture_cancel(s->io.context);
    if (!(terminal == LCR_SESSION_DONE && error == LCR_SESSION_NO_ERROR &&
          s->keep_excitation)) lcr_frontend_stop(&s->frontend);
    if (s->frontend.state == LCR_FE_FAULT) {
        s->error = LCR_SESSION_FRONTEND_ERROR;
        terminal = LCR_SESSION_FAILED;
    }
    if (s->error != LCR_SESSION_NO_ERROR) s->result.valid = 0u;
    s->terminal = terminal;
    s->state = (s->frontend.state == LCR_FE_BREAK || s->frontend.state == LCR_FE_MAKE)
                   ? LCR_SESSION_STOPPING : terminal;
}

static int begin_range(lcr_session *s, unsigned range, uint32_t now) {
    s->settling=s->adaptive_settling && s->kind==LCR_CAPTURE_MEASURE;
    s->stable_count=0u;s->have_previous=0u;s->settle_since=now;s->settle_next=now;
    s->frontend.settle_ms=s->settling?LCR_MIN_SETTLE_MS:LCR_ANALOG_SETTLE_MS;
    s->config.range_index = range;
    s->config.calibration = 0;
    if (s->io.calibration) {
        const lcr_calibration *slot = s->io.calibration(s->io.context, s->config.frequency_index, range);
        if (slot) {
            s->calibration_snapshot = *slot;
            s->config.calibration = &s->calibration_snapshot;
        }
    }
    if (lcr_measure_config_check(&s->config) != LCR_OK) {
        finish(s, LCR_SESSION_CONFIG_ERROR, LCR_SESSION_FAILED);
        return 0;
    }
    lcr_frontend_stop(&s->frontend);
    if (s->frontend.state == LCR_FE_FAULT) {
        finish(s, LCR_SESSION_FRONTEND_ERROR, LCR_SESSION_FAILED);
        return 0;
    }
    if (!s->io.frequency(s->io.context, lcr_frequencies[s->config.frequency_index].nominal_hz)) {
        finish(s, LCR_SESSION_FRONTEND_ERROR, LCR_SESSION_FAILED);
        return 0;
    }
    if (!lcr_frontend_begin(&s->frontend, range, s->kind == LCR_CAPTURE_CHANNEL, now)) {
        finish(s, LCR_SESSION_FRONTEND_ERROR, LCR_SESSION_FAILED);
        return 0;
    }
    s->visited |= (uint8_t)(1u << range);
    s->state = LCR_SESSION_SWITCHING;
    return 1;
}

int lcr_session_init(lcr_session *s, lcr_session_io io) {
    if (!s || !io.frequency || !io.capture || !io.capture_cancel ||
        !io.frontend.mute || !io.frontend.relays) return 0;
    memset(s, 0, sizeof(*s));
    s->io = io;
    if (!lcr_frontend_init(&s->frontend, io.frontend)) {
        s->state = LCR_SESSION_FAILED;
        s->error = LCR_SESSION_FRONTEND_ERROR;
        return 0;
    }
    return 1;
}

static int start(lcr_session *s, unsigned frequency, unsigned initial_range,
                  int autorange, double hse_hz, uint32_t now, lcr_capture_kind kind) {
    if (!s || s->state == LCR_SESSION_SWITCHING || s->state == LCR_SESSION_CAPTURING ||
        s->state == LCR_SESSION_STOPPING || s->frontend.state == LCR_FE_FAULT) return 0;
    const lcr_measure_config config = {frequency, initial_range, hse_hz, 0};
    if ((autorange != 0 && autorange != 1) || lcr_measure_config_check(&config) != LCR_OK) return 0;
    /* Reuse the warmed-up frontend when the same acquisition is repeated:
       K-frame measurement averaging and the many-sample calibration sweep.
       Only a frequency, range or capture-kind change pays the settling. */
    const int reuse = s->kind == kind && s->keep_excitation &&
                      s->frontend.state == LCR_FE_READY &&
                      s->config.frequency_index == frequency && s->frontend.range == initial_range;
    s->kind = kind;
    s->config = config;
    s->visited = 0u;
    s->autorange = (unsigned)autorange;
    s->capture_count = 0u;
    s->error = LCR_SESSION_NO_ERROR;
    memset(&s->result, 0, sizeof(s->result));
    if (reuse) {
        s->settling=0u;
        if (s->io.calibration) {
            const lcr_calibration *slot = s->io.calibration(s->io.context, frequency, initial_range);
            if (slot) { s->calibration_snapshot = *slot; s->config.calibration = &s->calibration_snapshot; }
        }
        if (lcr_measure_config_check(&s->config) != LCR_OK) {
            finish(s, LCR_SESSION_CONFIG_ERROR, LCR_SESSION_FAILED); return 0;
        }
        s->visited = (uint8_t)(1u << initial_range);
        s->state = LCR_SESSION_SWITCHING;
        return 1;
    }
    return begin_range(s, initial_range, now);
}

int lcr_session_start(lcr_session *s, unsigned frequency, unsigned range, int autorange, double hse_hz, uint32_t now) {
    return start(s, frequency, range, autorange, hse_hz, now, LCR_CAPTURE_MEASURE);
}
int lcr_session_calibrate(lcr_session *s, unsigned frequency, unsigned range,
                          lcr_capture_kind kind, double hse_hz, uint32_t now) {
    if (kind < LCR_CAPTURE_CHANNEL || kind > LCR_CAPTURE_STANDARD) return 0;
    return start(s, frequency, range, 0, hse_hz, now, kind);
}

static lcr_status calibration_frame(lcr_session *s, const uint16_t *pairs, size_t count) {
    lcr_measurement *m = &s->result;
    memset(m, 0, sizeof(*m));
    if (!pairs || count == 0u || count > LCR_CAPTURE_PAIRS_MAX) return LCR_BAD_INPUT;
    const lcr_frequency *f = &lcr_frequencies[s->config.frequency_index];
    m->frequency_index = s->config.frequency_index; m->range_index = s->config.range_index;
    m->actual_hz = s->config.hse_hz * lcr_dds_word(f->nominal_hz) / 268435456.0;
    lcr_status status = lcr_fit_adc(pairs, count, 2u, lcr_sample_omega(f), &m->v1);
    if (status != LCR_OK) return status;
    if (hypot(m->v1.phasor.re, m->v1.phasor.im) < LCR_EXCITATION_MIN_PEAK_CODES)
        return LCR_NO_EXCITATION;
    status = lcr_fit_adc(pairs + 1u, count, 2u, lcr_sample_omega(f), &m->v2);
    if (status != LCR_OK) return status;
    const lcr_complex one = {1.0,0.0}, negative_one = {-1.0,0.0};
    if (s->kind == LCR_CAPTURE_CHANNEL || s->kind == LCR_CAPTURE_STANDARD)
        /* The 10 ohm branch has very little V2 on the low band; let a weak
           frame through so the standard sweep can still average it (the
           channel sweep keeps the strict gate). */
        return lcr_impedance_ex(&m->v1, &m->v2, negative_one, one,
                                (uint8_t)(s->kind == LCR_CAPTURE_STANDARD),
                                &m->raw_z);
    const lcr_calibration *cal = s->config.calibration;
    double rf = lcr_nominal_rf[s->config.range_index], cf = 0.0;
    lcr_complex alpha = one, zf;
    if (cal && (cal->flags & LCR_CAL_CHANNEL)) alpha = cal->alpha;
    else return LCR_BAD_INPUT; /* Open workflow requires channel calibration first. */
    if (cal->flags & LCR_CAL_FEEDBACK) { rf = cal->rf_ohm; cf = cal->cf_f; }
    status = lcr_feedback(rf, cf, m->actual_hz, &zf);
    if (status != LCR_OK) return status;
    return lcr_open_admittance(&m->v1, &m->v2, zf, alpha, &m->raw_z);
}

void lcr_session_poll(lcr_session *s, uint32_t now) {
    if (!s) return;
    if(s->settling && (s->state==LCR_SESSION_SWITCHING || s->state==LCR_SESSION_CAPTURING) &&
       now-s->settle_since>=LCR_STABILITY_TIMEOUT_MS) {
        finish(s,LCR_SESSION_UNSTABLE,LCR_SESSION_FAILED);return;
    }
    if (s->state == LCR_SESSION_SWITCHING || s->state == LCR_SESSION_STOPPING) {
        lcr_frontend_poll(&s->frontend, now);
        if (s->frontend.state == LCR_FE_FAULT) {
            finish(s, LCR_SESSION_FRONTEND_ERROR, LCR_SESSION_FAILED);
            return;
        }
        if (s->state == LCR_SESSION_STOPPING) {
            if (s->frontend.state == LCR_FE_MUTED) s->state = s->terminal;
            return;
        }
        if (lcr_frontend_ready(&s->frontend)) {
            if(s->settling && (int32_t)(now-s->settle_next)<0)return;
            const lcr_frequency *f = &lcr_frequencies[s->config.frequency_index];
            size_t capture_pairs = f->sample_pairs;
            /* Calibration is not time limited: give the low bands a longer
               frame so the per-sample channel ratio is not dominated by the
               80 ms capture noise at 100 Hz.  Measurements keep the table
               window (the 1 s measurement budget). */
            if (s->kind != LCR_CAPTURE_MEASURE && capture_pairs < LCR_CAL_MIN_PAIRS)
                capture_pairs = LCR_CAL_MIN_PAIRS;
            if (s->settling && capture_pairs > 256u) capture_pairs = 256u;
            s->capture_timeout = 100u + ((uint32_t)capture_pairs * f->timer_divisor + 83999u) / 84000u;
            s->capture_since = now;
            s->active_token = next_token(s);
            s->state = LCR_SESSION_CAPTURING;
            ++s->capture_count;
            if (!s->io.capture(s->io.context, s->config.frequency_index, s->config.range_index,
                                capture_pairs, s->active_token))
                finish(s, LCR_SESSION_CAPTURE_ERROR, LCR_SESSION_FAILED);
        }
    } else if (s->state == LCR_SESSION_CAPTURING && now - s->capture_since >= s->capture_timeout) {
        finish(s, LCR_SESSION_TIMEOUT, LCR_SESSION_FAILED);
    }
}

static int stable(const lcr_measurement *a,const lcr_measurement *b) {
    /* Settling is judged on the source amplitude and the channel bias only.
       V2 carries the DUT current; its frame-to-frame noise is measurement
       noise that the complex-ratio averaging over K frames removes later. */
    const double v1=hypot(a->v1.phasor.re,a->v1.phasor.im);
    const double p1=hypot(b->v1.phasor.re,b->v1.phasor.im);
    return fabs(a->v1.dc-b->v1.dc)<LCR_STABILITY_DC_CODES &&
           fabs(v1-p1)<LCR_STABILITY_RELATIVE*fmax(v1,3.0);
}

int lcr_session_submit(lcr_session *s, uint32_t token,
                       const uint16_t *pairs, size_t count, uint32_t now) {
    if (!s || s->state != LCR_SESSION_CAPTURING || token != s->active_token) return 0;
    if(s->settling && now-s->settle_since>=LCR_STABILITY_TIMEOUT_MS) {
        finish(s,LCR_SESSION_UNSTABLE,LCR_SESSION_FAILED);return 0;
    }
    if (now - s->capture_since >= s->capture_timeout) {
        finish(s, LCR_SESSION_TIMEOUT, LCR_SESSION_FAILED);
        return 0;
    }
    s->io.capture_cancel(s->io.context);
    s->active_token = next_token(s);
    if (s->kind == LCR_CAPTURE_MEASURE) (void)lcr_measure_frame(pairs, count, &s->config, &s->result);
    else {
        s->result.status = calibration_frame(s, pairs, count);
        s->result.valid = s->result.status == LCR_OK;
    }
    if (s->result.status == LCR_NO_EXCITATION) {
        finish(s, LCR_SESSION_NO_EXCITATION, LCR_SESSION_FAILED);
        return 1;
    }
    if(s->settling) {
        const int retry=!s->result.v1.clipped && !s->result.v2.clipped &&
                        (s->result.status==LCR_OK || s->result.status==LCR_WEAK_SIGNAL);
        if(retry) {
            s->stable_count=s->result.valid && s->have_previous && stable(&s->result,&s->previous)?s->stable_count+1u:0u;
            s->previous=s->result;s->have_previous=s->result.valid;
            if(s->stable_count<2u) {
                if(now-s->settle_since>=LCR_STABILITY_TIMEOUT_MS) {
                    /* The analogue chain may never look perfectly still while the
                       excitation and I-V node keep drifting.  Rather than report a
                       hard failure, fall back to the last complete frame so the
                       measurement still produces a (weak-signal) result. */
                    if(s->have_previous && s->previous.valid) { s->result=s->previous;s->settling=0u; }
                    else { finish(s,LCR_SESSION_UNSTABLE,LCR_SESSION_FAILED);return 1; }
                } else {
                    s->result.valid=0u;
                    s->settle_next=now+LCR_STABILITY_INTERVAL_MS;s->state=LCR_SESSION_SWITCHING;
                    return 1;
                }
            } else {
                s->settling=0u;
            }
        }
    }
    if (s->autorange) {
        const lcr_range_choice choice = lcr_choose_range(&s->result, s->visited);
        if (choice.action == LCR_RANGE_CHANGE) {
            s->result.valid = 0u; /* Intermediate frames are diagnostics only. */
            (void)begin_range(s, choice.next_range, now);
            return 1;
        }
        if (choice.action != LCR_RANGE_ACCEPT) {
            finish(s, choice.action == LCR_RANGE_LIMIT ? LCR_SESSION_RANGE_LIMIT : LCR_SESSION_FRAME_ERROR,
                   LCR_SESSION_FAILED);
            return 1;
        }
    } else if (!s->result.valid) {
        finish(s, LCR_SESSION_FRAME_ERROR, LCR_SESSION_FAILED);
        return 1;
    }
    finish(s, LCR_SESSION_NO_ERROR, LCR_SESSION_DONE);
    return 1;
}

void lcr_session_stop(lcr_session *s) {
    if (!s || (s->state != LCR_SESSION_SWITCHING && s->state != LCR_SESSION_CAPTURING &&
               s->state != LCR_SESSION_STOPPING && s->frontend.state != LCR_FE_READY)) return;
    finish(s, LCR_SESSION_USER_STOP, LCR_SESSION_CANCELLED);
}
