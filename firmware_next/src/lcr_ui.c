#include "lcr_ui.h"
#include "lcr_font_data.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void lcr_keys_init(lcr_keys *keys, unsigned raw, uint32_t now) {
    memset(keys, 0, sizeof(*keys));
    for (unsigned i = 0u; i < 2u; ++i) {
        keys->keys[i].candidate = keys->keys[i].stable = (uint8_t)((raw >> i) & 1u);
        keys->keys[i].blocked = keys->keys[i].stable;
        keys->keys[i].changed = now;
    }
}

unsigned lcr_keys_poll(lcr_keys *keys, unsigned raw, uint32_t now) {
    unsigned events = 0u;
    if ((raw & 3u) == 3u) keys->keys[0].blocked = keys->keys[1].blocked = 1u;
    for (unsigned i = 0u; i < 2u; ++i) {
        lcr_key *key = &keys->keys[i];
        const uint8_t down = (uint8_t)((raw >> i) & 1u);
        if (down != key->candidate) { key->candidate = down; key->changed = now; }
        if (key->candidate != key->stable && now - key->changed >= 25u) {
            key->stable = key->candidate;
            if (key->stable) { key->pressed = now; key->long_sent = 0u; }
            else {
                if (!key->blocked && !key->long_sent) events |= 1u << i;
                key->blocked = 0u;
            }
        }
        if (key->stable && !key->blocked && !key->long_sent && now - key->pressed >= 800u) {
            key->long_sent = 1u;
            events |= 4u << i;
        }
    }
    return events;
}

