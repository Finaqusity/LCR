#ifndef LCR_UART_H
#define LCR_UART_H

#include <stdint.h>

void uart_init(uint32_t baud);
void uart_write(const char *s);
int  uart_printf(const char *fmt, ...);
int  uart_available(void);
int  uart_read_byte(void);       /* 无数据返回 -1 */

#endif /* LCR_UART_H */
