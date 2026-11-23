#ifndef LCR_JOURNAL_H
#define LCR_JOURNAL_H
#include "lcr_cal_store.h"
#define LCR_JOURNAL_COMMIT 0x4a52434cu
typedef struct {
    void *context;
    int (*read)(void *context, unsigned bank, size_t offset, uint8_t *bytes, size_t count);
    int (*erase)(void *context, unsigned bank);
    int (*word)(void *context, unsigned bank, size_t offset, uint32_t value);
} lcr_journal_io;
/* Load: 1=record found, 0=no valid record, -1=I/O failure (must not erase). */
int lcr_journal_load(lcr_journal_io io, uint32_t board, uint32_t fixture, lcr_cal_profile *out, unsigned *bank);
int lcr_journal_save(lcr_journal_io io, lcr_cal_profile *profile);
#endif
