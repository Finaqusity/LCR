#include "lcr_cal_store.h"

#include <float.h>
#include <math.h>
#include <string.h>

static void put32(uint8_t *bytes, uint32_t value) {
    for (unsigned i = 0u; i < 4u; ++i) bytes[i] = (uint8_t)(value >> (8u * i));
}
static uint32_t get32(const uint8_t *bytes) {
    uint32_t value = 0u;
    for (unsigned i = 0u; i < 4u; ++i) value |= (uint32_t)bytes[i] << (8u * i);
    return value;
}
static void put_double(uint8_t *bytes, double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    for (unsigned i = 0u; i < 8u; ++i) bytes[i] = (uint8_t)(bits >> (8u * i));
}
static double get_double(const uint8_t *bytes) {
    uint64_t bits = 0u;
    double value;
    for (unsigned i = 0u; i < 8u; ++i) bits |= (uint64_t)bytes[i] << (8u * i);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

uint32_t lcr_crc32(const uint8_t *bytes, size_t size) {
    uint32_t crc = UINT32_MAX;
    if (!bytes && size) return 0u;
    for (size_t i = 0u; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0u; bit < 8u; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

void lcr_cal_profile_nominal(lcr_cal_profile *profile, uint32_t board_id,
                            uint32_t fixture_id, double hse_hz) {
    if (!profile) return;
    memset(profile, 0, sizeof(*profile));
    profile->board_id = board_id;
    profile->fixture_id = fixture_id;
    profile->hse_hz = hse_hz;
    for (unsigned f = 0u; f < LCR_FREQUENCY_COUNT; ++f)
        for (unsigned r = 0u; r < LCR_RANGE_COUNT; ++r)
            lcr_calibration_nominal(&profile->slots[f][r], f, r);
}

static int valid_profile(const lcr_cal_profile *profile) {
    if (!profile || !profile->board_id || !profile->fixture_id ||
        sizeof(double) != 8u || FLT_RADIX != 2 || DBL_MANT_DIG != 53 || DBL_MAX_EXP != 1024) return 0;
    for (unsigned f = 0u; f < LCR_FREQUENCY_COUNT; ++f) {
        for (unsigned r = 0u; r < LCR_RANGE_COUNT; ++r) {
            const lcr_calibration *slot = &profile->slots[f][r];
            const lcr_measure_config config = {f, r, profile->hse_hz, slot};
            if (lcr_measure_config_check(&config) != LCR_OK) return 0;
            /* Inactive fields must also be finite in the storage representation. */
            if (!isfinite(slot->alpha.re) || !isfinite(slot->alpha.im) ||
                !isfinite(slot->rf_ohm) || slot->rf_ohm <= 0.0 ||
                !isfinite(slot->cf_f) || slot->cf_f < 0.0 ||
                !isfinite(slot->zs.re) || !isfinite(slot->zs.im) ||
                !isfinite(slot->yo.re) || !isfinite(slot->yo.im)) return 0;
        }
    }
    return 1;
}

lcr_store_status lcr_cal_encode(const lcr_cal_profile *profile, uint8_t *bytes, size_t capacity) {
    if (!bytes || capacity < LCR_CAL_BLOB_SIZE || !valid_profile(profile)) return LCR_STORE_BAD_INPUT;
    put32(bytes, 0x3152434cu); /* ASCII LCR1 */
    put32(bytes + 4u, 1u);
    put32(bytes + 8u, LCR_CAL_BLOB_SIZE);
    put32(bytes + 12u, profile->sequence);
    put32(bytes + 16u, profile->board_id);
    put32(bytes + 20u, profile->fixture_id);
    put_double(bytes + 24u, profile->hse_hz);
    put32(bytes + 32u, 0u);
    put32(bytes + 36u, LCR_FREQUENCY_COUNT * LCR_RANGE_COUNT);
    size_t offset = 40u;
    for (unsigned f = 0u; f < LCR_FREQUENCY_COUNT; ++f) {
        for (unsigned r = 0u; r < LCR_RANGE_COUNT; ++r) {
            const lcr_calibration *slot = &profile->slots[f][r];
            put32(bytes + offset, slot->flags);
            put32(bytes + offset + 4u, slot->revision);
            const double values[8] = {slot->alpha.re, slot->alpha.im, slot->rf_ohm,
                slot->cf_f, slot->zs.re, slot->zs.im, slot->yo.re, slot->yo.im};
            for (unsigned i = 0u; i < 8u; ++i) put_double(bytes + offset + 8u + i * 8u, values[i]);
            offset += 72u;
        }
    }
    put32(bytes + offset, lcr_crc32(bytes, offset));
    return LCR_STORE_OK;
}

lcr_store_status lcr_cal_decode(const uint8_t *bytes, size_t size,
                                uint32_t board_id, uint32_t fixture_id, lcr_cal_profile *out) {
    if (!bytes || !out || size != LCR_CAL_BLOB_SIZE || sizeof(double) != 8u)
        return LCR_STORE_BAD_INPUT;
    if (get32(bytes + size - 4u) != lcr_crc32(bytes, size - 4u)) return LCR_STORE_CHECKSUM;
    if (get32(bytes) != 0x3152434cu || get32(bytes + 4u) != 1u ||
        get32(bytes + 8u) != size || get32(bytes + 32u) != 0u ||
        get32(bytes + 36u) != LCR_FREQUENCY_COUNT * LCR_RANGE_COUNT) return LCR_STORE_BAD_FORMAT;
    if (!board_id || !fixture_id || get32(bytes + 16u) != board_id ||
        get32(bytes + 20u) != fixture_id) return LCR_STORE_CONTEXT;
    lcr_cal_profile profile;
    lcr_cal_profile_nominal(&profile, board_id, fixture_id, get_double(bytes + 24u));
    profile.sequence = get32(bytes + 12u);
    size_t offset = 40u;
    for (unsigned f = 0u; f < LCR_FREQUENCY_COUNT; ++f) {
        for (unsigned r = 0u; r < LCR_RANGE_COUNT; ++r) {
            lcr_calibration *slot = &profile.slots[f][r];
            slot->flags = get32(bytes + offset);
            slot->revision = get32(bytes + offset + 4u);
            slot->alpha.re = get_double(bytes + offset + 8u);
            slot->alpha.im = get_double(bytes + offset + 16u);
            slot->rf_ohm = get_double(bytes + offset + 24u);
            slot->cf_f = get_double(bytes + offset + 32u);
            slot->zs.re = get_double(bytes + offset + 40u);
            slot->zs.im = get_double(bytes + offset + 48u);
            slot->yo.re = get_double(bytes + offset + 56u);
            slot->yo.im = get_double(bytes + offset + 64u);
            offset += 72u;
        }
    }
    if (!valid_profile(&profile)) return LCR_STORE_BAD_FORMAT;
    *out = profile;
    return LCR_STORE_OK;
}
