/* 事件日志环（§8.3：1 KB .bss，最近 16 条，掉电不保）。 */

#include "log.h"
#include "board.h"

typedef struct {
    uint32_t ts;
    uint32_t a, b;
    uint8_t  kind;
} log_entry_t;

static log_entry_t log_ring[LOG_CAP];
static uint16_t log_head;      /* 下一个写入位置 */
static uint16_t log_n;         /* 有效条数 ≤16 */

void log_init(void)
{
    log_head = 0U;
    log_n = 0U;
    for (uint16_t i = 0U; i < LOG_CAP; ++i) {
        log_ring[i].ts = 0U;
        log_ring[i].a = 0U;
        log_ring[i].b = 0U;
        log_ring[i].kind = 0U;
    }
}

void log_event(uint8_t kind, uint32_t a, uint32_t b)
{
    log_ring[log_head].ts = board_millis();
    log_ring[log_head].a = a;
    log_ring[log_head].b = b;
    log_ring[log_head].kind = kind;
    log_head = (uint16_t)((log_head + 1U) % LOG_CAP);
    if (log_n < LOG_CAP) {
        ++log_n;
    }
}

uint16_t log_count(void) { return log_n; }

int log_get(uint16_t i, uint8_t *kind, uint32_t *a, uint32_t *b, uint32_t *ts)
{
    if (i >= log_n) {
        return -1;
    }
    /* 最旧的一条在 head−n（环回绕） */
    const uint16_t idx = (uint16_t)((log_head + LOG_CAP - log_n + i) % LOG_CAP);
    if (kind != 0) { *kind = log_ring[idx].kind; }
    if (a != 0) { *a = log_ring[idx].a; }
    if (b != 0) { *b = log_ring[idx].b; }
    if (ts != 0) { *ts = log_ring[idx].ts; }
    return 0;
}

void log_clear(void) { log_init(); }