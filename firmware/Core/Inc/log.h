#ifndef LCR_LOG_H
#define LCR_LOG_H

/* 事件日志环：最近 16 条，1 KB .bss，掉电不保（§8.3）。
 * 只存整数，不存字符串，避免 printf 洪水与浮点格式化开销。
 * 串口 `I` 命令导出，用于台架定位偶发故障。 */

#include <stdint.h>

#define LOG_CAP 16U

typedef enum {
    LOG_BOOT = 0, LOG_MEAS, LOG_CAL, LOG_ERR, LOG_RELAY, LOG_FLASH, LOG_SELFTEST
} log_kind_t;

void     log_init(void);
void     log_event(uint8_t kind, uint32_t a, uint32_t b);
uint16_t log_count(void);
/* 第 i 条（0=最旧）。返回 0=OK，−1=越界。 */
int      log_get(uint16_t i, uint8_t *kind, uint32_t *a, uint32_t *b, uint32_t *ts);
void     log_clear(void);

#endif /* LCR_LOG_H */