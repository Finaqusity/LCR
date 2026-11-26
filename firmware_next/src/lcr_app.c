#include "lcr_app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "lcr_config.h"

/* Implemented below; used by the settings menu before its definition. */
int lcr_app_cal_save(lcr_app *a);

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
    /* A terminal state must not block the next start: the page above the
       session (message / idle) owns the user-visible state from here on. */
    if (a->session.state == LCR_SESSION_FAILED ||
        a->session.state == LCR_SESSION_CANCELLED ||
        a->session.state == LCR_SESSION_DONE)
        a->session.state = LCR_SESSION_IDLE;
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
static void reset_filter(lcr_app *a) {
    a->smooth_n = 0u;
    a->stable_kind = LCR_COMPONENT_UNKNOWN;
    a->kind_streak = 0u;
}
static void start_measure(lcr_app *a, uint32_t now) {
    a->page = LCR_PAGE_MEASURE;
    a->running = 1u;
    a->scan = a->frequency_mode == 0u;
    a->scan_index = 0u; a->have_best = 0u;
    reset_filter(a);
    a->next_start = now;
    a->dirty = 1u;
}
static void menu_activate(lcr_app *a) {
    if (a->menu == 0u) a->frequency_mode = (a->frequency_mode + 1u) % 5u;
    else if (a->menu == 1u) a->range_mode = (a->range_mode + 1u) % 7u;
    else if (a->menu >= 2u && a->menu <= 4u) a->page = LCR_PAGE_CONFIRM;
    else if (a->menu == 5u) message(a, "SHORT NOT VALIDATED", "NO EXCITATION STARTED");
    else if (a->menu == 6u) (void)lcr_app_cal_save(a);
    else if (a->menu == 7u) a->page = LCR_PAGE_CONFIRM;
    else a->page = LCR_PAGE_MEASURE;
}
/* Start a calibration sweep; shared by the key UI and the serial console. */
static int cal_begin(lcr_app *a, unsigned kind, uint32_t now) {
    if (!a || active(a)) return 0;
    a->working = a->profile;
    a->calibration = kind;
    a->cal_frequency = 0u;
    a->cal_range = kind == LCR_CAPTURE_CHANNEL ? 3u :
                    kind == LCR_CAPTURE_STANDARD ? (a->range_mode ? a->range_mode - 1u : a->active_range) : 0u;
    if (kind != LCR_CAPTURE_CHANNEL) {
        for (unsigned f = 0u; f <= LCR_CAL_MAX_FREQUENCY_INDEX; ++f)
            if (!(a->working.slots[f][a->cal_range].flags & LCR_CAL_CHANNEL)) {
                message(a, "RUN CHANNEL CAL FIRST", "DISCONNECT DUT"); return 0;
            }
    }
    a->cal_samples = 0u; a->cal_sum.re = a->cal_sum.im = 0.0;
    a->last.valid = 0u;
    a->next_start = now; a->page = LCR_PAGE_CAL; a->dirty = 1u;
    return 1;
}
static void confirm_execute(lcr_app *a, uint32_t now) {
    if (a->menu == 7u) { (void)lcr_app_cal_clear(a); return; }
    (void)cal_begin(a, a->menu == 2u ? LCR_CAPTURE_CHANNEL : a->menu == 3u ? LCR_CAPTURE_OPEN : LCR_CAPTURE_STANDARD, now);
}
/* One physical key (WK_UP) with three gestures:
 *   MEASURE  short = start/stop, long = settings, double = series/parallel
 *   MENU     short = next item, long = activate, double = back to measure
 *   CONFIRM  short = cancel, long = execute
 *   MESSAGE  short = retry measurement, long = back to menu
 *   CAL      short = cancel calibration, previous calibration retained */
