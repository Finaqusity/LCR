#include "lcr_app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "lcr_config.h"

static const unsigned frequencies[4] = {1u,2u,3u,0u};
static const char *const menu_names[] = {"FREQUENCY", "RANGE", "CHANNEL CAL", "OPEN CAL", "RF STANDARD",
    "SHORT CAL:LOCKED", "SAVE CAL", "CLEAR CAL", "BACK"};
static int frequency_io(void *context, uint32_t hz) {
    lcr_app *a = context;
    return a->io.acquisition.frequency(a->io.acquisition.context, hz);
}
static int capture_io(void *context, unsigned f, unsigned r, size_t count, uint32_t token) {
    lcr_app *a = context;
    return a->io.acquisition.capture(a->io.acquisition.context, f, r, count, token);
}
static void cancel_io(void *context) {
    lcr_app *a = context;
    a->io.acquisition.capture_cancel(a->io.acquisition.context);
}
static const lcr_calibration *calibration_io(void *context, unsigned f, unsigned r) {
    lcr_app *a = context;
    return a->calibration ? &a->working.slots[f][r] : &a->profile.slots[f][r];
}
static int active(const lcr_app *a) {
    return a->session.state == LCR_SESSION_SWITCHING || a->session.state == LCR_SESSION_CAPTURING ||
           a->session.state == LCR_SESSION_STOPPING;
}
static void stop(lcr_app *a) {
    a->running = 0u; a->scan = 0u;
    lcr_session_stop(&a->session);
    a->session.keep_excitation = 0u;
}
static void message(lcr_app *a, const char *line1, const char *line2) {
    stop(a); a->calibration = 0u;
    a->page = LCR_PAGE_MESSAGE;
    a->message = line1; a->message2 = line2; a->dirty = 1u;
}
int lcr_app_init(lcr_app *a, lcr_app_io io, const lcr_cal_profile *profile, unsigned buttons, uint32_t now) {
    if (!a || !profile || !io.acquisition.frequency || !io.acquisition.capture || !io.acquisition.capture_cancel) return 0;
    memset(a, 0, sizeof(*a));
    a->io = io; a->profile = *profile;
    a->active_frequency = 1u; a->active_range = 3u;
    const lcr_session_io session_io = {io.acquisition.frontend, a, frequency_io, capture_io, cancel_io, calibration_io};
    if (!lcr_session_init(&a->session, session_io)) return 0;
    a->session.adaptive_settling=LCR_ADAPTIVE_SETTLE;
    lcr_keys_init(&a->keys, buttons, now);
    a->dirty = 1u;
    lcr_ui_measure(&a->canvas, 0, a->model, "IDLE", 0);
    return 1;
}
static void start_measure(lcr_app *a, uint32_t now) {
    a->page = LCR_PAGE_MEASURE;
    a->running = 1u;
    a->scan = a->frequency_mode == 0u;
    a->scan_index = 0u; a->have_best = 0u;
    a->next_start = now;
    a->dirty = 1u;
}
void lcr_app_event(lcr_app *a, unsigned events, uint32_t now) {
    if (!a) return;
    if (events & LCR_KEY2_LONG) {
        stop(a); a->calibration = 0u;
        a->page = a->page == LCR_PAGE_MEASURE ? LCR_PAGE_MENU : LCR_PAGE_MEASURE;
        a->dirty = 1u; return;
    }
    if (a->page == LCR_PAGE_MEASURE) {
        if (events & LCR_KEY1_SHORT) a->model = a->model == LCR_MODEL_SERIES ? LCR_MODEL_PARALLEL : LCR_MODEL_SERIES;
        if (events & LCR_KEY1_LONG) a->details ^= 1u;
        if (events & LCR_KEY2_SHORT) { if (a->running) stop(a); else start_measure(a, now); }
    } else if (a->page == LCR_PAGE_MENU) {
        if (events & LCR_KEY1_SHORT) a->menu = (a->menu + 1u) % 9u;
        if ((events & LCR_KEY2_SHORT) && !active(a)) {
            if (a->menu == 0u) a->frequency_mode = (a->frequency_mode + 1u) % 5u;
            else if (a->menu == 1u) a->range_mode = (a->range_mode + 1u) % 7u;
            else if (a->menu >= 2u && a->menu <= 4u) a->page = LCR_PAGE_CONFIRM;
            else if (a->menu == 5u) message(a, "SHORT NOT VALIDATED", "NO EXCITATION STARTED");
            else if (a->menu == 6u) {
                if (a->session.frontend.state != LCR_FE_MUTED || !a->io.save ||
                    !a->io.save(a->io.context, &a->profile)) message(a, "SAVE FAILED", "RAM CAL RETAINED");
                else { a->unsaved = 0u; message(a, "CAL SAVED", "CRC VERIFIED"); }
            } else if (a->menu == 7u) a->page = LCR_PAGE_CONFIRM;
            else a->page = LCR_PAGE_MEASURE;
        }
    } else if (a->page == LCR_PAGE_CONFIRM) {
        if (events & LCR_KEY1_SHORT) a->page = LCR_PAGE_MENU;
        if ((events & LCR_KEY2_SHORT) && !active(a)) {
            if (a->menu == 7u) {
                lcr_cal_profile_nominal(&a->profile, a->profile.board_id, a->profile.fixture_id, a->profile.hse_hz);
                a->unsaved = 1u; a->last.valid = 0u; message(a, "CAL CLEARED IN RAM", "SAVE TO MAKE PERMANENT");
            } else {
                a->working = a->profile;
                a->calibration = a->menu == 2u ? LCR_CAPTURE_CHANNEL : a->menu == 3u ? LCR_CAPTURE_OPEN : LCR_CAPTURE_STANDARD;
                a->cal_frequency = 0u;
                a->cal_range = a->calibration == LCR_CAPTURE_CHANNEL ? 3u :
                                a->calibration == LCR_CAPTURE_STANDARD ? (a->range_mode ? a->range_mode - 1u : a->active_range) : 0u;
                if (a->calibration != LCR_CAPTURE_CHANNEL) {
                    for (unsigned f = 0u; f < 4u; ++f)
                        if (!(a->working.slots[f][a->cal_range].flags & LCR_CAL_CHANNEL)) {
                            message(a, "RUN CHANNEL CAL FIRST", "DISCONNECT DUT"); return;
                        }
                }
                a->cal_samples = 0u; a->cal_sum.re = a->cal_sum.im = 0.0;
                a->last.valid = 0u;
                a->next_start = now; a->page = LCR_PAGE_CAL;
            }
        }
    } else if (a->page == LCR_PAGE_MESSAGE && (events & (LCR_KEY1_SHORT | LCR_KEY2_SHORT))) a->page = LCR_PAGE_MENU;
    else if (a->page == LCR_PAGE_CAL && (events & LCR_KEY2_SHORT)) message(a, "CAL CANCELLED", "PREVIOUS CAL RETAINED");
    if (events) a->dirty = 1u;
}