void lcr_canvas_clear(lcr_canvas *canvas) { memset(canvas, 0, sizeof(*canvas)); }
static void pixel(lcr_canvas *canvas, unsigned x, unsigned y) {
    if (x < 128u && y < 64u) canvas->pixels[(y / 8u) * 128u + x] |= (uint8_t)(1u << (y % 8u));
}
void lcr_canvas_text(lcr_canvas *canvas, unsigned x, unsigned y, const char *text, unsigned scale) {
    if (!canvas || !text || !scale || scale > 2u) return;
    while (*text && x < 128u) {
        unsigned ch = (unsigned char)*text++;
        if (ch < 32u || ch > 126u) ch = '?';
        for (unsigned col = 0u; col < 6u; ++col)
            for (unsigned row = 0u; row < 10u; ++row)
                if (lcr_font[ch - 32u][col] & (1u << row))
                    for (unsigned a = 0u; a < scale; ++a)
                        for (unsigned b = 0u; b < scale; ++b)
                            pixel(canvas, x + col * scale + a, y + row * scale + b);
        x += 6u * scale;
    }
}
static uint8_t reverse(uint8_t value) {
    value = (uint8_t)(((value & 0x55u) << 1) | ((value >> 1) & 0x55u));
    value = (uint8_t)(((value & 0x33u) << 2) | ((value >> 2) & 0x33u));
    return (uint8_t)((value << 4) | (value >> 4));
}
void lcr_canvas_wire(const lcr_canvas *canvas, uint8_t *wire, int rotate) {
    for (unsigned i = 0u; i < LCR_OLED_BYTES; ++i)
        wire[i] = rotate ? reverse(canvas->pixels[LCR_OLED_BYTES - 1u - i]) : canvas->pixels[i];
}
int lcr_ui_value(const lcr_measurement *m, lcr_model model, double *value, const char **unit) {
    if (!m || !m->valid || !value || !unit || (model != LCR_MODEL_SERIES && model != LCR_MODEL_PARALLEL)) return 0;
    const lcr_parameters *p = &m->parameters;
    switch (p->kind) {
    case LCR_COMPONENT_R:
        if (model == LCR_MODEL_PARALLEL && !(p->available & LCR_PARAM_RP)) return 0;
        *value = model == LCR_MODEL_SERIES ? p->rs_ohm : p->rp_ohm; *unit = "OHM"; break;
    case LCR_COMPONENT_L:
        if (!(p->available & (model == LCR_MODEL_SERIES ? LCR_PARAM_LS : LCR_PARAM_LP))) return 0;
        *value = model == LCR_MODEL_SERIES ? p->ls_h : p->lp_h; *unit = "H"; break;
    case LCR_COMPONENT_C:
        if (!(p->available & (model == LCR_MODEL_SERIES ? LCR_PARAM_CS : LCR_PARAM_CP))) return 0;
        *value = model == LCR_MODEL_SERIES ? p->cs_f : p->cp_f; *unit = "F"; break;
    default: return 0;
    }
    return isfinite(*value) && *value >= 0.0;
}
void lcr_ui_number(char *out, size_t size, double value, const char *unit) {
    static const char *const prefixes[] = {"p", "n", "u", "m", "", "k", "M", "G"};
    if (!out || !size) return;
    if (!isfinite(value) || value < 0.0 || !unit) { (void)snprintf(out, size, "----"); return; }
    unsigned index = 4u;
    if (value > 0.0) {
        while (value < 1.0 && index) { value *= 1000.0; --index; }
        while (value >= 1000.0 && index < 7u) { value /= 1000.0; ++index; }
    }
    if (value > 9999.0 || (value > 0.0 && value < 0.001)) { (void)snprintf(out, size, "RANGE"); return; }
    /* Integer formatting avoids printf floating support and preserves bounded output. */
    unsigned decimals = value < 10.0 ? 3u : value < 100.0 ? 2u : 1u;
    unsigned multiplier = decimals == 3u ? 1000u : decimals == 2u ? 100u : 10u;
    unsigned rounded = (unsigned)(value * multiplier + 0.5);
    if (rounded >= 1000u * multiplier && index < 7u) {
        ++index; value /= 1000.0; decimals = 3u; multiplier = 1000u;
        rounded = (unsigned)(value * multiplier + 0.5);
    }
    (void)snprintf(out, size, "%u.%0*u %s%s", rounded / multiplier, (int)decimals,
                   rounded % multiplier, prefixes[index], unit);
}
void lcr_ui_message(lcr_canvas *c, const char *title, const char *line1, const char *line2) {
    lcr_canvas_clear(c);
    lcr_canvas_text(c, 0u, 0u, title, 1u);
    lcr_canvas_text(c, 0u, 21u, line1, 1u);
    lcr_canvas_text(c, 0u, 35u, line2, 1u);
}
void lcr_ui_measure(lcr_canvas *c, const lcr_measurement *m, lcr_model model, const char *status, int details) {
    char text[40];
    lcr_canvas_clear(c);
    const char type=m && m->valid && m->parameters.kind<=LCR_COMPONENT_C?
                    "ZRLC"[m->parameters.kind]:'-';
    (void)snprintf(text, sizeof(text), "%c %s %s", type, model == LCR_MODEL_SERIES ? "SERIES" : "PARALLEL", status?status:"IDLE");
    lcr_canvas_text(c, 0u, 0u, text, 1u);
    double value;
    const char *unit;
    if (!lcr_ui_value(m, model, &value, &unit)) (void)snprintf(text, sizeof(text), "----");
    else lcr_ui_number(text, sizeof(text), value, unit);
    /* Value and unit remain on separate lines so 128px cannot truncate the unit. */
    char *separator = strchr(text, ' ');
    if (separator) *separator++ = '\0';
    lcr_canvas_text(c, 0u, 14u, text, 2u);
    if (separator) lcr_canvas_text(c, 89u, 23u, separator, 1u);
    if (m && m->valid && m->range_index < LCR_RANGE_COUNT) {
        if (details) {
            char number[24],q[16],d[16];
            lcr_ui_number(number,sizeof(number),m->parameters.rs_ohm,"OHM");
            (void)snprintf(text,sizeof(text),"Rs:%s",number);
            lcr_canvas_text(c,0u,37u,text,1u);
            if(m->parameters.available&LCR_PARAM_Q)lcr_ui_number(q,sizeof(q),m->parameters.q,"");
            else (void)snprintf(q,sizeof(q),"--");
            if(m->parameters.available&LCR_PARAM_D)lcr_ui_number(d,sizeof(d),m->parameters.d,"");
            else (void)snprintf(d,sizeof(d),"--");
            (void)snprintf(text,sizeof(text),"Q:%s D:%s",q,d);
        } else {
            (void)snprintf(text, sizeof(text), "%lu HZ  RF:%lu", (unsigned long)(m->actual_hz + 0.5),
                           (unsigned long)lcr_nominal_rf[m->range_index]);
            lcr_canvas_text(c, 0u, 37u, text, 1u);
            if(!m->calibration_flags) (void)snprintf(text,sizeof(text),"UNCAL%s",m->parameters.negative_resistance?" NEG-R":"");
            else (void)snprintf(text,sizeof(text),"CAL:%s%s%s%s",
                m->calibration_flags&LCR_CAL_CHANNEL?"CH ":"",
                m->calibration_flags&LCR_CAL_FEEDBACK?"RF ":"",
                m->calibration_flags&LCR_CAL_FIXTURE?"OPEN":"",
                m->parameters.negative_resistance?" NEG-R":"");
        }
        lcr_canvas_text(c, 0u, 50u, text, 1u);
    } else lcr_canvas_text(c, 0u, 47u, "PB2:START HOLD:MENU", 1u);
}
