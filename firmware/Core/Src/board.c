/* L0 板级支持：HSE 8 MHz → PLL 168 MHz 时钟树、MCO1 供 DDS、SysTick、DWT、
 * PC13 心跳、看门狗、栈高水位。依据 docs/软件架构.md §4.1、§8.1、§8.5。
 * 定稿约束：ADC 21 MHz（PCLK2/4）、APB1 定时器 84 MHz（TIM2 32 位）、
 * HSE 失败回退 HSI 并拒绝测量、LED 是 PC13 低有效（不是 PD12）。 */

#include "board.h"
#include "error.h"

volatile uint32_t board_clock_degraded = 0U;

static volatile uint32_t board_ms = 0U;
static uint32_t board_reset_flags = 0U;
static uint32_t board_iwdg_on = 0U;

extern uint32_t __StackLimit;   /* 链接脚本：栈底 = 0x2001F000 */

/* ---- 端口映射 ------------------------------------------------------------ */
static GPIO_TypeDef *const board_ports[] = { GPIOA, GPIOB, GPIOC, GPIOD, GPIOE };

GPIO_TypeDef *board_gpio(uint32_t port_idx)
{
    return board_ports[port_idx];
}

static void board_gpio_clk_enable(uint32_t port_idx)
{
    RCC->AHB1ENR |= (1UL << port_idx);   /* GPIOAEN=bit0 … GPIOEEN=bit4 */
    (void)RCC->AHB1ENR;                  /* 等待时钟稳定 */
}

static void board_pin_mode(GPIO_TypeDef *port, uint32_t pin, uint32_t mode)
{
    port->MODER = (port->MODER & ~(3UL << (pin * 2U))) | (mode << (pin * 2U));
}

static void board_pin_af(GPIO_TypeDef *port, uint32_t pin, uint32_t af)
{
    port->AFR[pin >> 3U] = (port->AFR[pin >> 3U] & ~(0xFUL << ((pin & 7U) * 4U)))
                         | (af << ((pin & 7U) * 4U));
}

static void board_pin_speed(GPIO_TypeDef *port, uint32_t pin, uint32_t speed)
{
    port->OSPEEDR = (port->OSPEEDR & ~(3UL << (pin * 2U))) | (speed << (pin * 2U));
}

/* ---- 最早期：继电器引脚输出低（D16/R3）---------------------------------- */
void board_relay_pins_low(void)
{
    board_gpio_clk_enable(CFG_RELAY_PORT);
    GPIO_TypeDef *port = board_gpio(CFG_RELAY_PORT);
    for (uint32_t i = 0U; i < CFG_RELAY_PIN_COUNT; ++i) {
        const uint32_t pin = CFG_RELAY_PIN_FIRST + i;
        port->BSRR = (1UL << (pin + 16U));   /* 先拉低再改模式，避免毛刺 */
        board_pin_mode(port, pin, 1U);       /* 通用推挽输出 */
        port->OTYPER &= ~(1UL << pin);
        board_pin_speed(port, pin, 0U);      /* 低速即可，线圈 21 mA */
        port->PUPDR &= ~(3UL << (pin * 2U)); /* 无上下拉：ULN2803 输入 */
    }
}

/* ---- SysTick / DWT ------------------------------------------------------- */
void SysTick_Handler(void)
{
    board_ms++;
}

uint32_t board_millis(void)
{
    return board_ms;
}

uint32_t board_cycles(void)
{
    return DWT->CYCCNT;
}

uint32_t board_sysclk_hz(void) { return SystemCoreClock; }

uint32_t board_pclk1_hz(void)
{
    const uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    return (ppre < 4U) ? SystemCoreClock : (SystemCoreClock >> (ppre - 3U));
}

uint32_t board_pclk2_hz(void)
{
    const uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    return (ppre < 4U) ? SystemCoreClock : (SystemCoreClock >> (ppre - 3U));
}

uint32_t board_timclk1_hz(void)
{
    const uint32_t ppre = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    return (ppre < 4U) ? SystemCoreClock : (SystemCoreClock << 1U);
}

uint32_t board_adcclk_hz(void)
{
    const uint32_t pre = (ADC->CCR & ADC_CCR_ADCPRE) >> ADC_CCR_ADCPRE_Pos;
    return board_pclk2_hz() / (2U * (pre + 1U));   /* /2 /4 /6 /8 */
}