void lcr_app_event(lcr_app *a, unsigned events, uint32_t now) {
    if (!a) return;
    if (a->page == LCR_PAGE_MEASURE) {
        if (events & LCR_KEY_LONG) {
            stop(a); a->calibration = 0u; a->page = LCR_PAGE_MENU;
        } else if (events & LCR_KEY_DOUBLE) {
            a->model = a->model == LCR_MODEL_SERIES ? LCR_MODEL_PARALLEL : LCR_MODEL_SERIES;
        } else if (events & LCR_KEY_SHORT) {
            if (a->running) stop(a); else start_measure(a, now);
        }
    } else if (a->page == LCR_PAGE_MENU) {
        if (events & LCR_KEY_DOUBLE) a->page = LCR_PAGE_MEASURE;
        else if ((events & LCR_KEY_LONG) && !active(a)) menu_activate(a);
        else if (events & LCR_KEY_SHORT) a->menu = (a->menu + 1u) % 9u;
    } else if (a->page == LCR_PAGE_CONFIRM) {
        if ((events & LCR_KEY_LONG) && !active(a)) confirm_execute(a, now);
        else if (events & LCR_KEY_SHORT) a->page = LCR_PAGE_MENU;
    } else if (a->page == LCR_PAGE_MESSAGE) {
        if (events & LCR_KEY_LONG) a->page = LCR_PAGE_MENU;
        else if (events & LCR_KEY_SHORT) start_measure(a, now);
    } else if (a->page == LCR_PAGE_CAL && (events & LCR_KEY_SHORT)) {
        message(a, "CAL CANCELLED", "PREVIOUS CAL RETAINED");
    }
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
        /* The displayed nominal Rf is replaced by the real standard value the
           operator entered (web console `CAL,STD,<uohm>`) when it is known. */
        const double standard = a->cal_standard_ohm > 0.0 ? a->cal_standard_ohm : lcr_nominal_rf[r];
        const lcr_complex numerator = {-standard*slot->alpha.re,-standard*slot->alpha.im};
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
/* Move the open-standard sweep to its next (frequency, range) pair. */
static void cal_advance(lcr_app *a, uint32_t now) {
    a->cal_samples = 0u; a->cal_sum.re = a->cal_sum.im = 0.0;
    if (a->calibration == LCR_CAPTURE_OPEN && ++a->cal_range < 6u) { /* next range */ }
    else {
        if (a->calibration == LCR_CAPTURE_OPEN) {
            /* The fixture itself (parallel stray impedance) is the same on
               every range; copy the best range's measured admittance to the
               ranges whose V2 was too small to measure reliably. */
            unsigned f = a->cal_frequency; int best = -1;
            for (int r = 5; r >= 0; --r)
                if (a->working.slots[f][r].flags & LCR_CAL_FIXTURE) { best = r; break; }
            if (best >= 0)
                for (unsigned r = 0u; r < 6u; ++r)
                    if (!(a->working.slots[f][r].flags & LCR_CAL_FIXTURE)) {
                        a->working.slots[f][r].yo = a->working.slots[f][best].yo;
                        a->working.slots[f][r].flags |= LCR_CAL_FIXTURE;
                        ++a->working.slots[f][r].revision;
                    }
            a->cal_range = 0u;
        }
        ++a->cal_frequency;
    }
    if (a->cal_frequency >= 4u) {
        a->profile = a->working; a->unsaved = 1u;
        message(a, "CAL COMPLETE IN RAM", "MENU: SAVE CAL"); return;
    }
    a->session.state = LCR_SESSION_IDLE;
    a->next_start = now + 40u; a->dirty = 1u;
}
/* Skip a whole frequency that the excitation source cannot drive.  The OPEN
   sweep advances range by range, so jump the range cursor past 5 to let
   cal_advance() roll over to the next frequency (and copy the fixture terms
   for the unsupported one). */
static void cal_skip_frequency(lcr_app *a, uint32_t now) {
    a->cal_range = 6u;
    cal_advance(a, now);
}
static void calibration_result(lcr_app *a, uint32_t now) {
    /* Bands above the calibrated-excitation limit cannot be measured with the
       present source (see LCR_CAL_MAX_FREQUENCY_INDEX); skip them so the rest
       of the sweep still completes. */
    if (a->cal_frequency > LCR_CAL_MAX_FREQUENCY_INDEX) { cal_skip_frequency(a, now); return; }
    /* An open fixture has almost no current on the low ranges: V2 is below
       the noise floor and the raw impedance is meaningless. Skip those pairs
       instead of treating them as unstable and aborting the whole sweep. */
    if (a->calibration == LCR_CAPTURE_OPEN) {
        const unsigned f = a->cal_frequency, r = a->cal_range;
        const lcr_calibration *cur = &a->profile.slots[f][r];
        const double rf = (cur->flags & LCR_CAL_FEEDBACK) ? cur->rf_ohm : lcr_nominal_rf[r];
        const double v2 = hypot(a->session.result.v2.phasor.re, a->session.result.v2.phasor.im);
        /* For the open workflow the frame stores the fixture admittance. */
        const double y = hypot(a->session.result.raw_z.re, a->session.result.raw_z.im);
        const double z = y > 1e-15 ? 1.0 / y : 0.0;
        const double rho = rf > 0.0 ? z / rf : 0.0;
        const int clipped = a->session.result.v2.clipped ||
                            a->session.result.status == LCR_CLIPPED;
        if (clipped || v2 < 100.0 || rho < 1.0 || rho > 100.0) { cal_advance(a, now); return; }
    }
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
            /* High ranges see very little V2 on an open fixture; the raw open
               impedance is dominated by ADC noise there, so require only that
               the three samples agree in magnitude before averaging. */
            if(a->cal_range>=3u) tolerance=fmax(tolerance,0.05*hypot(mean.re,mean.im));
        }
        if(hypot(a->session.result.raw_z.re-mean.re,a->session.result.raw_z.im-mean.im)>tolerance) {
            message(a,"CAL UNSTABLE","PREVIOUS CAL RETAINED");return;
        }
    }
    a->cal_sum.re += a->session.result.raw_z.re; a->cal_sum.im += a->session.result.raw_z.im;
    ++a->cal_samples;
    if (a->cal_samples == 3u) {
        if (!finish_slot(a)) { message(a, "CAL MODEL REJECTED", "PREVIOUS CAL RETAINED"); return; }
        cal_advance(a, now);
        return;
    }
    a->session.state = LCR_SESSION_IDLE;
    a->next_start = now + 40u; a->dirty = 1u;
}
static double score(const lcr_measurement *m) {
    if (m->parameters.kind == LCR_COMPONENT_R) return m->frequency_index == 1u ? 100.0 : 80.0;
    if (m->parameters.kind == LCR_COMPONENT_UNKNOWN) return -100.0;
    return 90.0 - 20.0*fabs(log10(fmax(fabs(m->corrected_z.im),1e-12)/1000.0));
}
static double median_of(const double *v, unsigned n) {
    double tmp[3];
    unsigned i, j;
    for (i = 0u; i < n; ++i) tmp[i] = v[i];
    for (i = 1u; i < n; ++i) {
        const double key = tmp[i];
        for (j = i; j > 0u && tmp[j - 1u] > key; --j) tmp[j] = tmp[j - 1u];
        tmp[j] = key;
    }
    return n & 1u ? tmp[n / 2u] : 0.5 * (tmp[n / 2u - 1u] + tmp[n / 2u]);
}

