#ifndef LCR_UI_H
#define LCR_UI_H
#include "lcr_measure.h"

#define LCR_OLED_BYTES 1024u
typedef enum { LCR_MODEL_SERIES, LCR_MODEL_PARALLEL } lcr_model;
typedef struct { uint8_t pixels[LCR_OLED_BYTES]; } lcr_canvas;
typedef struct {
    uint8_t candidate, stable, blocked, long_sent;
    uint32_t changed, pressed;
} lcr_key;
typedef struct { lcr_key keys[2]; } lcr_keys;
enum { LCR_KEY1_SHORT = 1u, LCR_KEY2_SHORT = 2u, LCR_KEY1_LONG = 4u, LCR_KEY2_LONG = 8u };

void lcr_keys_init(lcr_keys *keys, unsigned raw, uint32_t now);
unsigned lcr_keys_poll(lcr_keys *keys, unsigned raw, uint32_t now);
void lcr_canvas_clear(lcr_canvas *canvas);
void lcr_canvas_text(lcr_canvas *canvas, unsigned x, unsigned y, const char *text, unsigned scale);
/* Convert logical framebuffer to OLED wire order. Default installation uses rotate=1. */
void lcr_canvas_wire(const lcr_canvas *canvas, uint8_t *wire, int rotate);
int lcr_ui_value(const lcr_measurement *measurement, lcr_model model, double *value, const char **unit);
void lcr_ui_number(char *out, size_t size, double value, const char *unit);
void lcr_ui_measure(lcr_canvas *canvas, const lcr_measurement *measurement,
                    lcr_model model, const char *status, int details);
void lcr_ui_message(lcr_canvas *canvas, const char *title, const char *line1, const char *line2);
#endif
