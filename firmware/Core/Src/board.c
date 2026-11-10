#include "board.h"
#include "stm32f4xx.h"

/* SysTick 1 ms 计数（HCLK = 16 MHz HSI 时为 16000 个时钟） */
static volatile uint32_t s_ms = 0U;

void SysTick_Handler(void)
{
    s_ms++;
}

uint32_t board_millis(void)
{
    return s_ms;
}

void board_init(void)
{
    /* 1) 时钟：先用复位默认的 HSI 16 MHz（够点灯、够 115200 串口）。
     *    等 CubeMX 生成的时钟配置接进来后，再换成 HSE 8 MHz × PLL = 168 MHz。 */
    SystemCoreClockUpdate();

    /* 2) LED GPIO */
    BOARD_LED_CLK();
    BOARD_LED_PORT->MODER &= ~(3UL << (BOARD_LED_PIN * 2U));
    BOARD_LED_PORT->MODER |=  (1UL << (BOARD_LED_PIN * 2U));
    BOARD_LED_PORT->BSRR = (1UL << BOARD_LED_PIN);   /* 先熄灭（假设高电平点亮则这里置高=亮，见下） */

    /* 3) SysTick：1 ms 中断 */
    SysTick_Config(SystemCoreClock / 1000U);
    NVIC_SetPriority(SysTick_IRQn, 3U);
}

void board_led_toggle(void)
{
    if ((BOARD_LED_PORT->ODR & (1UL << BOARD_LED_PIN)) != 0UL) {
        BOARD_LED_PORT->BSRR = (1UL << (BOARD_LED_PIN + 16U));  /* 复位位 = 输出低 */
    } else {
        BOARD_LED_PORT->BSRR = (1UL << BOARD_LED_PIN);          /* 置位位 = 输出高 */
    }
}

void board_delay_ms(uint32_t ms)
{
    const uint32_t start = s_ms;
    while ((s_ms - start) < ms) {
        __NOP();
    }
}
