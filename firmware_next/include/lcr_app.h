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
    /* Result generation counter for the serial protocol (0 = none yet). */
    uint32_t result_seq;
    /* Real standard impedance for RF STANDARD calibration, 0 = nominal Rf. */
    double cal_standard_ohm;
    /* Display stabilisation: median of the last few corrected impedances on the
     * same frequency/range, plus a classification hysteresis so phase noise
     * cannot flip the reading between R, L and C. */
    lcr_complex smooth_hist[3];
    unsigned smooth_n;
    unsigned smooth_frequency, smooth_range;
    lcr_component stable_kind;
    unsigned kind_streak;
} lcr_app;
int lcr_app_init(lcr_app *app, lcr_app_io io, const lcr_cal_profile *profile, unsigned buttons, uint32_t now);
void lcr_app_poll(lcr_app *app, unsigned buttons, uint32_t now);
void lcr_app_submit(lcr_app *app, uint32_t token, const uint16_t *pairs, size_t count, uint32_t now);
void lcr_app_capture_error(lcr_app *app);
void lcr_app_halt(lcr_app *app);
/* Same actions as the two physical keys; also usable for host simulation. */
void lcr_app_event(lcr_app *app, unsigned events, uint32_t now);

/* Serial-console API: every action is shared with the UI path, so the web
 * console and the keys cannot diverge. Modes are 0..4 (frequency) and 0..6
 * (range); model 0=series 1=parallel. Start returns 0 while a capture is
 * active; calibration kind is an lcr_capture_kind value. */
void lcr_app_set_frequency_mode(lcr_app *app, unsigned mode);
void lcr_app_set_range_mode(lcr_app *app, unsigned mode);
void lcr_app_set_model(lcr_app *app, unsigned parallel);
int lcr_app_start_measure(lcr_app *app, uint32_t now);
int lcr_app_stop_measure(lcr_app *app);
int lcr_app_cal_start(lcr_app *app, unsigned kind, double standard_ohm, uint32_t now);
int lcr_app_cal_save(lcr_app *app);
int lcr_app_cal_clear(lcr_app *app);
int lcr_app_cal_abort(lcr_app *app);
/* 0 = nominal, up to 4 with channel/fixture/standard calibration present. */
unsigned lcr_app_calibration_level(const lcr_app *app);
#endif
