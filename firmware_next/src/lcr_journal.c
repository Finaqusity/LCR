#include "lcr_journal.h"

static uint32_t word32(const uint8_t *p) {
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int record(lcr_journal_io io, unsigned bank, uint32_t board, uint32_t fixture, lcr_cal_profile *out) {
    uint8_t bytes[LCR_CAL_BLOB_SIZE], commit[4];
    if (!io.read(io.context, bank, LCR_CAL_BLOB_SIZE, commit, 4u)) return -1;
    if (word32(commit) != LCR_JOURNAL_COMMIT) return 0;
    if (!io.read(io.context, bank, 0u, bytes, sizeof(bytes))) return -1;
    return lcr_cal_decode(bytes, sizeof(bytes), board, fixture, out) == LCR_STORE_OK;
}
int lcr_journal_load(lcr_journal_io io, uint32_t board, uint32_t fixture, lcr_cal_profile *out, unsigned *bank) {
    if (!io.read || !out || !bank) return 0;
    lcr_cal_profile candidate;
    int found = 0;
    for (unsigned b = 0u; b < 2u; ++b) {
        const int status = record(io, b, board, fixture, &candidate);
        if (status < 0) return -1;
        if (status > 0) {
            const uint32_t difference = found ? candidate.sequence - out->sequence : 1u;
            if (!found || (difference && difference < 0x80000000u)) {
                *out = candidate; *bank = b; found = 1;
            }
        }
    }
    return found;
}
int lcr_journal_save(lcr_journal_io io, lcr_cal_profile *profile) {
    if (!profile || !io.read || !io.erase || !io.word) return 0;
    lcr_cal_profile current, next = *profile;
    unsigned active = 1u;
    const int found = lcr_journal_load(io, profile->board_id, profile->fixture_id, &current, &active);
    if (found < 0) return 0;
    next.sequence = found ? current.sequence + 1u : 1u;
    uint8_t bytes[LCR_CAL_BLOB_SIZE];
    if (lcr_cal_encode(&next, bytes, sizeof(bytes)) != LCR_STORE_OK) return 0;
    const unsigned destination = active ^ 1u;
    if (!io.erase(io.context, destination)) return 0;
    for (size_t offset = 0u; offset < sizeof(bytes); offset += 4u)
        if (!io.word(io.context, destination, offset, word32(bytes + offset))) return 0;
    /* Verify the entire payload before making it eligible for recovery. */
    if (!io.read(io.context, destination, 0u, bytes, sizeof(bytes)) ||
        lcr_cal_decode(bytes, sizeof(bytes), next.board_id, next.fixture_id, &current) != LCR_STORE_OK ||
        current.sequence != next.sequence) return 0;
    if (!io.word(io.context, destination, sizeof(bytes), LCR_JOURNAL_COMMIT)) return 0;
    if (record(io, destination, next.board_id, next.fixture_id, &current) != 1) return 0;
    *profile = current;
    return 1;
}