static lcr_complex divide(lcr_complex a, lcr_complex b) {
    const double norm = b.re * b.re + b.im * b.im;
    const lcr_complex result = {(a.re*b.re+a.im*b.im)/norm, (a.im*b.re-a.re*b.im)/norm};
    return result;
}
static int finish_slot(lcr_app *a) {
    const unsigned f = a->cal_frequency, r = a->cal_range;
    lcr_complex mean = {a->cal_sum.re / 3.0, a->cal_sum.im / 3.0};
    if (!isfinite(mean.re) || !isfinite(mean.im)) return 0;
    lcr_calibration *slot = &a->working.slots[f][r];
    if (a->calibration == LCR_CAPTURE_CHANNEL) {
        for (unsigned i = 0u; i < 6u; ++i) {
            lcr_calibration_nominal(&a->working.slots[f][i], f, i);
            a->working.slots[f][i].alpha = mean;
            a->working.slots[f][i].flags = LCR_CAL_CHANNEL;
            a->working.slots[f][i].revision = a->profile.slots[f][i].revision + 1u;
        }
    } else if (a->calibration == LCR_CAPTURE_OPEN) {
        if(!(slot->flags&LCR_CAL_FIXTURE))slot->zs.re=slot->zs.im=0.0;
        const lcr_complex product = {mean.re*slot->zs.re-mean.im*slot->zs.im,
                                     mean.re*slot->zs.im+mean.im*slot->zs.re};
        const lcr_complex denominator = {1.0-product.re,-product.im};
        if (hypot(denominator.re,denominator.im) < 1e-9) return 0;
        slot->yo = divide(mean, denominator);
        slot->flags |= LCR_CAL_FIXTURE; ++slot->revision;
    } else {
        /* Exact standard R is the displayed nominal Rf. No short is involved. */
        const lcr_complex numerator = {-lcr_nominal_rf[r]*slot->alpha.re,-lcr_nominal_rf[r]*slot->alpha.im};
        const lcr_complex zf = divide(numerator,mean);
        const lcr_complex one = {1.0,0.0}, y = divide(one,zf);
        if (y.re <= 0.0 || y.im < 0.0 || !isfinite(y.re) || !isfinite(y.im)) return 0;
        slot->rf_ohm = 1.0/y.re;
        slot->cf_f = y.im/(6.2831853071795864769*a->session.result.actual_hz);
        if (slot->rf_ohm < 0.8*lcr_nominal_rf[r] || slot->rf_ohm > 1.2*lcr_nominal_rf[r]) return 0;
        slot->flags = (slot->flags | LCR_CAL_FEEDBACK) & ~(uint32_t)LCR_CAL_FIXTURE;
        slot->zs.re = slot->zs.im = slot->yo.re = slot->yo.im = 0.0;
        ++slot->revision;
    }
    const lcr_measure_config check = {f,r,a->working.hse_hz,slot};
    return lcr_measure_config_check(&check) == LCR_OK;
}
static void calibration_result(lcr_app *a, uint32_t now) {
    if(a->cal_samples) {
        const lcr_complex mean={a->cal_sum.re/a->cal_samples,a->cal_sum.im/a->cal_samples};
        double tolerance=0.005*hypot(mean.re,mean.im);
        if(a->calibration==LCR_CAPTURE_OPEN) {
            const lcr_calibration *slot=&a->working.slots[a->cal_frequency][a->cal_range];
            lcr_complex zf;
            const double rf=(slot->flags&LCR_CAL_FEEDBACK)?slot->rf_ohm:lcr_nominal_rf[a->cal_range];
            const double cf=(slot->flags&LCR_CAL_FEEDBACK)?slot->cf_f:0.0;
            if(lcr_feedback(rf,cf,a->session.result.actual_hz,&zf)!=LCR_OK) {
                message(a,"CAL MODEL REJECTED","PREVIOUS CAL RETAINED");return;
            }
            const double v1=hypot(a->session.result.v1.phasor.re,a->session.result.v1.phasor.im);
            tolerance=fmax(tolerance,4.0/(v1*hypot(zf.re,zf.im)));
        }
        if(hypot(a->session.result.raw_z.re-mean.re,a->session.result.raw_z.im-mean.im)>tolerance) {
            message(a,"CAL UNSTABLE","PREVIOUS CAL RETAINED");return;
        }
    }
    a->cal_sum.re += a->session.result.raw_z.re; a->cal_sum.im += a->session.result.raw_z.im;
    ++a->cal_samples;
    if (a->cal_samples == 3u) {
        if (!finish_slot(a)) { message(a, "CAL MODEL REJECTED", "PREVIOUS CAL RETAINED"); return; }
        a->cal_samples = 0u; a->cal_sum.re = a->cal_sum.im = 0.0;
        if (a->calibration == LCR_CAPTURE_OPEN && ++a->cal_range < 6u) { /* next range */ }
        else {
            if (a->calibration == LCR_CAPTURE_OPEN) a->cal_range = 0u;
            ++a->cal_frequency;
        }
        if (a->cal_frequency >= 4u) {
            a->profile = a->working; a->unsaved = 1u;
            message(a, "CAL COMPLETE IN RAM", "MENU: SAVE CAL"); return;
        }
    }
    a->session.state = LCR_SESSION_IDLE;
    a->next_start = now + 40u; a->dirty = 1u;
}
static double score(const lcr_measurement *m) {
    if (m->parameters.kind == LCR_COMPONENT_R) return m->frequency_index == 1u ? 100.0 : 80.0;
    if (m->parameters.kind == LCR_COMPONENT_UNKNOWN) return -100.0;
    return 90.0 - 20.0*fabs(log10(fmax(fabs(m->corrected_z.im),1e-12)/1000.0));
}
static void result(lcr_app *a, uint32_t now) {
    if (a->calibration) { calibration_result(a, now); return; }
    if (a->scan) {
        const double quality = score(&a->session.result);
        if (!a->have_best || quality > a->best_score) {
            a->best = a->session.result; a->best_score = quality; a->have_best = 1u;
        }
        ++a->scan_index;
        if (a->scan_index < 4u) { a->session.state = LCR_SESSION_IDLE; a->next_start = now+40u; a->dirty=1u; return; }
        if (a->best.parameters.kind == LCR_COMPONENT_UNKNOWN) { message(a,"MODEL UNCERTAIN","TRY MANUAL FREQUENCY"); return; }
        a->scan = 0u; a->last = a->best;
    } else {
        if (!a->frequency_mode && a->last.valid &&
            (a->last.parameters.kind != a->session.result.parameters.kind ||
             a->session.result.parameters.magnitude_ohm > 2.0*a->last.parameters.magnitude_ohm ||
             a->session.result.parameters.magnitude_ohm < 0.5*a->last.parameters.magnitude_ohm)) {
            a->scan=1u; a->scan_index=0u; a->have_best=0u;
            lcr_session_stop(&a->session); a->session.keep_excitation=0u;
            a->next_start=now+40u; a->dirty=1u; return;
        }
        a->last = a->session.result;
    }
    a->active_frequency = a->last.frequency_index; a->active_range = a->last.range_index;
    a->session.state = LCR_SESSION_IDLE; a->next_start = now+40u; a->dirty = 1u;
}
static void render(lcr_app *a) {
    if (a->page == LCR_PAGE_MEASURE) lcr_ui_measure(&a->canvas, &a->last, a->model,
        a->scan ? "SCAN" : a->running ? "RUN" : a->last.valid ? "HOLD" : "IDLE", (int)a->details);
    else if (a->page == LCR_PAGE_MENU) {
        char setting[30];
        if (a->menu == 0u) {
            if (!a->frequency_mode) (void)snprintf(setting,sizeof(setting),"AUTO 100-100K HZ");
            else (void)snprintf(setting,sizeof(setting),"%lu HZ",(unsigned long)lcr_frequencies[a->frequency_mode-1u].nominal_hz);
        } else if (a->menu == 1u) {
            if (!a->range_mode) (void)snprintf(setting,sizeof(setting),"AUTO 6 RANGES");
            else (void)snprintf(setting,sizeof(setting),"RF %lu OHM",(unsigned long)lcr_nominal_rf[a->range_mode-1u]);
        } else (void)snprintf(setting,sizeof(setting),"%s",a->unsaved?"UNSAVED CAL":"");
        lcr_ui_message(&a->canvas,"SETTINGS",menu_names[a->menu],setting);
        lcr_canvas_text(&a->canvas,0u,53u,"PB1:NEXT PB2:OK",1u);
    } else if (a->page == LCR_PAGE_CONFIRM) {
        char instruction[32];
        if (a->menu == 4u) (void)snprintf(instruction,sizeof(instruction),"CONNECT %lu OHM STD",
            (unsigned long)lcr_nominal_rf[a->range_mode?a->range_mode-1u:a->active_range]);
        else (void)snprintf(instruction,sizeof(instruction),"%s",a->menu==7u?"CLEAR ALL RAM CAL?":"DISCONNECT DUT");
        lcr_ui_message(&a->canvas,"CONFIRM",instruction,a->menu==3u?"OPEN ONLY / ZS=0":"");
        lcr_canvas_text(&a->canvas,0u,53u,"PB1:CANCEL PB2:START",1u);
    } else if (a->page == LCR_PAGE_CAL) {
        char progress[32];
        (void)snprintf(progress,sizeof(progress),"F%u R%u SAMPLE %u/3",a->cal_frequency+1u,a->cal_range+1u,a->cal_samples+1u);
        lcr_ui_message(&a->canvas,"CALIBRATING",progress,"PB2:CANCEL");
    } else {
        lcr_ui_message(&a->canvas,"LCR STATUS",a->message,a->message2);
        lcr_canvas_text(&a->canvas,0u,53u,"PB1/PB2:BACK",1u);
    }
}
void lcr_app_poll(lcr_app *a, unsigned buttons, uint32_t now) {
    lcr_app_event(a,lcr_keys_poll(&a->keys,buttons,now),now);
    lcr_session_poll(&a->session,now);
    if (a->session.state == LCR_SESSION_DONE) { a->session.state=LCR_SESSION_IDLE; result(a,now); }
    else if (a->session.state == LCR_SESSION_FAILED && (a->running || a->calibration)) {
        if (a->scan && a->session.error == LCR_SESSION_RANGE_LIMIT && ++a->scan_index < 4u) {
            a->session.state=LCR_SESSION_IDLE; a->next_start=now+40u;
        } else if (a->scan && a->session.error == LCR_SESSION_RANGE_LIMIT && a->have_best) {
            a->scan=0u; a->last=a->best; a->active_frequency=a->last.frequency_index;
            a->active_range=a->last.range_index; a->session.state=LCR_SESSION_IDLE; a->next_start=now+40u; a->dirty=1u;
        } else message(a,"MEASUREMENT FAILED",a->calibration?"PREVIOUS CAL RETAINED":"CHECK LOAD / SIGNAL");
    }
    if (!active(a) && (int32_t)(now-a->next_start)>=0 && a->session.state != LCR_SESSION_DONE) {
        if (a->page==LCR_PAGE_CAL && a->calibration) {
            if (!lcr_session_calibrate(&a->session,a->cal_frequency,a->cal_range,
                                      (lcr_capture_kind)a->calibration,a->profile.hse_hz,now))
                message(a,"CAL START FAILED","PREVIOUS CAL RETAINED");
        } else if (a->running && a->page==LCR_PAGE_MEASURE) {
            const unsigned f=a->scan?frequencies[a->scan_index]:a->frequency_mode?a->frequency_mode-1u:a->active_frequency;
            const unsigned r=a->range_mode?a->range_mode-1u:a->active_range;
            a->session.keep_excitation=!a->scan;
            if (!lcr_session_start(&a->session,f,r,!a->range_mode,a->profile.hse_hz,now))
                message(a,"MEASURE START FAILED","CHECK HARDWARE");
        }
    }
    if (a->dirty) render(a);
}
void lcr_app_submit(lcr_app *a, uint32_t token, const uint16_t *pairs, size_t count, uint32_t now) {
    if (lcr_session_submit(&a->session,token,pairs,count,now)) a->dirty=1u;
}
void lcr_app_capture_error(lcr_app *a) { message(a,"ADC / DMA ERROR","NO RESULT ACCEPTED"); }
void lcr_app_halt(lcr_app *a) { stop(a);a->calibration=0u;a->page=LCR_PAGE_MEASURE;a->dirty=1u; }
