#include "lcr_protocol.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "lcr_board.h"
#include "stm32f4xx.h"

#define LCR_PROTO_LINE_MAX 200u
#define LCR_PROTO_FRAME_MAX 320u

static const char *protocol_fw = "0.0.0";
static uint32_t protocol_board;
static uint32_t protocol_fixture = 1u;

static char rx_line[LCR_PROTO_LINE_MAX + 1u];
static unsigned rx_length;

/* Last view sent to the console, so only real changes produce frames. */
static struct {
    uint32_t result_seq;
    unsigned running, freq_mode, range_mode, model, level, unsaved;
    unsigned cal_kind, cal_f, cal_r, cal_samples;
    const char *message;
} mirror;

static unsigned pending_single;

/* ---- small fixed-point/ASCII formatting -------------------------------- */

static void put_text(char **p, const char *text) {
    while (*text) *(*p)++ = *text++;
}
static void put_u64(char **p, uint64_t value) {
    char digits[20];
    unsigned n = 0u;
    do {
        digits[n++] = (char)('0' + (unsigned)(value % 10u));
        value /= 10u;
    } while (value && n < sizeof(digits));
    while (n) *(*p)++ = digits[--n];
}
static void put_i64(char **p, int64_t value) {
    if (value < 0) {
        *(*p)++ = '-';
        put_u64(p, (uint64_t)(-(value + 1)) + 1u);
    } else {
        put_u64(p, (uint64_t)value);
    }
}
static int64_t scaled_i64(double value, double scale) {
    const double x = value * scale;
    if (!isfinite(x)) return 0;
    if (x >= 9.0e18) return 9000000000000000000LL;
    if (x <= -9.0e18) return -9000000000000000000LL;
    return (int64_t)(x >= 0.0 ? x + 0.5 : x - 0.5);
}
static uint64_t scaled_u64(double value, double scale) {
    const double x = value * scale;
    if (!isfinite(x) || x <= 0.0) return 0u;
    if (x >= 1.8e19) return 18000000000000000000ULL;
    return (uint64_t)(x + 0.5);
}
static void put_hex(char **p, uint32_t value, unsigned digits) {
    static const char hex[] = "0123456789ABCDEF";
    for (unsigned i = digits; i > 0u; --i)
        *(*p)++ = hex[(value >> (4u * (i - 1u))) & 0xFu];
}
static void put_hex_auto(char **p, uint32_t value) {
    unsigned shift = 28u;
    while (shift > 0u && ((value >> shift) & 0xFu) == 0u) shift -= 4u;
    put_hex(p, value, shift / 4u + 1u);
}
static void field_u(char **p, uint64_t value) {
    *(*p)++ = ',';
    put_u64(p, value);
}
static void field_i(char **p, int64_t value) {
    *(*p)++ = ',';
    put_i64(p, value);
}
static void field_s(char **p, const char *text) {
    *(*p)++ = ',';
    put_text(p, text);
}
static char *frame_begin(char *buffer, const char *type) {
    char *p = buffer;
    *p++ = '$';
    put_text(&p, type);
    return p;
}
static void frame_send(char *buffer, char *end) {
    static const char hex[] = "0123456789ABCDEF";
    unsigned char sum = 0u;
    for (char *q = buffer + 1; q < end; ++q) sum ^= (unsigned char)*q;
    *end++ = '*';
    *end++ = hex[sum >> 4];
    *end++ = hex[sum & 0xFu];
    *end++ = '\r';
    *end++ = '\n';
    *end = '\0';
    (void)lcr_board_puts(buffer);
}

/* ---- device -> PC frames ------------------------------------------------ */

