#include "lcr_app.h"
#include "lcr_config.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static lcr_app app;
static lcr_cal_profile profile;
static int mute_io(void *context, int muted) { (void)context; (void)muted; return 1; }
static void relay_io(void *context, uint8_t mask) { (void)context; (void)mask; }
static int frequency_io(void *context, uint32_t hz) { (void)context; (void)hz; return 1; }
static int capture_io(void *context, unsigned f, unsigned r, size_t count, uint32_t token) {
    (void)context; (void)f; (void)r; (void)count; (void)token; return 1;
}
static void cancel_io(void *context) { (void)context; }
static void init_app(void) {
    const lcr_frontend_io frontend = {0, mute_io, relay_io};
    const lcr_session_io session = {frontend, 0, frequency_io, capture_io, cancel_io, 0};
    const lcr_app_io io = {session, 0, 0};
    lcr_cal_profile_nominal(&profile, 1u, 1u, 8000000.0);
    assert(lcr_app_init(&app, io, &profile, 0u, 0u));
    assert(app.frequency_mode == 0u); /* AUTO at power-up */
    lcr_app_set_frequency_mode(&app, 2u);
    assert(lcr_app_start_measure(&app, 1000u));
}
static void result(double r, double x, unsigned f, unsigned range, uint32_t now) {
    lcr_measurement *m = &app.session.result;
    memset(m, 0, sizeof(*m));
    m->valid = 1; m->status = LCR_OK;
    m->frequency_index = f; m->range_index = range;
    m->actual_hz = lcr_frequencies[f].nominal_hz;
    m->corrected_z.re = r; m->corrected_z.im = x;
    assert(lcr_parameters_from_z(m->corrected_z, m->actual_hz, &m->parameters) == LCR_OK);
    /* Inject a completed session with the same frontend invariants as finish(). */
    app.session.frontend.state = LCR_FE_READY;
    app.session.frontend.range = (uint8_t)range;
    app.session.settling = 0u;
    app.session.state = LCR_SESSION_DONE;
    lcr_app_poll(&app, 0u, now);
}
static void fail(lcr_session_error error, uint32_t now) {
    /* Failed captures have been cancelled and the frontend has been muted. */
    app.session.frontend.state = LCR_FE_MUTED;
    app.session.settling = 0u;
    app.session.state = LCR_SESSION_FAILED;
    app.session.error = error;
    lcr_app_poll(&app, 0u, now);
}
int main(void) {
    /* Public application path, deliberately over three and then five readings.
       The pre-fix sorting scratch array overflows at the fourth result. */
    init_app();
    for (unsigned n = 0u; n < 1000u; ++n) {
        result(1000.0 + n % 5u, 0.1, 1u, 3u, 1000u + n * 50u);
        assert(app.last.valid && app.result_seq == n + 1u);
        assert(app.smooth_n >= 1u && app.smooth_n <= 5u);
        if (n >= 4u) assert(fabs(app.last.parameters.rs_ohm - 1002.0) < 1e-9);
    }
    puts("regression: five-sample median survives 1000 completed measurements");

    /* A second history window must not mix samples from different ranges. */
    result(2000.0, 0.1, 1u, 4u, 52000u);
    assert(app.smooth_n == 1u && fabs(app.last.parameters.rs_ohm - 2000.0) < 1e-9);

    /* Physical double press and the public single-shot API both stop after
       one completed logical result, without relying on the serial poller. */
    init_app();
    lcr_app_event(&app, LCR_KEY_DOUBLE, 1000u);
    assert(app.single_measurement && app.running);
    result(1000.0, 0.1, 1u, 3u, 1100u);
    assert(app.result_seq == 1u && !app.running && !app.single_measurement);
    assert(lcr_app_start_once(&app, 1200u));
    result(1000.0, 0.1, 1u, 3u, 1300u);
    assert(app.result_seq == 2u && !app.running);
    puts("regression: physical/API single shots publish exactly once and hold");

    /* A band request during acquisition must not reuse that old frame as
       the first frame of a newly requested AUTO scan. */
    init_app(); lcr_app_poll(&app, 0u, 1000u);
    assert(app.session.config.frequency_index == 1u);
    lcr_app_set_frequency_mode(&app, 3u);
    result(1000.0, 0.1, 1u, 3u, 1100u);
    assert(app.result_seq == 0u && !app.scan);
    assert(app.session.config.frequency_index == 2u);
    lcr_app_set_frequency_mode(&app, 0u);
    result(1000.0, 0.1, 2u, 3u, 1200u);
    assert(app.scan && app.scan_index == 0u && app.result_seq == 0u);
    assert(app.session.config.frequency_index == 1u);
    puts("regression: in-flight band changes apply at the next capture boundary");

    /* An old displayed reading cannot hide failure of every new scan band.
       Previously that quiet-retry path indexed frequencies[4]. */
    init_app(); result(1000.0, 0.1, 1u, 3u, 1100u);
    lcr_app_set_frequency_mode(&app, 0u);
    lcr_app_poll(&app, 0u, 1200u);
    for (unsigned i = 0u; app.running && i < 4u; ++i) {
        fail(LCR_SESSION_FRAME_ERROR, 1300u + i * 100u);
        if (app.running) {
            lcr_app_poll(&app, 0u, 1341u + i * 100u);
            assert(app.session.config.frequency_index <= LCR_CAL_MAX_FREQUENCY_INDEX);
        }
    }
    assert(!app.running && !app.scan && app.page == LCR_PAGE_MESSAGE);
    assert(app.result_seq == 1u && app.retry_measurement);
    puts("regression: an exhausted AUTO sweep exits without a table overrun");

    /* Failure of the remaining bands still publishes the best available
       result and completes a single shot; the UART is not needed to stop it. */
    init_app(); lcr_app_halt(&app); lcr_app_set_frequency_mode(&app, 0u);
    assert(lcr_app_start_once(&app, 1000u));
    result(1000.0, 0.1, 1u, 3u, 1100u);
    for (unsigned i = 0u; app.running && i < 4u; ++i) {
        fail(LCR_SESSION_TIMEOUT, 1200u + i * 100u);
    }
    assert(app.result_seq == 1u && app.last.valid && !app.running);
    puts("regression: degraded AUTO single shot publishes and stops");

    /* Single-shot errors and calibration completion/error messages must not
       arm main's automatic continuous-measurement retry. */
    init_app(); lcr_app_halt(&app);
    assert(lcr_app_start_once(&app, 1000u));
    fail(LCR_SESSION_FRAME_ERROR, 1100u);
    assert(!app.retry_measurement && !app.running);
    assert(lcr_app_cal_start(&app, LCR_CAPTURE_CHANNEL, 0.0, 1200u));
    lcr_app_capture_error(&app);
    assert(!app.retry_measurement && memcmp(&profile, &app.profile, sizeof(profile)) == 0);
    puts("regression: one-shot/calibration failures retain operator control and calibration");
    return 0;
}
