#ifndef LCR_BOARD_H
#define LCR_BOARD_H

/* L0 板级：时钟树、SysTick、DWT、LED、延时、看门狗、复位原因、栈高水位。
 * 依据 docs/软件架构.md §4.1、§8.1、§8.5。 */

#include <stdint.h>
#include "stm32f4xx.h"
#include "board_config.h"

/* HSE 起振失败时回退 HSI 16 MHz 并置 1；此时必须拒绝测量（§4.1）。 */
extern volatile uint32_t board_clock_degraded;

/* 复位后 GPIO 是 Hi-Z，ULN2803 无外部下拉 → 最早期把 PD0–PD6 配成输出低，
 * 让常闭的 10 kΩ 档独自在位（§4.7、D16、R3）。main 的第一行就调用。 */
void board_relay_pins_low(void);

void     board_init(void);
uint32_t board_millis(void);
void     board_delay_ms(uint32_t ms);
void     board_delay_us(uint32_t us);
void     board_led_set(int on);
void     board_led_toggle(void);
uint32_t board_cycles(void);          /* DWT CYCCNT，用于 µs 级计时 */
uint32_t board_reset_cause(void);     /* RCC_CSR 复位标志快照 */
void     board_iwdg_start(uint32_t timeout_ms);
void     board_iwdg_feed(void);

/* 栈高水位：返回剩余未使用的栈字节数（填充 0xDEADBEEF 法，§8.1）。 */
uint32_t board_stack_free(void);

/* 端口索引 → GPIO_TypeDef*（board_config.h 不依赖 CMSIS，映射放这里）。 */
GPIO_TypeDef *board_gpio(uint32_t port_idx);

/* 时钟树自检字符串用的实际频率（回退 HSI 时与名义值不同）。 */
uint32_t board_sysclk_hz(void);
uint32_t board_pclk1_hz(void);
uint32_t board_pclk2_hz(void);
uint32_t board_adcclk_hz(void);
uint32_t board_timclk1_hz(void);

#endif /* LCR_BOARD_H */