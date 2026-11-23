#ifndef LCR_FRONTEND_H
#define LCR_FRONTEND_H

#include <stdint.h>

/* Cooperative relay sequencer. Delays are provisional until measured on board. */
#define LCR_RELAY_BREAK_MS 10u
#define LCR_RELAY_MAKE_MS 10u
#define LCR_ANALOG_SETTLE_MS 800u

typedef enum {
    LCR_FE_MUTED, LCR_FE_BREAK, LCR_FE_MAKE, LCR_FE_SETTLE,
    LCR_FE_READY, LCR_FE_FAULT
} lcr_frontend_state;

typedef struct {
    void *context;
    int (*mute)(void *context, int muted); /* Nonzero means the write completed. */
    void (*relays)(void *context, uint8_t mask);
} lcr_frontend_io;

typedef struct {
    lcr_frontend_io io;
    lcr_frontend_state state;
    uint32_t since;
    uint8_t range, calibration, cancel;
    uint32_t settle_ms;
} lcr_frontend;

/* Caller must establish GPIO default feedback (PD[6:0]=0) before init. */
int lcr_frontend_init(lcr_frontend *fe, lcr_frontend_io io);
int lcr_frontend_begin(lcr_frontend *fe, unsigned range, int calibration, uint32_t now);
void lcr_frontend_poll(lcr_frontend *fe, uint32_t now);
/* Cancellation finishes mechanical switching with excitation muted. */
void lcr_frontend_stop(lcr_frontend *fe);
int lcr_frontend_ready(const lcr_frontend *fe);

#endif