static void send_frame_error(unsigned code, const char *text) {
    char buffer[LCR_PROTO_FRAME_MAX];
    char *p = frame_begin(buffer, "ERR");
    field_u(&p, code);
    field_s(&p, text);
    frame_send(buffer, p);
}
static void send_frame_ack(const char *command) {
    char buffer[LCR_PROTO_FRAME_MAX];
    char *p = frame_begin(buffer, "ACK");
    field_s(&p, command);
    frame_send(buffer, p);
}
static void send_frame_msg(const char *text) {
    char buffer[LCR_PROTO_FRAME_MAX];
    char *p = frame_begin(buffer, "MSG");
    field_s(&p, text);
    frame_send(buffer, p);
}
static void send_frame_state(lcr_app *app, int force) {
    const unsigned running = app->running ? 1u : 0u;
    const unsigned level = lcr_app_calibration_level(app);
    if (!force && running == mirror.running &&
        app->frequency_mode == mirror.freq_mode &&
        app->range_mode == mirror.range_mode &&
        (unsigned)app->model == mirror.model &&
        level == mirror.level && app->unsaved == mirror.unsaved)
        return;
    mirror.running = running;
    mirror.freq_mode = app->frequency_mode;
    mirror.range_mode = app->range_mode;
    mirror.model = (unsigned)app->model;
    mirror.level = level;
    mirror.unsaved = app->unsaved;
    char buffer[LCR_PROTO_FRAME_MAX];
    char *p = frame_begin(buffer, "STATE");
    field_u(&p, running);
    field_u(&p, app->frequency_mode);
    field_u(&p, app->range_mode);
    field_u(&p, (unsigned)app->model);
    field_u(&p, level);
    field_u(&p, app->unsaved);
    frame_send(buffer, p);
}
static void send_frame_result(lcr_app *app) {
    const lcr_measurement *m = &app->last;
    const lcr_parameters *q = &m->parameters;
    int64_t value = 0;
    unsigned unit = 0u;
    if (q->kind == LCR_COMPONENT_R) {
        value = scaled_i64(q->magnitude_ohm, 1.0e6);
        unit = 0u;
    } else if (q->kind == LCR_COMPONENT_C) {
        value = scaled_i64((unsigned)app->model == 1u ? q->cp_f : q->cs_f, 1.0e15);
        unit = 1u;
    } else if (q->kind == LCR_COMPONENT_L) {
        value = scaled_i64((unsigned)app->model == 1u ? q->lp_h : q->ls_h, 1.0e12);
        unit = 2u;
    }
    char buffer[LCR_PROTO_FRAME_MAX];
    char *p = frame_begin(buffer, "RES");
    field_u(&p, app->result_seq);
    field_u(&p, lcr_board_millis());
    field_u(&p, scaled_u64(m->actual_hz, 1000.0));
    field_u(&p, m->frequency_index);
    field_u(&p, m->range_index);
    field_u(&p, scaled_u64(lcr_nominal_rf[m->range_index], 1000.0));
    field_u(&p, (unsigned)m->status);
    *p++ = ',';
    put_hex_auto(&p, m->calibration_flags);
    *p++ = ',';
    put_hex(&p, 0u, 8u);
    field_u(&p, scaled_u64(q->magnitude_ohm, 1.0e6));
    field_i(&p, scaled_i64(q->phase_deg, 1000.0));
    field_i(&p, scaled_i64(q->rs_ohm, 1.0e6));
    field_i(&p, scaled_i64(q->xs_ohm, 1.0e6));
    field_u(&p, (unsigned)q->kind);
    field_i(&p, value);
    field_u(&p, unit);
    field_i(&p, scaled_i64(q->d, 1.0e6));
    field_i(&p, scaled_i64(q->q, 1.0e6));
    field_u(&p, (unsigned)app->model);
    frame_send(buffer, p);
}
static void send_frame_cal(lcr_app *app, unsigned kind, unsigned done, unsigned code) {
    char buffer[LCR_PROTO_FRAME_MAX];
    char *p = frame_begin(buffer, "CAL");
    field_u(&p, kind);
    field_u(&p, app->cal_frequency);
    field_u(&p, app->cal_range);
    field_u(&p, kind == 1u ? 3u : app->cal_samples);
    field_u(&p, done);
    field_u(&p, code);
    frame_send(buffer, p);
}
void lcr_protocol_boot(lcr_app *app) {
    char buffer[LCR_PROTO_FRAME_MAX];
    char *p = frame_begin(buffer, "BOOT");
    field_u(&p, 1u);
    *p++ = ',';
    put_hex(&p, protocol_board, 8u);
    field_s(&p, protocol_fw);
    field_u(&p, protocol_fixture);
    field_u(&p, lcr_app_calibration_level(app));
    field_u(&p, LCR_FREQUENCY_COUNT);
    field_u(&p, LCR_RANGE_COUNT);
    frame_send(buffer, p);
    mirror.running = 0xFFFFFFFFu;
    mirror.message = 0;
    send_frame_state(app, 1);
}

/* ---- helpers ------------------------------------------------------------ */

