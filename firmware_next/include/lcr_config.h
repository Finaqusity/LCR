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

/* Excitation source. 1 = internal DAC1 channel 2 (PA5; PA4 is shorted to GND
 * the AD9833 was removed. 0 = external AD9833 over SPI1 (PA4=FSYNC, PA5=SCLK,
 * PA7=MOSI, MCLK from PA8/MCO1).  Build with -DLCR_EXCITATION_DAC=0 or with
 * tools/rewrite/build_firmware.ps1 -Ad9833 to select that backend. */
#ifndef LCR_EXCITATION_DAC
#define LCR_EXCITATION_DAC 1u
#endif

/* Continuous excitation. 1 = the DAC sine keeps running even when the
 * measurement layer requests "mute"; the logical mute state is still tracked
 * (for frequency changes) but TP1 shows a continuous sine between
 * measurements, which is what a bench instrument is expected to do.
 * Frequency switches pause the timer for well under one cycle, i.e. invisible
 * at 100 Hz..10 kHz.  Set to 0 to silence the excitation while idle. */
#ifndef LCR_EXCITATION_ALWAYS_ON
#define LCR_EXCITATION_ALWAYS_ON 1u
#endif

/* Excitation-presence gate: if the V1 fundamental peak is below this many ADC
 * codes (25 codes is about 20 mV peak at the V1 pin), reject the measurement as
 * "no excitation" instead of showing a result. */
#define LCR_EXCITATION_MIN_PEAK_CODES 25.0

/* Auto-start. 1 = begin a continuous measurement as soon as the firmware
 * boots, so a demo/bench setup shows a reading without touching the key
 * (the key is on the back of this core board). Stop with a short press.
 * Set to 0 to boot into the idle screen. */
#ifndef LCR_AUTO_START
#define LCR_AUTO_START 1u
#endif

/* Highest frequency index the calibration sweep measures. This core board has
 * no calibrated 100 kHz excitation today: the original AD9833 is removed and
 * the internal DAC backend is only coherent up to 10 kHz (100 kHz would need
 * 5.6 MSPS). Leave that band uncalibrated instead of failing the whole sweep;
 * it simply keeps nominal alpha/Rf until the path supports 100 kHz. */
#if LCR_EXCITATION_DAC
#define LCR_CAL_MAX_FREQUENCY_INDEX 2u
#else
#define LCR_CAL_MAX_FREQUENCY_INDEX 3u
#endif

/* DMA block limit of the platform capture layer (dma_words[8192] pairs). */
#define LCR_CAPTURE_PAIRS_MAX 8192u

/* Calibration frames are longer than a normal measurement frame whenever the
 * frequency table only gives a short capture.  The 100 Hz slot captures 128
 * pairs (80 ms); a calibration frame may be long without hurting the 1 s
 * measurement budget. */
#define LCR_CAL_MIN_PAIRS 512u

/* CHANNEL calibration averages many frames because the dominant sample-to-
 * sample scatter is the 0.5..2 Hz V2 gain drift (~0.7 % between frames), not
 * white noise; a wider consistency gate accepts that drift and the mean over
 * many frames averages it down. */
#define LCR_CAL_CHANNEL_SAMPLES 16u
#define LCR_CAL_CHANNEL_CONSISTENCY 0.02

#define LCR_FIXTURE_ID 1u
#define LCR_DEFAULT_HSE_HZ 8000000.0

/* Analogue saturation window.  The I-V output stage (BUF634A inside the
 * OPA2810 loop, 9 V rail, ADC inputs clamped to 3V3A) runs out of swing before
 * the ADC itself reaches 0/4095: a saturated V2 frame measures min 13 / max
 * 4071, so a rail test at 8/4087 never flags it and the auto-range would never
 * step down.  Keep the window just inside the analogue swing. */
#define LCR_ADC_CLIP_LOW 32u
#define LCR_ADC_CLIP_HIGH 4063u

/* Provisional observable-signal stability gates; final limits need bench validation. */
#define LCR_ADAPTIVE_SETTLE 1u
#define LCR_MIN_SETTLE_MS 20u
#define LCR_STABILITY_INTERVAL_MS 20u
#define LCR_STABILITY_TIMEOUT_MS 1200u
#define LCR_STABILITY_DC_CODES 5.0
/* The settling gate only has to reject a range-switch step.  A settling frame
 * is only 256 pairs (16 ms at 1 kHz), so its V1 amplitude estimate itself
 * jitters by ~1 % on a healthy channel and a 1 % gate never closes; 3 % still
 * rejects the 10x amplitude step of a wrong range. */
#define LCR_STABILITY_RELATIVE 0.03
#endif
