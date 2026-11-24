#include "lcr_frontend.h"
#include "lcr_core.h"

static int mute(lcr_frontend *fe, int value) {
    if (fe->io.mute(fe->io.context, value)) return 1;
    /* A failed transaction is not evidence that excitation was silenced. */
    fe->state = LCR_FE_FAULT;
    return 0;
}

int lcr_frontend_init(lcr_frontend *fe, lcr_frontend_io io) {
    if (!fe || !io.mute || !io.relays) return 0;
    *fe = (lcr_frontend){io, LCR_FE_MUTED, 0u, 3u, 0u, 0u, LCR_ANALOG_SETTLE_MS};
    return mute(fe, 1);
}

int lcr_frontend_begin(lcr_frontend *fe, unsigned range, int calibration, uint32_t now) {
    if (!fe || range >= 6u || (calibration != 0 && calibration != 1) ||
        (fe->state != LCR_FE_READY && fe->state != LCR_FE_MUTED)) return 0;
    if (!mute(fe, 1)) return 0;
    fe->range = (uint8_t)range;
    fe->calibration = (uint8_t)calibration;
    fe->cancel = 0u;
    /* Explicit break: drop every range relay (there is no NC default range on
       the current wiring); K78 stays inactive unless calibrating. */
    fe->io.relays(fe->io.context, (uint8_t)(calibration ? 0x40u : 0u));
    fe->since = now;
    fe->state = LCR_FE_BREAK;
    return 1;
}

void lcr_frontend_poll(lcr_frontend *fe, uint32_t now) {
    if (!fe) return;
    const uint32_t elapsed = now - fe->since;
    switch (fe->state) {
    case LCR_FE_BREAK:
        if (elapsed < LCR_RELAY_BREAK_MS) return;
        fe->io.relays(fe->io.context, (uint8_t)(lcr_range_masks[fe->range] |
                                              (fe->calibration ? 0x40u : 0u)));
        fe->since = now;
        fe->state = LCR_FE_MAKE;
        break;
    case LCR_FE_MAKE:
        if (elapsed < LCR_RELAY_MAKE_MS) return;
        if (fe->cancel) {
            fe->state = LCR_FE_MUTED;
        } else if (mute(fe, 0)) {
            fe->since = now;
            fe->state = LCR_FE_SETTLE;
        }
        break;
    case LCR_FE_SETTLE:
        if (elapsed >= fe->settle_ms) fe->state = LCR_FE_READY;
        break;
    default:
        break;
    }
}

void lcr_frontend_stop(lcr_frontend *fe) {
    if (!fe || fe->state == LCR_FE_FAULT) return;
    if (!mute(fe, 1)) return;
    fe->cancel = 1u;
    if (fe->state == LCR_FE_READY || fe->state == LCR_FE_SETTLE)
        fe->state = LCR_FE_MUTED;
}

int lcr_frontend_ready(const lcr_frontend *fe) {
    return fe && fe->state == LCR_FE_READY;
}
