#ifndef LCR_CAL_STORE_H
#define LCR_CAL_STORE_H

#include "lcr_measure.h"

/* Explicit LE format; no structure padding/pointers are written to storage. */
#define LCR_CAL_BLOB_SIZE (40u + LCR_FREQUENCY_COUNT * LCR_RANGE_COUNT * 72u + 4u)
typedef struct {
    uint32_t sequence, board_id, fixture_id;
    double hse_hz;
    lcr_calibration slots[LCR_FREQUENCY_COUNT][LCR_RANGE_COUNT];
} lcr_cal_profile;

typedef enum {
    LCR_STORE_OK, LCR_STORE_BAD_INPUT, LCR_STORE_BAD_FORMAT,
    LCR_STORE_CHECKSUM, LCR_STORE_CONTEXT
} lcr_store_status;

void lcr_cal_profile_nominal(lcr_cal_profile *profile, uint32_t board_id,
                            uint32_t fixture_id, double hse_hz);
uint32_t lcr_crc32(const uint8_t *bytes, size_t size);
lcr_store_status lcr_cal_encode(const lcr_cal_profile *profile, uint8_t *bytes, size_t capacity);
/* Exact-length record only; failed decode leaves output untouched. */
lcr_store_status lcr_cal_decode(const uint8_t *bytes, size_t size,
                                uint32_t board_id, uint32_t fixture_id, lcr_cal_profile *out);

#endif
