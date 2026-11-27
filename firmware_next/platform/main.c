#include "lcr_app.h"
#include "lcr_board.h"
#include "lcr_protocol.h"
#include "stm32f4xx.h"
#include "lcr_config.h"

static lcr_app app;
static lcr_cal_profile boot_profile;
static int frequency(void *context,uint32_t hz) { (void)context;return lcr_board_dds_frequency(hz); }
static int save(void *context,lcr_cal_profile *profile) { (void)context;return lcr_journal_save(lcr_board_journal(),profile); }

#if defined(LCR_WAVE_TEST)
#include <math.h>

/* SWD-readable diagnostics: last captured frame's fitted amplitudes, in millicodes. */
typedef struct {
    uint32_t seq, status;
    int32_t v1amp, v2amp, v1dc, v2dc, v1res, v2res;
} lcr_wave_diag;
volatile lcr_wave_diag lcr_wave_diag_data;

/* Analog bring-up image: keep the DAC excitation running at 1 kHz with all
 * range relays released (TP1 = unloaded excitation). In parallel, capture one
 * 1 kHz frame per 250 ms and publish V1/V2 fits so the ADC path can be checked
 * over SWD. */
static void wave_diag_run(void) {
    const lcr_measure_config cfg = {1u, 0u, 8000000.0, 0}; /* 1 kHz, 10 ohm slot. */
    lcr_measurement m;
    uint32_t token = 1u;
    uint32_t next = lcr_board_millis(), led = next, on = 0u;
    int pending = 0;
    for (;;) {
        const uint16_t *pairs = 0;
        size_t count = 0u;
        uint32_t got = 0u;
        const int taken = lcr_board_capture_take(&pairs, &count, &got);
        if (taken > 0) {
            (void)lcr_measure_frame(pairs, count, &cfg, &m);
            lcr_wave_diag_data.status = (uint32_t)m.status;
            lcr_wave_diag_data.v1amp = (int32_t)(hypot(m.v1.phasor.re, m.v1.phasor.im) * 1000.0);
            lcr_wave_diag_data.v2amp = (int32_t)(hypot(m.v2.phasor.re, m.v2.phasor.im) * 1000.0);
            lcr_wave_diag_data.v1dc = (int32_t)(m.v1.dc * 1000.0);
            lcr_wave_diag_data.v2dc = (int32_t)(m.v2.dc * 1000.0);
            lcr_wave_diag_data.v1res = (int32_t)(m.v1.residual_rms * 1000.0);
            lcr_wave_diag_data.v2res = (int32_t)(m.v2.residual_rms * 1000.0);
            ++lcr_wave_diag_data.seq;
            pending = 0;
            next = lcr_board_millis() + 250u;
        } else if (taken < 0) {
            lcr_wave_diag_data.status = 0xEEEE0000u;
            pending = 0;
            next = lcr_board_millis() + 250u;
        }
        const uint32_t now = lcr_board_millis();
        if (!pending && (int32_t)(now - next) >= 0) {
            pending = lcr_board_capture_start(0, 1u, 0u, 256u, token);
            if (pending) ++token;
            else { lcr_wave_diag_data.status = 0xDDDD0000u; next = now + 250u; }
        }
        if ((uint32_t)(now - led) >= 500u) { led = now; on ^= 1u; lcr_board_led((int)on); }
        __WFI();
    }
}

static void wave_test_run(void) {
    if (!lcr_board_init()) {
        for (;;) { lcr_board_led(1);__WFI(); }
    }
    const lcr_frontend_io io = lcr_board_frontend_io();
    io.relays(io.context, 0u);
    if (!io.mute(io.context, 0)) {
        for (;;) { lcr_board_led(1);__WFI(); }
    }
    (void)lcr_board_puts("LCR WAVE TEST: TP1 1 kHz, relays released; ADC diag on\r\n");
    wave_diag_run();
}
#endif

