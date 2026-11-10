#ifndef LCR_BOARD_H
#define LCR_BOARD_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * 板级定义（STM32F407VET6 核心板）
 * 如果你的板子 LED 引脚不同，只改这里的三行就行；其他代码不依赖具体引脚。
 * ------------------------------------------------------------------------- */
#define BOARD_LED_PORT      GPIOD
#define BOARD_LED_PIN       12U      /* PD12：很多 VET6 核心板上是绿灯 */
#define BOARD_LED_CLK()     (RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN)

/* 调试串口：USART1 = PA9(TX) / PA10(RX)，接 CH340/CP2102 模块 */
#define BOARD_UART          USART1
#define BOARD_UART_IRQn     USART1_IRQn
#define BOARD_UART_CLK()    (RCC->APB2ENR |= RCC_APB2ENR_USART1EN)
#define BOARD_UART_GPIO_CLK() (RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN)
#define BOARD_UART_AF       7U
#define BOARD_UART_TX_PORT  GPIOA
#define BOARD_UART_TX_PIN   9U
#define BOARD_UART_RX_PORT  GPIOA
#define BOARD_UART_RX_PIN   10U

void board_init(void);
void board_led_toggle(void);
void board_delay_ms(uint32_t ms);
uint32_t board_millis(void);

#endif /* LCR_BOARD_H */
