#include "lcr_session.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int muted, active, fail_mute, fail_frequency, fail_capture, cal_enabled;
    unsigned frequency, range, starts;
    uint8_t mask;
    uint32_t token;
    lcr_complex dut;
    lcr_calibration cal;
} simulator;
static uint16_t samples[2400];

static int mute_io(void *context, int muted) {
    simulator *sim = context;
    if (sim->fail_mute) return 0;
    sim->muted = muted;
    return 1;
}
static void relay_io(void *context, uint8_t mask) {
    simulator *sim = context;
    assert(sim->muted && !sim->active);
    sim->mask = mask;
}
static int frequency_io(void *context, uint32_t hz) {
    simulator *sim = context;
    assert(sim->muted && !sim->active);
    assert(hz == 100u || hz == 1000u || hz == 10000u || hz == 100000u);
    return !sim->fail_frequency;
}
static int capture_io(void *context, unsigned frequency, unsigned range, size_t count, uint32_t token) {
    simulator *sim = context;
    assert(!sim->muted && !sim->active);
    assert(sim->mask == lcr_range_masks[range]);
    assert(count == lcr_frequencies[frequency].sample_pairs);
    sim->frequency = frequency; sim->range = range; sim->token = token;
    sim->active = 1; ++sim->starts;
    return !sim->fail_capture;
}
static void cancel_io(void *context) { ((simulator *)context)->active = 0; }
static const lcr_calibration *calibration_io(void *context, unsigned frequency, unsigned range) {
    simulator *sim = context;
    (void)frequency; (void)range;
    return sim->cal_enabled ? &sim->cal : 0;
}
static void init(simulator *sim, lcr_session *s) {
    memset(sim, 0, sizeof(*sim));
    sim->dut.re = 1000.0;
    const lcr_frontend_io frontend = {sim, mute_io, relay_io};
    const lcr_session_io io = {frontend, sim, frequency_io, capture_io, cancel_io, calibration_io};
    assert(lcr_session_init(s, io));
}

static uint16_t adc(double value) {
    if (value < 0.0) return 0u;
    if (value > 4095.0) return 4095u;
    return (uint16_t)(value + 0.5);
}
static void waveform(const simulator *sim) {
    const lcr_frequency *f = &lcr_frequencies[sim->frequency];
    const double hz = 8000000.0 * lcr_dds_word(f->nominal_hz) / 268435456.0;
    const double omega = 6.2831853071795864769 * hz * f->timer_divisor / 84000000.0;
    const double norm = sim->dut.re * sim->dut.re + sim->dut.im * sim->dut.im;
    const double re = -600.0 * lcr_nominal_rf[sim->range] * sim->dut.re / norm;
    const double im = 600.0 * lcr_nominal_rf[sim->range] * sim->dut.im / norm;
    for (unsigned n = 0u; n < f->sample_pairs; ++n) {
        samples[2u * n] = adc(2100.0 + 600.0 * cos(n * omega));
        samples[2u * n + 1u] = adc(1900.0 + re * cos(n * omega) - im * sin(n * omega));
    }
}
static void ready(lcr_session *s, simulator *sim, uint32_t *now) {
    assert(s->state == LCR_SESSION_SWITCHING && !sim->active);
    lcr_session_poll(s, *now + 9u);
    assert(!sim->active);
    lcr_session_poll(s, *now + 10u);
    lcr_session_poll(s, *now + 20u);
    lcr_session_poll(s, *now + 819u);
    assert(!sim->active);
    lcr_session_poll(s, *now + 820u);
    *now += 820u;
    if (s->state == LCR_SESSION_CAPTURING) assert(sim->active);
}
static void submit(lcr_session *s, simulator *sim, uint32_t *now) {
    waveform(sim);
    ++*now;
    assert(lcr_session_submit(s, sim->token, samples,
                              lcr_frequencies[sim->frequency].sample_pairs, *now));
    assert(!sim->active);
}