void board_delay_us(uint32_t us)
{
    const uint32_t start = DWT->CYCCNT;
    const uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks) {
        /* 忙等只用于初始化与 µs 级等待（协作说明 §8.4） */
    }
}

void board_delay_ms(uint32_t ms)
{
    const uint32_t start = board_ms;
    while ((board_ms - start) < ms) {
        __NOP();
    }
}

/* ---- LED（PC13，低电平点亮）--------------------------------------------- */
void board_led_set(int on)
{
    const uint32_t pin = CFG_LED_PIN;
    GPIO_TypeDef *port = board_gpio(CFG_LED_PORT);
#if CFG_LED_ACTIVE_LOW
    port->BSRR = on ? (1UL << (pin + 16U)) : (1UL << pin);
#else
    port->BSRR = on ? (1UL << pin) : (1UL << (pin + 16U));
#endif
}

void board_led_toggle(void)
{
    GPIO_TypeDef *port = board_gpio(CFG_LED_PORT);
    if ((port->ODR & (1UL << CFG_LED_PIN)) != 0U) {
        board_led_set(1);
    } else {
        board_led_set(0);
    }
}

/* ---- 复位原因 / 看门狗 --------------------------------------------------- */
uint32_t board_reset_cause(void)
{
    return board_reset_flags;
}