int main(void) {
#if defined(LCR_WAVE_TEST)
    wave_test_run();
#endif
    if (!lcr_board_init()) {
        /* Initialization failed: never start the acquisition/measurement scheduler. */
        for (;;) { lcr_board_led(1);__WFI(); }
    }
    (void)lcr_board_puts("LCR next: boot; excitation muted\r\n");
    const uint32_t board=lcr_board_identity();
    lcr_cal_profile_nominal(&boot_profile,board,LCR_FIXTURE_ID,LCR_DEFAULT_HSE_HZ);
    unsigned bank=0u;
    const int loaded=lcr_journal_load(lcr_board_journal(),board,LCR_FIXTURE_ID,&boot_profile,&bank);
    if(loaded<=0)lcr_cal_profile_nominal(&boot_profile,board,LCR_FIXTURE_ID,LCR_DEFAULT_HSE_HZ);
    (void)lcr_board_puts(loaded>0?"Calibration loaded\r\n":"Nominal parameters; calibration required\r\n");
    const lcr_session_io acquisition={lcr_board_frontend_io(),0,frequency,
        lcr_board_capture_start,lcr_board_capture_cancel,0};
    const lcr_app_io io={acquisition,0,save};
    if (!lcr_app_init(&app,io,&boot_profile,lcr_board_buttons(),lcr_board_millis()))
        for (;;) { lcr_board_led(1);__WFI(); }
    int display_ok=lcr_board_display_init();
    lcr_protocol_init(&app, "0.5.0", board, LCR_FIXTURE_ID);
    lcr_protocol_boot(&app);
    uint32_t displayed=0u,recovery=0u;
#if LCR_AUTO_START
    /* Bench/demo convenience: the on-board key is on the back of the board, so
     * start continuous measurement after warm-up. Physical short press changes
     * band, double press takes one result, long press opens the menu. */
    (void)lcr_app_start_measure(&app, lcr_board_millis() + 1500u);
    uint32_t auto_retry=lcr_board_millis();
#endif
    for (;;) {
        uint32_t now=lcr_board_millis();
        /* Serial console: commands + automatic $RES/$STATE/$CAL frames. */
        lcr_protocol_poll(&app, now);
        lcr_app_poll(&app,lcr_board_buttons(),now);
#if LCR_AUTO_START
        /* Keep retrying a failed measurement every 2 s, so connecting the DUT
         * brings a reading back without touching the key. */
        if(app.running || app.page!=LCR_PAGE_MESSAGE) auto_retry=now;
        else if(app.retry_measurement && now-auto_retry>=2000u) {
            /* Retry through the API: the short-press gesture now steps the
               excitation band, so driving it from here would only change the
               frequency and never restart the measurement. */
            auto_retry=now;
            (void)lcr_app_start_measure(&app, now);
            lcr_app_poll(&app,lcr_board_buttons(),lcr_board_millis());
        }
#endif
        const uint16_t *pairs=0;size_t count=0u;uint32_t token=0u;
        const int capture=lcr_board_capture_take(&pairs,&count,&token);
        if(capture>0)lcr_app_submit(&app,token,pairs,count,lcr_board_millis());
        else if(capture<0)lcr_app_capture_error(&app);
        if(capture)lcr_app_poll(&app,lcr_board_buttons(),lcr_board_millis());
        /* Push a freshly completed result out immediately. */
        lcr_protocol_poll(&app, lcr_board_millis());
        now=lcr_board_millis();
        if(app.session.state!=LCR_SESSION_CAPTURING) {
            if(!display_ok && now-recovery>=2000u) { recovery=now;display_ok=lcr_board_display_init();app.dirty=1u; }
            if(display_ok && app.dirty && now-displayed>=100u) {
                display_ok=lcr_board_display(app.canvas.pixels);
                app.dirty=0u;displayed=lcr_board_millis();
                if(!display_ok)(void)lcr_board_puts("OLED offline; keys/measurement remain active\r\n");
            }
            lcr_board_led(app.running && ((now/500u)&1u));
        }
        __WFI();
    }
}
