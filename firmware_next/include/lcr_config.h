#ifndef LCR_CONFIG_H
#define LCR_CONFIG_H

/* Physical module: SSD1306 128x64 I2C. 0 = upright mounting (no rotation). */
#define LCR_OLED_ROTATE_180 0

/* Single on-board key. This board wires the key to WK_UP (PA0, active high:
 * key closes PA0 to 3V3, internal pull-down). Change PORT/PIN/ACTIVE below if
 * the key is moved to another pin. PORT: 0=PA, 1=PB, 2=PC, 3=PD. */
#define LCR_KEY_PORT 0u
#define LCR_KEY_PIN 0u
#define LCR_KEY_ACTIVE_HIGH 1u

/* Key timing: debounce, long press and double-click window. */
#define LCR_KEY_DEBOUNCE_MS 25u
#define LCR_KEY_LONG_MS 800u
#define LCR_KEY_DOUBLE_MS 300u

#define LCR_FIXTURE_ID 1u
#define LCR_DEFAULT_HSE_HZ 8000000.0

/* Provisional observable-signal stability gates; final limits need bench validation. */
#define LCR_ADAPTIVE_SETTLE 1u
#define LCR_MIN_SETTLE_MS 20u
#define LCR_STABILITY_INTERVAL_MS 20u
#define LCR_STABILITY_TIMEOUT_MS 1200u
#define LCR_STABILITY_DC_CODES 0.2
#define LCR_STABILITY_RELATIVE 0.001
#endif
