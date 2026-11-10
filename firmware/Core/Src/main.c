/*
 * LCR 电桥测量装置 —— 固件 Day1 心跳程序
 *   串口：USART1 PA9(TX)/PA10(RX)，115200 8N1
 *   现象：上电后打印启动信息，然后每 500 ms 打印一行 tick，LED 同步翻转；
 *         从串口发任意字符会被回显，发 'v' 打印版本，发 '?' 打印帮助。
 * 这一步只证明：工具链 → 编译 → 烧录 → 串口 → 时钟 全链路是通的。
 */
#include "board.h"
#include "uart.h"
#include "stm32f4xx.h"

#include <stdio.h>

#define FW_VERSION "v0.1.0"

static void handle_rx(void)
{
    int c;
    while ((c = uart_read_byte()) >= 0) {
        if (c == 'v') {
            uart_printf("# LCR fw=" FW_VERSION " build=" __DATE__ " " __TIME__ " clk=%lu\r\n",
                        (unsigned long)SystemCoreClock);
        } else if (c == '?') {
            uart_write("# help: v=version ?=help -> 回显任意字符\r\n");
        } else {
            uart_printf("# rx: '%c' (%d)\r\n", (c >= 32 && c < 127) ? c : '.', c);
        }
    }
}

int main(void)
{
    board_init();
    uart_init(115200U);

    uart_printf("\r\n# ===============================================\r\n");
    uart_printf("# LCR 电桥测量装置 固件 " FW_VERSION "\r\n");
    uart_printf("# build: " __DATE__ " " __TIME__ "\r\n");
    uart_printf("# MCU  : STM32F407VET6  SYSCLK = %lu Hz (HSI 16 MHz)\r\n",
                (unsigned long)SystemCoreClock);
    uart_printf("# UART : USART1 PA9/PA10 @115200 8N1  (发送 'v' 查看版本)\r\n");
    uart_printf("# ===============================================\r\n");

    uint32_t tick = 0U;
    uint32_t last_led = 0U;

    for (;;) {
        handle_rx();

        const uint32_t now = board_millis();
        if ((now - last_led) >= 500U) {
            last_led = now;
            board_led_toggle();
            tick++;
            uart_printf("$ tick=%lu ms=%lu\r\n", (unsigned long)tick, (unsigned long)now);
        }
    }
}
