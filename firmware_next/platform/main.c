#include "lcr_app.h"
#include "lcr_board.h"
#include "stm32f4xx.h"
#include "lcr_config.h"

static lcr_app app;
static lcr_cal_profile boot_profile;
static int frequency(void *context,uint32_t hz) { (void)context;return lcr_board_dds_frequency(hz); }
static int save(void *context,lcr_cal_profile *profile) { (void)context;return lcr_journal_save(lcr_board_journal(),profile); }

#if defined(LCR_WAVE_TEST)
/* Analog bring-up image: board init leaves the AD9833 at 1 kHz but in reset.
 * Release reset without starting any measurement, so TP1 carries a continuous
 * 1 kHz sine and the fixture is unloaded (all range relays released). */
static void wave_test_run(void) {
    if (!lcr_board_init()) {
        for (;;) { lcr_board_led(1);__WFI(); }
    }
    const lcr_frontend_io io = lcr_board_frontend_io();
    io.relays(io.context, 0u);
    if (!io.mute(io.context, 0)) {
        for (;;) { lcr_board_led(1);__WFI(); }
    }
    (void)lcr_board_puts("LCR WAVE TEST: TP1 1 kHz, relays released\r\n");
    uint32_t next = lcr_board_millis();
    uint32_t on = 0u;
    for (;;) {
        const uint32_t now = lcr_board_millis();
        if ((uint32_t)(now - next) >= 500u) {
            next = now; on ^= 1u; lcr_board_led((int)on);
        }
        __WFI();
    }
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
    uint32_t displayed=0u,recovery=0u;
    for (;;) {
        uint32_t now=lcr_board_millis();
        for (unsigned budget=0u;budget<8u;++budget) {
            const int ch=lcr_board_getchar();
            if (ch==-1)break;
            if (ch==-2) { (void)lcr_board_puts("RX ERROR\r\n");break; }
            if (ch=='1'||ch=='2')lcr_app_event(&app,LCR_KEY_SHORT,now);
            else if(ch=='b')lcr_app_event(&app,LCR_KEY_LONG,now);
            else if(ch=='d')lcr_app_event(&app,LCR_KEY_DOUBLE,now);
            else if(ch=='s') { lcr_app_halt(&app);(void)lcr_board_puts("STOP\r\n"); }
            else if(ch=='?') (void)lcr_board_puts("1/2:short b:long d:double s:stop\r\n");
        }
        lcr_app_poll(&app,lcr_board_buttons(),now);
        const uint16_t *pairs=0;size_t count=0u;uint32_t token=0u;
        const int capture=lcr_board_capture_take(&pairs,&count,&token);
        if(capture>0)lcr_app_submit(&app,token,pairs,count,lcr_board_millis());
        else if(capture<0)lcr_app_capture_error(&app);
        if(capture)lcr_app_poll(&app,lcr_board_buttons(),lcr_board_millis());
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
