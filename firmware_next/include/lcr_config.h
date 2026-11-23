#ifndef LCR_CONFIG_H
#define LCR_CONFIG_H
/* Physical module: SSD1306 128x64 I2C; rotation applies to every page at flush. */
#define LCR_OLED_ROTATE_180 1
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