int main(void) {
    simulator sim;
    lcr_session s;
    unsigned cases = 0u;
    for (unsigned f = 0u; f < 4u; ++f) {
        for (unsigned resistance = 0u; resistance < 6u; ++resistance) {
            init(&sim, &s);
            sim.dut.re = lcr_nominal_rf[resistance];
            uint32_t now = UINT32_MAX - 500u;
            assert(lcr_session_start(&s, f, 3u, 1, 8000000.0, now));
            while (s.state == LCR_SESSION_SWITCHING) {
                ready(&s, &sim, &now);
                assert(s.state == LCR_SESSION_CAPTURING);
                assert(!lcr_session_start(&s, f, 0u, 0, 8000000.0, now));
                const uint32_t token = sim.token;
                assert(!lcr_session_submit(&s, token + 99u, samples, 1u, now));
                assert(sim.active);
                submit(&s, &sim, &now);
                assert(!lcr_session_submit(&s, token, samples, 1u, now));
                assert(s.capture_count <= 6u);
            }
            assert(s.state == LCR_SESSION_DONE && s.result.valid && sim.muted);
            assert(s.error == LCR_SESSION_NO_ERROR);
            assert(s.result.range_index == resistance);
            assert(fabs(s.result.corrected_z.re / sim.dut.re - 1.0) < 0.003);
            ++cases;
        }
    }
    unsigned reactive_cases = 0u;
    for (unsigned f = 0u; f < 4u; ++f) {
        const double c[4] = {10e-6, 1e-6, 10e-9, 10e-12};
        const double l[4] = {100e-3, 10e-3, 1e-3, 10e-6};
        const double omega = 6.2831853071795864769 * 8000000.0 *
                             lcr_dds_word(lcr_frequencies[f].nominal_hz) / 268435456.0;
        for (unsigned kind = 0u; kind < 2u; ++kind) {
            init(&sim, &s);
            sim.dut.re = 0.3;
            sim.dut.im = kind ? omega * l[f] : -1.0 / (omega * c[f]);
            uint32_t time = 0u;
            assert(lcr_session_start(&s, f, 3u, 1, 8000000.0, time));
            assert(!s.result.valid);
            while (s.state == LCR_SESSION_SWITCHING) {
                assert(!s.result.valid);
                ready(&s, &sim, &time);
                submit(&s, &sim, &time);
                assert(s.capture_count <= 6u);
            }
            assert(s.state == LCR_SESSION_DONE && s.result.valid && sim.muted);
            assert(s.result.parameters.kind == (kind ? LCR_COMPONENT_L : LCR_COMPONENT_C));
            const double value = kind ? s.result.parameters.ls_h : s.result.parameters.cs_f;
            assert(fabs(value / (kind ? l[f] : c[f]) - 1.0) < 0.005);
            ++reactive_cases;
        }
    }
    init(&sim, &s);
    uint32_t now = 0u;
    assert(lcr_session_start(&s, 1u, 3u, 0, 8000000.0, now));
    lcr_session_stop(&s);
    assert(s.state == LCR_SESSION_STOPPING && sim.muted);
    lcr_session_poll(&s, 10u); lcr_session_poll(&s, 20u);
    assert(s.state == LCR_SESSION_CANCELLED && sim.starts == 0u && !s.result.valid);
    assert(sim.mask == lcr_range_masks[3]);

    assert(lcr_session_start(&s, 1u, 2u, 0, 8000000.0, now));
    ready(&s, &sim, &now);
    uint32_t old_token = sim.token;
    lcr_session_stop(&s);
    assert(s.state == LCR_SESSION_CANCELLED && !sim.active && sim.muted);
    assert(!lcr_session_submit(&s, old_token, samples, 256u, now));
    assert(lcr_session_start(&s, 1u, 2u, 0, 8000000.0, now));
    ready(&s, &sim, &now);
    assert(sim.token != old_token);
    assert(!lcr_session_submit(&s, old_token, samples, 256u, now));
    lcr_session_poll(&s, now + s.capture_timeout);
    assert(s.state == LCR_SESSION_FAILED && s.error == LCR_SESSION_TIMEOUT && sim.muted);

    init(&sim, &s); now = UINT32_MAX - 300u;
    assert(lcr_session_start(&s, 0u, 2u, 0, 8000000.0, now));
    ready(&s, &sim, &now);
    assert(!lcr_session_submit(&s, sim.token, samples, 128u, now + s.capture_timeout));
    assert(s.error == LCR_SESSION_TIMEOUT && !s.result.valid && !sim.active);

    init(&sim, &s); now = 0u; sim.dut.re = 1e12;
    assert(lcr_session_start(&s, 3u, 5u, 1, 8000000.0, now));
    ready(&s, &sim, &now); submit(&s, &sim, &now);
    assert(s.state == LCR_SESSION_FAILED && s.error == LCR_SESSION_RANGE_LIMIT && s.capture_count == 1u);
    assert(!s.result.valid && sim.muted);

    init(&sim, &s); now = 0u; sim.dut.re = 0.001;
    assert(lcr_session_start(&s, 3u, 0u, 1, 8000000.0, now));
    ready(&s, &sim, &now); submit(&s, &sim, &now);
    assert(s.error == LCR_SESSION_RANGE_LIMIT && !s.result.valid && sim.muted);

    init(&sim, &s); now = 0u;
    assert(lcr_session_start(&s, 1u, 2u, 1, 8000000.0, now));
    ready(&s, &sim, &now);
    assert(lcr_session_submit(&s, sim.token, samples, 1u, now));
    assert(s.state == LCR_SESSION_FAILED && s.error == LCR_SESSION_FRAME_ERROR && !s.result.valid);

    init(&sim, &s); now = 0u; sim.fail_capture = 1;
    assert(lcr_session_start(&s, 1u, 2u, 0, 8000000.0, now));
    ready(&s, &sim, &now);
    assert(s.state == LCR_SESSION_FAILED && s.error == LCR_SESSION_CAPTURE_ERROR && sim.muted && !sim.active);
    init(&sim, &s); sim.fail_frequency = 1;
    assert(!lcr_session_start(&s, 1u, 2u, 0, 8000000.0, 0u));
    assert(s.error == LCR_SESSION_FRONTEND_ERROR && sim.starts == 0u);

    init(&sim, &s); now = 0u;
    lcr_calibration_nominal(&sim.cal, 1u, 2u);
    sim.cal.flags = LCR_CAL_CHANNEL; sim.cal.revision = 7u;
    sim.cal_enabled = 1;
    assert(lcr_session_start(&s, 1u, 2u, 0, 8000000.0, now));
    sim.cal.alpha.re = 2.0; sim.cal.revision = 8u;
    ready(&s, &sim, &now); submit(&s, &sim, &now);
    assert(s.state == LCR_SESSION_DONE && s.result.calibration_revision == 7u);
    assert(fabs(s.result.corrected_z.re - 1000.0) < 1.0);
    sim.cal.frequency_index = 2u;
    assert(!lcr_session_start(&s, 1u, 2u, 0, 8000000.0, now));
    assert(s.state == LCR_SESSION_FAILED && s.error == LCR_SESSION_CONFIG_ERROR && !s.result.valid);

    init(&sim, &s); now = 0u;
    assert(lcr_session_start(&s, 1u, 2u, 0, 8000000.0, now));
    ready(&s, &sim, &now); waveform(&sim);
    sim.fail_mute = 1;
    assert(lcr_session_submit(&s, sim.token, samples, 256u, now));
    assert(s.state == LCR_SESSION_FAILED && s.error == LCR_SESSION_FRONTEND_ERROR && !s.result.valid);
    assert(!lcr_session_start(&s, 1u, 2u, 0, 8000000.0, now));
    printf("lcr_session: %u R + %u L/C automatic searches, stale frames, cancellation, faults and snapshots passed\n",
           cases, reactive_cases);
    return 0;
}
