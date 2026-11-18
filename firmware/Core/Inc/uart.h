#ifndef LCR_UART_H
#define LCR_UART_H

/* USART1 PA9/PA10 @115200 8N1（§4.5）。
 * RX：256 B 环形缓冲 + 溢出计数（测量中主机发太快只缓冲，不阻塞采集）。
 * TX：512 B 环形缓冲 + TXE 中断，非阻塞；只有启动横幅用阻塞发送。 */

#include <stdint.h>

void     uart_init(uint32_t baud);

/* 非阻塞：缓冲满则丢弃并计数，绝不忙等（测量期间不许阻塞实时性）。 */
void     uart_write(const char *s);
void     uart_write_bytes(const uint8_t *data, uint32_t len);
int      uart_printf(const char *fmt, ...);

/* 阻塞：仅启动横幅/致命错误用。会先清空 TX 环。 */
void     uart_write_blocking(const char *s);

int      uart_available(void);
int      uart_read_byte(void);        /* 无数据返回 −1 */
void     uart_rx_flush(void);
uint32_t uart_rx_overflow(void);      /* RX 环溢出字节数 → DIAG_RX_OVERFLOW */
uint32_t uart_tx_dropped(void);       /* TX 环满丢弃字节数 */
int      uart_tx_idle(void);          /* TX 环空且移位完成 */

#endif /* LCR_UART_H */