/* Median of the last few complex results on the same frequency/range plus a
 * two-sample hysteresis on R/L/C.  The V2 channel noise (~130 codes) makes a
 * single phase estimate jitter around the 1.15 degree R threshold, which used
 * to flip the displayed component and retrigger the AUTO frequency scan on
 * every measurement. */
static void stabilise_result(lcr_app *a) {
    lcr_measurement *m = &a->last;
    lcr_parameters p;
    double re[3], im[3];
    lcr_complex z;
    unsigned i;
    if (!m->valid || m->parameters.kind == LCR_COMPONENT_UNKNOWN) {
        a->smooth_n = 0u;
        return;
    }
    if (a->smooth_n == 0u || m->frequency_index != a->smooth_frequency ||
        m->range_index != a->smooth_range) {
        a->smooth_frequency = m->frequency_index;
        a->smooth_range = m->range_index;
        a->smooth_n = 0u;
    }
    if (a->smooth_n == 3u) {
        a->smooth_hist[0] = a->smooth_hist[1];
        a->smooth_hist[1] = a->smooth_hist[2];
        --a->smooth_n;
    }
    a->smooth_hist[a->smooth_n++] = m->corrected_z;
    for (i = 0u; i < a->smooth_n; ++i) {
        re[i] = a->smooth_hist[i].re;
        im[i] = a->smooth_hist[i].im;
    }
    z.re = median_of(re, a->smooth_n);
    z.im = median_of(im, a->smooth_n);
    if (lcr_parameters_from_z(z, m->actual_hz, &p) != LCR_OK) return;
    /* Near-zero phase is a resistor even when the V2 noise pushes |phi| past
       the classifier threshold for one sample. Real L/C parts sit far away
       (a capacitor with D=10 is already at -5.7 deg), so a +-6 deg deadband
       keeps the type letter stable without hiding inductors or capacitors. */
    if (p.kind != LCR_COMPONENT_R && fabs(p.phase_deg) < 6.0) p.kind = LCR_COMPONENT_R;
    if (p.kind == a->stable_kind) {
        a->kind_streak = 0u;
    } else if (a->stable_kind == LCR_COMPONENT_UNKNOWN) {
        a->stable_kind = p.kind;
        a->kind_streak = 0u;
    } else if (++a->kind_streak >= 2u) {
        a->stable_kind = p.kind;
        a->kind_streak = 0u;
    }
    if (a->stable_kind != LCR_COMPONENT_UNKNOWN) p.kind = a->stable_kind;
    m->parameters = p;
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
            (a->session.result.parameters.magnitude_ohm > 4.0*a->last.parameters.magnitude_ohm ||
             a->session.result.parameters.magnitude_ohm < 0.25*a->last.parameters.magnitude_ohm)) {
            a->scan=1u; a->scan_index=0u; a->have_best=0u;
            lcr_session_stop(&a->session); a->session.keep_excitation=0u;
            a->next_start=now+40u; a->dirty=1u; return;
        }
        a->last = a->session.result;
    }
    stabilise_result(a);
    a->active_frequency = a->last.frequency_index; a->active_range = a->last.range_index;
    ++a->result_seq;
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
        lcr_canvas_text(&a->canvas,0u,53u,"KEY:NEXT HOLD:OK",1u);
    } else if (a->page == LCR_PAGE_CONFIRM) {
        char instruction[32];
        if (a->menu == 4u) (void)snprintf(instruction,sizeof(instruction),"CONNECT %lu OHM STD",
            (unsigned long)lcr_nominal_rf[a->range_mode?a->range_mode-1u:a->active_range]);
        else (void)snprintf(instruction,sizeof(instruction),"%s",a->menu==7u?"CLEAR ALL RAM CAL?":"DISCONNECT DUT");
        lcr_ui_message(&a->canvas,"CONFIRM",instruction,a->menu==3u?"OPEN ONLY / ZS=0":"");
        lcr_canvas_text(&a->canvas,0u,53u,"KEY:CANCEL HOLD:GO",1u);
    } else if (a->page == LCR_PAGE_CAL) {
        char progress[32];
        (void)snprintf(progress,sizeof(progress),"F%u R%u SAMPLE %u/3",a->cal_frequency+1u,a->cal_range+1u,a->cal_samples+1u);
        lcr_ui_message(&a->canvas,"CALIBRATING",progress,"KEY:CANCEL");
    } else {
        lcr_ui_message(&a->canvas,"LCR STATUS",a->message,a->message2);
        lcr_canvas_text(&a->canvas,0u,53u,"KEY:BACK",1u);
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
        } else if (a->session.error == LCR_SESSION_NO_EXCITATION)
            message(a,"NO EXCITATION","CHECK TP1 / DRIVE");
        else message(a,"MEASUREMENT FAILED",a->calibration?"PREVIOUS CAL RETAINED":"CHECK LOAD / SIGNAL");
    }
    if (!active(a) && (int32_t)(now-a->next_start)>=0 && a->session.state != LCR_SESSION_DONE) {
        if (a->page==LCR_PAGE_CAL && a->calibration) {
            if (a->cal_frequency > LCR_CAL_MAX_FREQUENCY_INDEX) {
                cal_skip_frequency(a, now);
            } else if (!lcr_session_calibrate(&a->session,a->cal_frequency,a->cal_range,
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

/* ---- Serial-console API -------------------------------------------------- */

void lcr_app_set_frequency_mode(lcr_app *a, unsigned mode) {
    if (!a || mode > 4u) return;
    a->frequency_mode = mode; reset_filter(a); a->dirty = 1u;
}
void lcr_app_set_range_mode(lcr_app *a, unsigned mode) {
    if (!a || mode > 6u) return;
    a->range_mode = mode; reset_filter(a); a->dirty = 1u;
}
void lcr_app_set_model(lcr_app *a, unsigned parallel) {
    if (!a) return;
    a->model = parallel ? LCR_MODEL_PARALLEL : LCR_MODEL_SERIES; a->dirty = 1u;
}
int lcr_app_start_measure(lcr_app *a, uint32_t now) {
    if (!a || active(a)) return 0;
    start_measure(a, now); return 1;
}
int lcr_app_stop_measure(lcr_app *a) {
    if (!a) return 0;
    stop(a); a->dirty = 1u; return 1;
}
int lcr_app_cal_start(lcr_app *a, unsigned kind, double standard_ohm, uint32_t now) {
    if (!a) return 0;
    if (kind != LCR_CAPTURE_CHANNEL && kind != LCR_CAPTURE_OPEN && kind != LCR_CAPTURE_STANDARD) return 0;
    if (active(a) || a->calibration) return 0;
    a->cal_standard_ohm = standard_ohm > 0.0 ? standard_ohm : 0.0;
    return cal_begin(a, kind, now);
}
int lcr_app_cal_save(lcr_app *a) {
    if (!a || active(a)) return 0;
    if (a->session.frontend.state != LCR_FE_MUTED || !a->io.save ||
        !a->io.save(a->io.context, &a->profile)) {
        message(a, "SAVE FAILED", "RAM CAL RETAINED"); return 0;
    }
    a->unsaved = 0u; message(a, "CAL SAVED", "CRC VERIFIED"); return 1;
}
int lcr_app_cal_clear(lcr_app *a) {
    if (!a) return 0;
    lcr_cal_profile_nominal(&a->profile, a->profile.board_id, a->profile.fixture_id, a->profile.hse_hz);
    a->unsaved = 1u; a->last.valid = 0u;
    message(a, "CAL CLEARED IN RAM", "SAVE TO MAKE PERMANENT");
    return 1;
}
int lcr_app_cal_abort(lcr_app *a) {
    if (!a || !a->calibration) return 0;
    message(a, "CAL CANCELLED", "PREVIOUS CAL RETAINED"); return 1;
}
unsigned lcr_app_calibration_level(const lcr_app *a) {
    if (!a) return 0u;
    unsigned channel = 1u, fixture = 0u, feedback = 0u, complete = 1u;
    for (unsigned f = 0u; f < LCR_FREQUENCY_COUNT; ++f)
        for (unsigned r = 0u; r < LCR_RANGE_COUNT; ++r) {
            const uint32_t flags = a->profile.slots[f][r].flags;
            if (!(flags & LCR_CAL_CHANNEL)) channel = 0u;
            if (flags & LCR_CAL_FIXTURE) fixture = 1u;
            if (flags & LCR_CAL_FEEDBACK) feedback = 1u;
            if ((flags & LCR_CAL_ALL) != LCR_CAL_ALL) complete = 0u;
        }
    unsigned level = channel ? 1u : 0u;
    if (level && (fixture || feedback)) level = 2u;
    if (level >= 2u && fixture && feedback) level = 3u;
    if (level == 3u && complete) level = 4u;
    return level;
}
