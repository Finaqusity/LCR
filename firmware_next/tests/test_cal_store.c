#include "lcr_cal_store.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint8_t bytes[LCR_CAL_BLOB_SIZE], saved[LCR_CAL_BLOB_SIZE];
static lcr_cal_profile original, decoded, sentinel;

static void checksum(void) {
    const uint32_t crc = lcr_crc32(bytes, sizeof(bytes) - 4u);
    for (unsigned i = 0u; i < 4u; ++i) bytes[sizeof(bytes) - 4u + i] = (uint8_t)(crc >> (8u * i));
}

int main(void) {
    const uint8_t reference[] = "123456789";
    assert(lcr_crc32(reference, 9u) == 0xcbf43926u);
    assert(lcr_crc32(0, 0u) == 0u);
    lcr_cal_profile_nominal(&original, 123u, 456u, 8000160.0);
    original.sequence = UINT32_MAX;
    for (unsigned f = 0u; f < 4u; ++f) {
        for (unsigned r = 0u; r < 6u; ++r) {
            lcr_calibration *cal = &original.slots[f][r];
            cal->flags = LCR_CAL_ALL;
            cal->revision = 10u * f + r;
            cal->alpha.re = 1.02; cal->alpha.im = -0.03;
            cal->rf_ohm *= 1.0001; cal->cf_f = (r + 1u) * 0.1e-12;
            cal->zs.re = 0.12; cal->zs.im = -0.01;
            cal->yo.re = 1e-9; cal->yo.im = 1e-7 * (f + 1u);
        }
    }
    assert(lcr_cal_encode(&original, bytes, sizeof(bytes)) == LCR_STORE_OK);
    assert(memcmp(bytes, "LCR1", 4u) == 0 && bytes[4] == 1u && bytes[5] == 0u);
    assert(bytes[16] == 123u && bytes[17] == 0u && bytes[20] == 200u && bytes[21] == 1u);
    assert(lcr_cal_decode(bytes, sizeof(bytes), 123u, 456u, &decoded) == LCR_STORE_OK);
    assert(decoded.sequence == UINT32_MAX && decoded.hse_hz == 8000160.0);
    for (unsigned f = 0u; f < 4u; ++f) {
        for (unsigned r = 0u; r < 6u; ++r) {
            const lcr_calibration *a = &original.slots[f][r], *b = &decoded.slots[f][r];
            assert(b->frequency_index == f && b->range_index == r && b->flags == a->flags);
            assert(b->revision == a->revision && b->alpha.re == a->alpha.re && b->alpha.im == a->alpha.im);
            assert(b->rf_ohm == a->rf_ohm && b->cf_f == a->cf_f);
            assert(b->zs.re == a->zs.re && b->zs.im == a->zs.im && b->yo.re == a->yo.re && b->yo.im == a->yo.im);
        }
    }
    memcpy(saved, bytes, sizeof(bytes));
    sentinel = decoded;
    for (size_t i = 0u; i < sizeof(bytes); ++i) {
        bytes[i] ^= 0x40u;
        assert(lcr_cal_decode(bytes, sizeof(bytes), 123u, 456u, &decoded) == LCR_STORE_CHECKSUM);
        assert(memcmp(&decoded, &sentinel, sizeof(decoded)) == 0);
        bytes[i] ^= 0x40u;
    }
    for (size_t length = 0u; length < sizeof(bytes); ++length)
        assert(lcr_cal_decode(bytes, length, 123u, 456u, &decoded) != LCR_STORE_OK);
    assert(memcmp(&decoded, &sentinel, sizeof(decoded)) == 0);
    assert(lcr_cal_decode(bytes, sizeof(bytes), 124u, 456u, &decoded) == LCR_STORE_CONTEXT);
    assert(lcr_cal_decode(bytes, sizeof(bytes), 123u, 457u, &decoded) == LCR_STORE_CONTEXT);
    assert(lcr_cal_decode(bytes, sizeof(bytes), 0u, 0u, &decoded) == LCR_STORE_CONTEXT);
    /* Even a correctly recomputed CRC cannot make invalid semantics acceptable. */
    bytes[4] = 2u; checksum();
    assert(lcr_cal_decode(bytes, sizeof(bytes), 123u, 456u, &decoded) == LCR_STORE_BAD_FORMAT);
    memcpy(bytes, saved, sizeof(bytes)); bytes[40] = 0xffu; checksum();
    assert(lcr_cal_decode(bytes, sizeof(bytes), 123u, 456u, &decoded) == LCR_STORE_BAD_FORMAT);
    memcpy(bytes, saved, sizeof(bytes));
    /* First alpha real is a little-endian binary64 NaN. */
    memset(bytes + 48u, 0, 8u); bytes[54] = 0xf8u; bytes[55] = 0x7fu; checksum();
    assert(lcr_cal_decode(bytes, sizeof(bytes), 123u, 456u, &decoded) == LCR_STORE_BAD_FORMAT);
    assert(memcmp(&decoded, &sentinel, sizeof(decoded)) == 0);
    original.slots[0][0].range_index = 1u;
    assert(lcr_cal_encode(&original, bytes, sizeof(bytes)) == LCR_STORE_BAD_INPUT);
    printf("lcr_cal_store: 24 slots, %u byte corruptions, truncation and context checks passed\n",
           (unsigned)LCR_CAL_BLOB_SIZE);
    return 0;
}
