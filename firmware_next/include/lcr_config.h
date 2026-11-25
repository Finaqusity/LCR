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

/* Excitation source. 1 = internal DAC1 (PA4), as on the current board where
 * the AD9833 was removed. 0 = external AD9833 over SPI1 (PA4=FSYNC, PA5=SCLK,
 * PA7=MOSI, MCLK from PA8/MCO1).  Build with -DLCR_EXCITATION_DAC=0 or with
 * tools/rewrite/build_firmware.ps1 -Ad9833 to select that backend. */
#ifndef LCR_EXCITATION_DAC
#define LCR_EXCITATION_DAC 1u
#endif

/* Excitation-presence gate: if the V1 fundamental peak is below this many ADC
 * codes (25 codes is about 20 mV peak at the V1 pin), reject the measurement as
 * "no excitation" instead of showing a result. */
#define LCR_EXCITATION_MIN_PEAK_CODES 25.0

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
