#include "lcr_frontend.h"
#include "lcr_core.h"

#include <assert.h>
#include <stdio.h>

typedef struct {
    int muted, fail_mute, writes;
    uint8_t mask;
} simulated_io;

static int write_mute(void *context, int muted) {
    simulated_io *io = context;
    if (io->fail_mute) return 0;
    io->muted = muted;
    return 1;
}

static void write_relays(void *context, uint8_t mask) {
    simulated_io *io = context;
    assert(io->muted);
    io->mask = mask;
    ++io->writes;
}

static void finish(lcr_frontend *fe, uint32_t now) {
    lcr_frontend_poll(fe, now + 10u);
    assert(fe->state == LCR_FE_MAKE);
    lcr_frontend_poll(fe, now + 20u);
    assert(fe->state == LCR_FE_SETTLE);
    assert(!lcr_frontend_ready(fe));
    lcr_frontend_poll(fe, now + 819u);
    assert(!lcr_frontend_ready(fe));
    lcr_frontend_poll(fe, now + 820u);
    assert(lcr_frontend_ready(fe));
}

int main(void) {
    /* Relay wiring snapshot: PD0+PD7 = 10R, PD1 = 100R, PD5 = 1K,
       PD2 = 10K, PD3 = 100K, PD4 = 1M; PD6 = K78 calibration path. */
    static const uint8_t expected_masks[6] = {0x81u, 0x02u, 0x20u, 0x04u, 0x08u, 0x10u};
    for (unsigned r = 0u; r < 6u; ++r) assert(lcr_range_masks[r] == expected_masks[r]);
    assert(lcr_range_masks[0] == (uint8_t)(0x01u | 0x80u)); /* both 10R coils */
    /* Actual production sequencer, all 36 transitions in both K1 positions. */
    for (unsigned from = 0u; from < 6u; ++from) {
        for (unsigned to = 0u; to < 6u; ++to) {
            for (int cal = 0; cal <= 1; ++cal) {
                simulated_io io = {0, 0, 0, 0u};
                lcr_frontend fe;
                lcr_frontend_io ops = {&io, write_mute, write_relays};
                assert(lcr_frontend_init(&fe, ops));
                assert(lcr_frontend_begin(&fe, from, !cal, 0u));
                finish(&fe, 0u);
                assert(lcr_frontend_begin(&fe, to, cal, 1000u));
                assert(io.muted && io.mask == (uint8_t)(cal ? 0x40u : 0u));
                assert(!lcr_frontend_begin(&fe, 3u, 0, 1000u));
                lcr_frontend_poll(&fe, 1009u);
                assert(fe.state == LCR_FE_BREAK);
                finish(&fe, 1000u);
                assert(io.mask == (uint8_t)(lcr_range_masks[to] | (cal ? 0x40u : 0u)));
                assert(!io.muted);
            }
        }
    }
    for (unsigned cancel_phase = 0u; cancel_phase < 4u; ++cancel_phase) {
        simulated_io io = {0, 0, 0, 0u};
        lcr_frontend fe;
        lcr_frontend_io ops = {&io, write_mute, write_relays};
        assert(lcr_frontend_init(&fe, ops));
        /* Millisecond wrap must preserve the full mechanical waits. */
        const uint32_t start = UINT32_MAX - 5u;
        assert(lcr_frontend_begin(&fe, 5u, 0, start));
        if (cancel_phase >= 1u) lcr_frontend_poll(&fe, start + 10u);
        if (cancel_phase >= 2u) lcr_frontend_poll(&fe, start + 20u);
        if (cancel_phase >= 3u) lcr_frontend_poll(&fe, start + 820u);
        lcr_frontend_stop(&fe);
        lcr_frontend_poll(&fe, start + 900u);
        lcr_frontend_poll(&fe, start + 910u);
        assert(io.muted && io.mask == lcr_range_masks[5]);
        assert(fe.state == LCR_FE_MUTED && !lcr_frontend_ready(&fe));
    }
    simulated_io io = {0, 0, 0, 0u};
    lcr_frontend fe;
    lcr_frontend_io ops = {&io, write_mute, write_relays};
    assert(lcr_frontend_init(&fe, ops));
    assert(!lcr_frontend_begin(&fe, 6u, 0, 0u));
    io.fail_mute = 1;
    assert(!lcr_frontend_begin(&fe, 0u, 0, 0u));
    assert(fe.state == LCR_FE_FAULT && io.writes == 0);
    io.fail_mute = 0;
    assert(!lcr_frontend_begin(&fe, 0u, 0, 0u)); /* Latched fault. */

    assert(lcr_frontend_init(&fe, ops));
    assert(lcr_frontend_begin(&fe, 0u, 0, 0u));
    lcr_frontend_poll(&fe, 1000u); /* Late poll cannot collapse dwell phases. */
    assert(fe.state == LCR_FE_MAKE && io.muted);
    lcr_frontend_poll(&fe, 1000u);
    assert(fe.state == LCR_FE_MAKE && io.muted);
    io.fail_mute = 1;
    lcr_frontend_poll(&fe, 1010u);
    assert(fe.state == LCR_FE_FAULT && !lcr_frontend_ready(&fe));
    puts("lcr_frontend: 72 range/CAL paths, cancellation, wrap and failures passed");
    return 0;
}
