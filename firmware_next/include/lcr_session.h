#ifndef LCR_SESSION_H
#define LCR_SESSION_H

#include "lcr_frontend.h"
#include "lcr_measure.h"

typedef enum {
    LCR_SESSION_IDLE, LCR_SESSION_SWITCHING, LCR_SESSION_CAPTURING,
    LCR_SESSION_STOPPING, LCR_SESSION_DONE, LCR_SESSION_CANCELLED, LCR_SESSION_FAILED
} lcr_session_state;
typedef enum {
    LCR_SESSION_NO_ERROR, LCR_SESSION_CONFIG_ERROR, LCR_SESSION_FRONTEND_ERROR,
    LCR_SESSION_CAPTURE_ERROR, LCR_SESSION_TIMEOUT, LCR_SESSION_FRAME_ERROR,
    LCR_SESSION_RANGE_LIMIT, LCR_SESSION_USER_STOP, LCR_SESSION_UNSTABLE
} lcr_session_error;
typedef enum { LCR_CAPTURE_MEASURE, LCR_CAPTURE_CHANNEL, LCR_CAPTURE_OPEN, LCR_CAPTURE_STANDARD } lcr_capture_kind;

typedef struct {
    lcr_frontend_io frontend;
    void *context;
    int (*frequency)(void *context, uint32_t hz); /* Called only when muted. */
    /* Start asynchronously; do not submit inline. Token identifies this capture. */
    int (*capture)(void *context, unsigned frequency, unsigned range, size_t pairs, uint32_t token);
    /* Idempotent; on return DMA must no longer modify a submitted frame. */
    void (*capture_cancel)(void *context);
    /* Optional. Slot copied before switching; NULL means nominal, uncalibrated. */
    const lcr_calibration *(*calibration)(void *context, unsigned frequency, unsigned range);
} lcr_session_io;

typedef struct {
    lcr_session_io io;
    lcr_frontend frontend;
    lcr_session_state state, terminal;
    lcr_session_error error;
    lcr_measure_config config;
    lcr_calibration calibration_snapshot;
    lcr_measurement result;
    uint32_t token_counter, active_token, capture_since, capture_timeout;
    uint8_t visited;
    unsigned autorange, capture_count;
    unsigned keep_excitation;
    lcr_capture_kind kind;
    unsigned adaptive_settling, settling, stable_count, have_previous;
    uint32_t settle_since, settle_next;
    lcr_measurement previous;
} lcr_session;

/* Single owner/main-loop API. Do not copy a live session or call from an ISR. */
int lcr_session_init(lcr_session *session, lcr_session_io io);
int lcr_session_start(lcr_session *session, unsigned frequency, unsigned initial_range,
                      int autorange, double hse_hz, uint32_t now);
void lcr_session_poll(lcr_session *session, uint32_t now);
/* Rejects stale/duplicate tokens. Stops acquisition before consuming the buffer. */
int lcr_session_submit(lcr_session *session, uint32_t token,
                       const uint16_t *pairs, size_t count, uint32_t now);
void lcr_session_stop(lcr_session *session);
int lcr_session_calibrate(lcr_session *session, unsigned frequency, unsigned range,
                          lcr_capture_kind kind, double hse_hz, uint32_t now);

#endif