static int parse_u64(const char *text, uint64_t *out) {
    uint64_t value = 0u;
    unsigned digits = 0u;
    if (!text || !*text) return 0;
    while (*text) {
        if (*text < '0' || *text > '9') return 0;
        if (value > (UINT64_MAX - 9u) / 10u) return 0;
        value = value * 10u + (uint64_t)(*text - '0');
        ++text;
        ++digits;
    }
    if (!digits) return 0;
    *out = value;
    return 1;
}
static void join_message(char *out, size_t capacity, const char *first, const char *second) {
    size_t n = 0u;
    for (const char *s = first; s && *s && n + 1u < capacity; ++s) out[n++] = *s;
    if (second && *second && n + 4u < capacity) {
        out[n++] = ' '; out[n++] = '|'; out[n++] = ' ';
        for (const char *s = second; *s && n + 1u < capacity; ++s) out[n++] = *s;
    }
    out[n] = '\0';
}

/* ---- PC -> device commands --------------------------------------------- */

static void send_help(void) {
    send_frame_msg("commands: ? M C1 C0 F0-4 R0-6 S0-1 CAL,CH|OPEN|STD <uohm>|SAVE|CLEAR|ABORT RAW,0-1 RESET H");
}
static void handle_command(lcr_app *app, const char *command) {
    const uint32_t now = lcr_board_millis();
    if (command[0] == '\0') return;
    if (!strcmp(command, "?")) { lcr_protocol_boot(app); return; }
    if (!strcmp(command, "PING")) { send_frame_ack("PING"); send_frame_state(app, 1); return; }
    if (!strcmp(command, "M")) {
        if (!lcr_app_start_measure(app, now)) { send_frame_error(102u, "BUSY"); return; }
        pending_single = 1u;
        send_frame_ack("M");
        send_frame_state(app, 1);
        return;
    }
    if (!strcmp(command, "C1")) {
        if (!app->running && !lcr_app_start_measure(app, now)) { send_frame_error(102u, "BUSY"); return; }
        pending_single = 0u;
        send_frame_ack("C1");
        send_frame_state(app, 1);
        return;
    }
    if (!strcmp(command, "C0")) {
        (void)lcr_app_stop_measure(app);
        pending_single = 0u;
        send_frame_ack("C0");
        send_frame_state(app, 1);
        return;
    }
    if ((command[0] == 'F' || command[0] == 'R') && command[1] >= '0' &&
        command[1] <= '9' && command[2] == '\0') {
        const unsigned mode = (unsigned)(command[1] - '0');
        if (command[0] == 'F') {
            if (mode > 4u) { send_frame_error(100u, "FREQ RANGE"); return; }
            lcr_app_set_frequency_mode(app, mode);
        } else {
            if (mode > 6u) { send_frame_error(100u, "RANGE RANGE"); return; }
            lcr_app_set_range_mode(app, mode);
        }
        send_frame_ack(command);
        send_frame_state(app, 1);
        return;
    }
    if (!strcmp(command, "S0") || !strcmp(command, "S1")) {
        lcr_app_set_model(app, command[1] == '1' ? 1u : 0u);
        send_frame_ack(command);
        send_frame_state(app, 1);
        return;
    }
    if (!strcmp(command, "CAL,CH") || !strcmp(command, "CAL,OPEN")) {
        const unsigned kind = command[4] == 'C' ? LCR_CAPTURE_CHANNEL : LCR_CAPTURE_OPEN;
        if (!lcr_app_cal_start(app, kind, 0.0, now)) {
            send_frame_error(102u, "CAL BUSY OR PREREQUISITE");
            return;
        }
        send_frame_ack(command);
        send_frame_state(app, 1);
        return;
    }
    if (!strncmp(command, "CAL,STD,", 8u)) {
        uint64_t micro_ohm = 0u;
        if (!parse_u64(command + 8u, &micro_ohm) || micro_ohm < 1000000u || micro_ohm > 10000000000000ULL) {
            send_frame_error(100u, "STD 1..10M OHM IN UOHM");
            return;
        }
        if (!lcr_app_cal_start(app, LCR_CAPTURE_STANDARD, (double)micro_ohm * 1.0e-6, now)) {
            send_frame_error(102u, "CAL BUSY OR PREREQUISITE");
            return;
        }
        send_frame_ack("CAL,STD");
        send_frame_state(app, 1);
        return;
    }
    if (!strcmp(command, "CAL,SAVE")) {
        if (!lcr_app_cal_save(app)) { send_frame_error(10u, "CAL SAVE FAILED"); return; }
        send_frame_ack("CAL,SAVE");
        send_frame_state(app, 1);
        return;
    }
    if (!strcmp(command, "CAL,CLEAR")) {
        (void)lcr_app_cal_clear(app);
        send_frame_ack("CAL,CLEAR");
        send_frame_state(app, 1);
        return;
    }
    if (!strcmp(command, "CAL,ABORT")) {
        if (!lcr_app_cal_abort(app)) { send_frame_error(100u, "NO CAL RUNNING"); return; }
        send_frame_ack("CAL,ABORT");
        send_frame_state(app, 1);
        return;
    }
    if (!strcmp(command, "RAW,0") || !strcmp(command, "RAW,1")) {
        send_frame_ack(command);
        return;
    }
    if (!strcmp(command, "RESET")) {
        send_frame_ack("RESET");
        NVIC_SystemReset();
        return;
    }
    if (!strcmp(command, "H")) { send_frame_ack("H"); send_help(); return; }
    /* Bench compatibility with the original key console. */
    if (!strcmp(command, "1") || !strcmp(command, "2")) {
        lcr_app_event(app, LCR_KEY_SHORT, now);
        send_frame_ack(command);
        return;
    }
    if (!strcmp(command, "b")) { lcr_app_event(app, LCR_KEY_LONG, now); send_frame_ack("b"); return; }
    if (!strcmp(command, "d")) { lcr_app_event(app, LCR_KEY_DOUBLE, now); send_frame_ack("d"); return; }
    if (!strcmp(command, "s")) { lcr_app_halt(app); send_frame_ack("s"); return; }
    send_frame_error(100u, "BAD COMMAND");
}

