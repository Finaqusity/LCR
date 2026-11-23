#ifndef LCR_APP_H
#define LCR_APP_H
#include "lcr_session.h"
#include "lcr_cal_store.h"
#include "lcr_ui.h"
typedef enum { LCR_PAGE_MEASURE, LCR_PAGE_MENU, LCR_PAGE_CONFIRM, LCR_PAGE_CAL, LCR_PAGE_MESSAGE } lcr_page;
typedef struct {
    lcr_session_io acquisition;
    void *context;
    int (*save)(void *context, lcr_cal_profile *profile);
} lcr_app_io;
typedef struct {
    lcr_app_io io;
    lcr_session session;
    lcr_cal_profile profile, working;
    lcr_measurement last, best;
    lcr_keys keys;
    lcr_canvas canvas;
    lcr_model model;
    lcr_page page;
    unsigned running, details, dirty, menu, frequency_mode, range_mode;
    unsigned active_frequency, active_range, scan, scan_index, have_best;
    unsigned calibration, cal_frequency, cal_range, cal_samples;
    unsigned unsaved;
    uint32_t next_start;
    double best_score;
    lcr_complex cal_sum;
    const char *message, *message2;
} lcr_app;
int lcr_app_init(lcr_app *app, lcr_app_io io, const lcr_cal_profile *profile, unsigned buttons, uint32_t now);
void lcr_app_poll(lcr_app *app, unsigned buttons, uint32_t now);
void lcr_app_submit(lcr_app *app, uint32_t token, const uint16_t *pairs, size_t count, uint32_t now);
void lcr_app_capture_error(lcr_app *app);
void lcr_app_halt(lcr_app *app);
/* Same actions as the two physical keys; also usable for host simulation. */
void lcr_app_event(lcr_app *app, unsigned events, uint32_t now);
#endif
