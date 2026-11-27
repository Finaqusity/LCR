#ifndef LCR_MEASURE_H
#define LCR_MEASURE_H

#include "lcr_core.h"

#define LCR_FREQUENCY_COUNT 4u
#define LCR_RANGE_COUNT 6u

enum {
    LCR_CAL_CHANNEL = 1u, LCR_CAL_FEEDBACK = 2u, LCR_CAL_FIXTURE = 4u,
    LCR_CAL_ALL = 7u
};

/* One frequency/range slot. flags identify supplied calibration, not accuracy. */
typedef struct {
    unsigned frequency_index, range_index;
    uint32_t flags, revision;
    lcr_complex alpha;
    double rf_ohm, cf_f;
    lcr_complex zs, yo;
} lcr_calibration;

typedef struct {
    unsigned frequency_index, range_index;
    double hse_hz; /* Measured clock if available; otherwise nominal 8 MHz. */
    const lcr_calibration *calibration; /* NULL selects nominal, uncalibrated. */
} lcr_measure_config;

typedef enum { LCR_COMPONENT_UNKNOWN, LCR_COMPONENT_R,
               LCR_COMPONENT_L, LCR_COMPONENT_C } lcr_component;
enum {
    LCR_PARAM_LS = 1u, LCR_PARAM_CS = 2u, LCR_PARAM_RP = 4u,
    LCR_PARAM_LP = 8u, LCR_PARAM_CP = 16u, LCR_PARAM_Q = 32u,
    LCR_PARAM_D = 64u
};

typedef struct {
    lcr_component kind; /* Single-frequency equivalent; not physical identification. */
    uint32_t available;
    unsigned negative_resistance; /* Preserve measured sign; do not manufacture ESR/Q. */
    double rs_ohm, xs_ohm, magnitude_ohm, phase_deg;
    double ls_h, cs_f, rp_ohm, lp_h, cp_f, q, d;
} lcr_parameters;

typedef struct {
    lcr_status status;
    unsigned valid, frequency_index, range_index;
    uint32_t calibration_flags, calibration_revision;
    double actual_hz;
    lcr_fit v1, v2;
    lcr_complex raw_z, corrected_z;
    lcr_parameters parameters;
} lcr_measurement;

extern const double lcr_nominal_rf[LCR_RANGE_COUNT];
void lcr_calibration_nominal(lcr_calibration *out, unsigned frequency, unsigned range);
lcr_status lcr_measure_config_check(const lcr_measure_config *config);
lcr_status lcr_parameters_from_z(lcr_complex z, double hz, lcr_parameters *out);
/* Exactly one complete frame, [ADC1,ADC2,...]; output invalidated on every call. */
lcr_status lcr_measure_frame(const uint16_t *pairs, size_t pair_count,
                            const lcr_measure_config *config, lcr_measurement *out);

typedef enum {
    LCR_RANGE_ACCEPT, LCR_RANGE_CHANGE, LCR_RANGE_LIMIT, LCR_RANGE_BAD_FRAME
} lcr_range_action;
typedef struct { lcr_range_action action; unsigned next_range; } lcr_range_choice;
/* Pure decision: caller supplies visited ranges and executes changes while muted. */
lcr_range_choice lcr_choose_range(const lcr_measurement *frame, uint8_t visited);

#endif