void board_iwdg_start(uint32_t timeout_ms)
{
    if (timeout_ms > 4095U) {
        timeout_ms = 4095U;      /* RLR 12 位上限 */
    }
    IWDG->KR = 0x5555U;          /* 允许写 PR/RLR */
    IWDG->PR = 3U;               /* LSI 32 kHz /32 = 1 kHz → 1 count = 1 ms */
    IWDG->RLR = timeout_ms;
    while ((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0U) {
        /* 等待预分频/重载寄存器同步 */
    }
    IWDG->KR = 0xCCCCU;          /* 启动 */
    IWDG->KR = 0xAAAAU;          /* 喂狗 */
    board_iwdg_on = 1U;
}

void board_iwdg_feed(void)
{
    if (board_iwdg_on != 0U) {
        IWDG->KR = 0xAAAAU;
    }
}

/* ---- 栈高水位（§8.1）---------------------------------------------------- */
static void board_stack_paint(void)
{
    uint32_t sp;
    __asm volatile ("mov %0, sp" : "=r" (sp));
    uint32_t *p = &__StackLimit;
    uint32_t *const guard = (uint32_t *)(sp - 64U);   /* 不覆盖正在用的栈帧 */
    while (p < guard) {
        *p++ = CFG_STACK_MAGIC;
    }
}

uint32_t board_stack_free(void)
{
    const uint32_t *p = (const uint32_t *)&__StackLimit;
    uint32_t free_bytes = 0U;
    while ((free_bytes < CFG_STACK_SIZE) && (*p == CFG_STACK_MAGIC)) {
        ++p;
        free_bytes += 4U;
    }
    return free_bytes;
}

/* ---- 未用引脚置模拟态（§4.1，降噪声/漏电）------------------------------- */
static void board_unused_analog(void)
{
    /* 保留：PA0/1/4/5/7/8/9/10/13/14、PB6/7、PC13、PD0…PD7。
     * PA13/PA14 是 SWD，绝不能动。 */
    static const struct { uint32_t port; uint32_t keep_mask; } keep[] = {
        { CFG_PORT_A, (1UL<<0)|(1UL<<1)|(1UL<<4)|(1UL<<5)|(1UL<<7)|(1UL<<8)
                    | (1UL<<9)|(1UL<<10)|(1UL<<13)|(1UL<<14) },
        { CFG_PORT_B, (1UL<<6)|(1UL<<7) },
        { CFG_PORT_C, (1UL<<13) },
        { CFG_PORT_D, 0xFFUL },
        { CFG_PORT_E, 0x00UL },
    };
    for (uint32_t i = 0U; i < (sizeof(keep) / sizeof(keep[0])); ++i) {
        GPIO_TypeDef *port = board_gpio(keep[i].port);
        for (uint32_t pin = 0U; pin < 16U; ++pin) {
            if ((keep[i].keep_mask & (1UL << pin)) == 0U) {
                board_pin_mode(port, pin, 3U);   /* 模拟态 */
                port->PUPDR &= ~(3UL << (pin * 2U));
            }
        }
    }
}

/* ---- 时钟树 -------------------------------------------------------------- */
static int board_clock_init(void)
{
    /* 1) 调压 Scale 1（168 MHz 必需） */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_VOS;

    /* 2) 起 HSE，100 ms 超时（此时 SysTick 还没跑，用 DWT 计时） */
    RCC->CR |= RCC_CR_HSEON;
    const uint32_t start = DWT->CYCCNT;
    const uint32_t limit = (CFG_HSE_TIMEOUT_MS * (CFG_HSI_HZ / 1000U));
    while (((RCC->CR & RCC_CR_HSERDY) == 0U) && ((DWT->CYCCNT - start) < limit)) {
        /* 等 HSE 就绪 */
    }
    if ((RCC->CR & RCC_CR_HSERDY) == 0U) {
        /* 回退：留在 HSI 16 MHz，不切 PLL。MCO 无 HSE 可输出 → DDS 不工作，
         * 上层必须拒绝测量并报错（§4.1）。 */
        RCC->CR &= ~RCC_CR_HSEON;
        board_clock_degraded = 1U;
        return -1;
    }

    /* 3) Flash 等待周期与预取/缓存必须在切频之前配好 */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY)
               | CFG_FLASH_WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    /* 4) PLL：8 MHz /8 ×336 /2 = 168 MHz，Q=7 → 48 MHz（未用 USB） */
    RCC->PLLCFGR = (CFG_PLL_M << RCC_PLLCFGR_PLLM_Pos)
                 | (CFG_PLL_N << RCC_PLLCFGR_PLLN_Pos)
                 | (((CFG_PLL_P / 2U) - 1U) << RCC_PLLCFGR_PLLP_Pos)
                 | (CFG_PLL_Q << RCC_PLLCFGR_PLLQ_Pos)
                 | RCC_PLLCFGR_PLLSRC_HSE;
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0U) {
        /* 等 PLL 锁定 */
    }

    /* 5) 总线分频：AHB /1、APB1 /4（42 MHz，定时器 84）、APB2 /2（84 MHz） */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2))
              | RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;

    /* 6) 切到 PLL */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
        /* 等切换完成 */
    }

    /* 7) MCO1 = HSE /1 → PA8，给 AD9833 当 MCLK（与采样时钟同源，相干采样）
     *    MCO1SEL[1:0]=0b10 选 HSE，MCO1PRE=0 不分频 */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_MCO1 | RCC_CFGR_MCO1PRE))
              | RCC_CFGR_MCO1_1;
    board_gpio_clk_enable(CFG_MCO_PORT);
    GPIO_TypeDef *mco = board_gpio(CFG_MCO_PORT);
    board_pin_mode(mco, CFG_MCO_PIN, 2U);
    board_pin_af(mco, CFG_MCO_PIN, CFG_MCO_AF);
    board_pin_speed(mco, CFG_MCO_PIN, 3U);
    mco->OTYPER &= ~(1UL << CFG_MCO_PIN);
    mco->PUPDR &= ~(3UL << (CFG_MCO_PIN * 2U));
    return 0;
}

void board_init(void)
{
    /* FPU（CP10/CP11 全访问）与 DWT 先开，后面要用 DWT 计时 */
    SCB->CPACR |= (0xFUL << 20U);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    board_reset_flags = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;      /* 清标志，下次复位重新统计 */

    board_relay_pins_low();        /* 继电器安全态优先于一切 */

    for (uint32_t i = CFG_PORT_A; i <= CFG_PORT_E; ++i) {
        board_gpio_clk_enable(i);
    }

    (void)board_clock_init();
    SystemCoreClockUpdate();

    /* LED：PC13 推挽输出，先熄灭 */
    GPIO_TypeDef *led = board_gpio(CFG_LED_PORT);
    board_pin_mode(led, CFG_LED_PIN, 1U);
    led->OTYPER &= ~(1UL << CFG_LED_PIN);
    board_pin_speed(led, CFG_LED_PIN, 0U);
    led->PUPDR &= ~(3UL << (CFG_LED_PIN * 2U));
    board_led_set(0);

    board_unused_analog();

    SysTick_Config(SystemCoreClock / CFG_SYSTICK_HZ);
    NVIC_SetPriority(SysTick_IRQn, 3U);

    board_stack_paint();
}