/* ---- public API --------------------------------------------------------- */

void lcr_protocol_init(lcr_app *app, const char *fw_version, uint32_t board_id,
                       uint32_t fixture_id) {
    (void)app;
    protocol_fw = fw_version ? fw_version : "0.0.0";
    protocol_board = board_id;
    protocol_fixture = fixture_id;
    rx_length = 0u;
    pending_single = 0u;
    memset(&mirror, 0, sizeof(mirror));
    mirror.running = 0xFFFFFFFFu;
    mirror.cal_samples = 0xFFFFFFFFu;
}
void lcr_protocol_poll(lcr_app *app, uint32_t now) {
    (void)now;
    if (!app) return;
    for (unsigned budget = 0u; budget < 32u; ++budget) {
        const int ch = lcr_board_getchar();
        if (ch == -1) break;
        if (ch == -2) { rx_length = 0u; continue; }
        if (ch == '\r') continue;
        if (ch == '\n') {
            if (rx_length) { rx_line[rx_length] = '\0'; handle_command(app, rx_line); rx_length = 0u; }
            continue;
        }
        if (ch == 8 || ch == 127) { if (rx_length) --rx_length; continue; }
        if (rx_length < LCR_PROTO_LINE_MAX) rx_line[rx_length++] = (char)ch;
        else rx_length = 0u;
    }
    if (app->result_seq != mirror.result_seq) {
        mirror.result_seq = app->result_seq;
        if (app->last.valid) send_frame_result(app);
        if (pending_single) {
            pending_single = 0u;
            (void)lcr_app_stop_measure(app);
        }
    }
    send_frame_state(app, 0);
    const unsigned cal_kind = app->calibration == LCR_CAPTURE_CHANNEL ? 1u :
                              app->calibration == LCR_CAPTURE_OPEN ? 2u :
                              app->calibration == LCR_CAPTURE_STANDARD ? 3u : 0u;
    if (cal_kind) {
        if (cal_kind != mirror.cal_kind || app->cal_frequency != mirror.cal_f ||
            app->cal_range != mirror.cal_r || app->cal_samples != mirror.cal_samples) {
            mirror.cal_kind = cal_kind;
            mirror.cal_f = app->cal_frequency;
            mirror.cal_r = app->cal_range;
            mirror.cal_samples = app->cal_samples;
            send_frame_cal(app, cal_kind, 0u, 0u);
        }
    } else if (mirror.cal_kind) {
        const char *text = app->message ? app->message : "";
        const unsigned code = (strstr(text, "COMPLETE") || strstr(text, "SAVED")) ? 0u : 1u;
        send_frame_cal(app, mirror.cal_kind, 1u, code);
        mirror.cal_kind = 0u;
        mirror.cal_samples = 0u;
    }
    if (app->page == LCR_PAGE_MESSAGE && app->message && app->message != mirror.message) {
        char text[128];
        mirror.message = app->message;
        join_message(text, sizeof(text), app->message, app->message2);
        send_frame_msg(text);
    } else if (app->page != LCR_PAGE_MESSAGE) {
        /* Leaving the status page re-arms the same message (e.g. the auto
           retry failing again), so the console sees every transition. */
        mirror.message = 0;
    }
}
