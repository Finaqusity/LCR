#include "lcr_board.h"
#include "lcr_config.h"
#include "lcr_core.h"
#include "stm32f4xx.h"

/* Register definitions: ST RM0090; DDS framing/timing: AD9833 Rev. G. */
uint32_t SystemCoreClock = 16000000u;
static volatile uint32_t milliseconds;
static int dds_initialized, dds_muted;
static volatile uint8_t rx_head, rx_tail, rx_error;
static uint8_t rx_bytes[128];
int lcr_board_muted(void) { return dds_initialized && dds_muted; }

static void mode(GPIO_TypeDef *gpio, unsigned pin, uint32_t value) {
    const unsigned shift = 2u * pin;
    gpio->MODER = (gpio->MODER & ~(3u << shift)) | (value << shift);
}

static void alternate(GPIO_TypeDef *gpio, unsigned pin, uint32_t function) {
    const unsigned index = pin / 8u, shift = 4u * (pin % 8u);
    gpio->AFR[index] = (gpio->AFR[index] & ~(15u << shift)) | (function << shift);
    mode(gpio, pin, 2u);
}

/* Single on-board key (WK_UP on this board): GPIO input with internal
 * pull-down for an active-high key, or pull-up for an active-low key. */
static GPIO_TypeDef *key_gpio(void) {
    switch (LCR_KEY_PORT) {
    case 0u: return GPIOA;
    case 1u: return GPIOB;
    case 2u: return GPIOC;
    default: return GPIOD;
    }
}

static void key_init(void) {
    GPIO_TypeDef *gpio = key_gpio();
    const unsigned shift = 2u * LCR_KEY_PIN;
    gpio->MODER &= ~(3u << shift);
    gpio->OTYPER &= ~(1u << LCR_KEY_PIN);
    gpio->PUPDR = (gpio->PUPDR & ~(3u << shift)) |
                  ((LCR_KEY_ACTIVE_HIGH ? 2u : 1u) << shift);
}

static void early_gpio(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                    RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN;
    (void)RCC->AHB1ENR;
    /* Load output latches BEFORE changing direction: every range relay (PD0-PD7)
       off, high level = energized. */
    GPIOD->BSRR = 0xffu << 16;
    GPIOD->OTYPER &= ~0xffu;
    GPIOD->PUPDR &= ~0xffffu;
    GPIOD->OSPEEDR &= ~0xffffu;
    GPIOD->MODER = (GPIOD->MODER & ~0xffffu) | 0x5555u;
    GPIOC->BSRR = 1u << 13; /* LED off. */
    mode(GPIOC, 13u, 1u);
#if LCR_EXCITATION_DAC
    /* PA4 is the DAC output (AD9833 removed, PA4 wired to the C1 chip-side
       node).  Keep it analog from the first instruction; PA5/PA7 stay as
       reset-default inputs because the SPI link is gone. */
    mode(GPIOA, 4u, 3u);
#else
    /* PA4 is the AD9833 FSYNC line: idle high, push-pull output. */
    GPIOA->BSRR = 1u << 4;
    mode(GPIOA, 4u, 1u);
#endif
    key_init(); /* WK_UP key first; PA0 must stay an input, V1 sampled on PC0. */
    __DSB();
}

void SystemInit(void) {
    /* No initialized globals, floating point, or libc may be used here. */
    early_gpio();
    SCB->CPACR |= 0xfu << 20;
    SCB->VTOR = FLASH_BASE;
    __DSB();
    __ISB();
}

static int wait_mask(volatile uint32_t *reg, uint32_t mask, uint32_t expected) {
    for (uint32_t remaining = 2000000u; remaining; --remaining)
        if ((*reg & mask) == expected) return 1;
    return 0;
}

static int clock_init(void) {
    RCC->CR |= RCC_CR_HSION;
    if (!wait_mask(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY)) return 0;
    RCC->CFGR &= ~RCC_CFGR_SW;
    if (!wait_mask(&RCC->CFGR, RCC_CFGR_SWS, 0u)) return 0;
    RCC->CR &= ~RCC_CR_PLLON;
    if (!wait_mask(&RCC->CR, RCC_CR_PLLRDY, 0u)) return 0;
    RCC->CR |= RCC_CR_HSEON;
    if (!wait_mask(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY)) return 0;
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_VOS;
    FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_5WS;
    if (!wait_mask(&FLASH->ACR, FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_5WS)) return 0;
    RCC->PLLCFGR = 8u | (336u << 6) | RCC_PLLCFGR_PLLSRC_HSE | (7u << 24);
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 |
                               RCC_CFGR_MCO1 | RCC_CFGR_MCO1PRE)) |
                RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2 | RCC_CFGR_MCO1_1;
    RCC->CR |= RCC_CR_PLLON;
    if (!wait_mask(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY)) return 0;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    if (!wait_mask(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL)) return 0;
    SystemCoreClock = 168000000u;
    GPIOA->OSPEEDR = (GPIOA->OSPEEDR & ~(3u << 16)) | (3u << 16);
    alternate(GPIOA, 8u, 0u); /* MCO1 = HSE, /1 = DDS 8 MHz. */
    return 1;
}

void SysTick_Handler(void) {
    ++milliseconds;
}

uint32_t lcr_board_millis(void) {
    return milliseconds;
}

static void delay_ms(uint32_t delay) {
    const uint32_t start = milliseconds;
    while (milliseconds - start < delay) __WFI();
}

static void uart_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;
    alternate(GPIOA, 9u, 7u);
    alternate(GPIOA, 10u, 7u);
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(3u << 20)) | (1u << 20);
    USART1->CR1 = 0u;
    USART1->CR2 = 0u;
    USART1->CR3 = 0u;
    USART1->BRR = 729u; /* round(84MHz/115200), oversampling by 16. */
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    USART1->CR1 |= USART_CR1_RXNEIE;
    NVIC_SetPriority(USART1_IRQn,2u);
    NVIC_EnableIRQ(USART1_IRQn);
}

int lcr_board_puts(const char *text) {
    if (!text) return 0;
    while (*text) {
        if (!wait_mask(&USART1->SR, USART_SR_TXE, USART_SR_TXE)) return 0;
        USART1->DR = (uint8_t)*text++;
    }
    return 1;
}

int lcr_board_getchar(void) {
    if (rx_error) {
        const uint32_t saved=__get_PRIMASK();__disable_irq();
        rx_error=0u;rx_tail=rx_head;__set_PRIMASK(saved);return -2;
    }
    if (rx_tail==rx_head)return -1;
    const uint8_t value=rx_bytes[rx_tail];rx_tail=(uint8_t)((rx_tail+1u)&127u);return value;
}
void USART1_IRQHandler(void) {
    const uint32_t status = USART1->SR;
    if (status & (USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)) {
        (void)USART1->DR;rx_error=1u;return;
    }
    if (status & USART_SR_RXNE) {
        const uint8_t value=(uint8_t)USART1->DR, next=(uint8_t)((rx_head+1u)&127u);
        if (next==rx_tail)rx_error=1u;
        else { rx_bytes[rx_head]=value;__DMB();rx_head=next; }
    }
}

#if LCR_EXCITATION_DAC
/* Excitation: 12-bit DAC1 on PA4, TIM6 update trigger, DMA1 Stream5 channel 7
 * playing a 56-entry sine table in circular mode.  This replaces the AD9833
 * that was removed from the board; PA4 was re-wired to the old AD9833 VOUT
 * node (chip side of C1).
 *
 * Update rate = 56 * f0, so the TIM6 divider is 84e6 / (56 * f0) = 1500000/f0.
 * That is exact for 100 Hz / 1 kHz / 10 kHz, which keeps the DAC tone and the
 * ADC sample clock (fs = 16 * f0) exactly coherent.  100 kHz needs 5.6 MSPS,
 * beyond a comfortable DAC update rate; it is still configured (divider 15)
 * but expected to be weak/distorted until a faster excitation path exists. */
#define DAC_LUT_LEN 56u
#define DAC_LUT_MID 2048u

static const uint16_t dac_sine[DAC_LUT_LEN] = {
    2048u, 2090u, 2131u, 2171u, 2209u, 2246u, 2280u,
    2311u, 2339u, 2363u, 2383u, 2399u, 2411u, 2418u,
    2420u, 2418u, 2411u, 2399u, 2383u, 2363u, 2339u,
    2311u, 2280u, 2246u, 2209u, 2171u, 2131u, 2090u,
    2048u, 2006u, 1965u, 1925u, 1887u, 1850u, 1816u,
    1785u, 1757u, 1733u, 1713u, 1697u, 1685u, 1678u,
    1676u, 1678u, 1685u, 1697u, 1713u, 1733u, 1757u,
    1785u, 1816u, 1850u, 1887u, 1925u, 1965u, 2006u,
};

static void dma1_stream5_reset(void) {
    DMA1_Stream5->CR = 0u;
    while (DMA1_Stream5->CR & DMA_SxCR_EN) { }
    /* Clear stream 5 flags (FEIF/DMEIF/TEIF/HTIF/TCIF at bits 6..10). */
    DMA1->HIFCR = 0x7c0u;
}

static void dac_mute_output(int muted) {
    if (muted) {
        TIM6->CR1 &= ~TIM_CR1_CEN;
        dma1_stream5_reset();
        DAC->DHR12R1 = DAC_LUT_MID; /* Constant mid-scale on the DAC pin. */
    } else {
        dma1_stream5_reset();
        DMA1_Stream5->PAR = (uint32_t)&DAC->DHR12R1;
        DMA1_Stream5->M0AR = (uint32_t)dac_sine;
        DMA1_Stream5->NDTR = DAC_LUT_LEN;
        DMA1_Stream5->CR = (7u << 25) /* Channel 7 = DAC1. */ |
                           DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0 |
                           DMA_SxCR_MINC | DMA_SxCR_CIRC | DMA_SxCR_DIR_0;
        DMA1_Stream5->CR |= DMA_SxCR_EN;
        TIM6->CNT = 0u;
        TIM6->CR1 |= TIM_CR1_CEN;
    }
    dds_muted = muted;
}

static int dac_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_DACEN | RCC_APB1ENR_TIM6EN;
    (void)RCC->AHB1ENR;
    (void)RCC->APB1ENR;
    mode(GPIOA, 4u, 3u);        /* PA4: analog, DAC_OUT1. */
    GPIOA->PUPDR &= ~(3u << 8); /* No pull-up/pull-down on PA4. */
    TIM6->CR1 = 0u;
    TIM6->CR2 = TIM_CR2_MMS_1; /* Update event drives DAC TRGO. */
    TIM6->PSC = 0u;
    TIM6->ARR = 1499u;         /* 84MHz / (56 * 1kHz) - 1 = 1499. */
    TIM6->EGR = TIM_EGR_UG;
    TIM6->SR = 0u;
    /* TSEL1 = 000 selects TIM6 TRGO (RM0090 DAC trigger table). */
    DAC->CR = DAC_CR_TEN1 | DAC_CR_DMAEN1;
    DAC->DHR12R1 = DAC_LUT_MID;
    DAC->CR |= DAC_CR_EN1;
    dma1_stream5_reset();
    return 1;
}

int lcr_board_dds_frequency(uint32_t hz) {
    if (!dds_initialized || !dds_muted || !hz) return 0;
    /* 84e6 / (56 * hz) = 1500000 / hz must divide exactly. */
    if ((1500000u % hz) != 0u) return 0;
    const uint32_t divisor = 1500000u / hz;
    if (divisor < 1u || divisor > 0x10000u) return 0;
    TIM6->ARR = divisor - 1u;
    TIM6->PSC = 0u;
    TIM6->EGR = TIM_EGR_UG;
    while (TIM6->SR & TIM_SR_UIF) TIM6->SR = ~TIM_SR_UIF;
    return 1;
}

static int mute_callback(void *context, int muted) {
    (void)context;
    if (!dds_initialized) return 0;
    dac_mute_output(muted);
    return 1;
}


#else /* LCR_EXCITATION_DAC == 0: external AD9833 over SPI1. */

static void spi_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;
    SPI1->CR1 = 0u;
    SPI1->CR2 = 0u;
    /* Mode 2, MSB, 16-bit, /16 = 5.25MHz, one-line transmit avoids RX overrun. */
    SPI1->CR1 = SPI_CR1_CPOL | SPI_CR1_MSTR | (3u << 3) | SPI_CR1_SSM |
                SPI_CR1_SSI | SPI_CR1_DFF | SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE;
    GPIOA->OSPEEDR = (GPIOA->OSPEEDR & ~((3u << 10) | (3u << 14))) |
                       (2u << 10) | (2u << 14);
    alternate(GPIOA, 5u, 5u);
    alternate(GPIOA, 7u, 5u);
    SPI1->CR1 |= SPI_CR1_SPE;
}

static void cs_delay(void) {
    /* Minimum 32 core cycles (>190ns at 168MHz), above DDS setup/hold minima. */
    for (unsigned i = 0u; i < 32u; ++i) __NOP();
}

static int dds_write(uint16_t word) {
    if (!wait_mask(&SPI1->SR, SPI_SR_TXE, SPI_SR_TXE)) {
        dds_initialized = 0;
        return 0;
    }
    GPIOA->BSRR = 1u << (4u + 16u);
    cs_delay();
    SPI1->DR = word;
    const int ok = wait_mask(&SPI1->SR, SPI_SR_TXE, SPI_SR_TXE) &&
                   wait_mask(&SPI1->SR, SPI_SR_BSY, 0u);
    cs_delay();
    GPIOA->BSRR = 1u << 4;
    cs_delay();
    if (!ok) dds_initialized = 0;
    return ok;
}

int lcr_board_dds_frequency(uint32_t hz) {
    const uint32_t word = lcr_dds_word(hz);
    if (!dds_initialized || !dds_muted || !word) return 0;
    /* Both halves address FREQ0. RESET remains set until the sequencer unmutes. */
    if (!dds_write(0x2100u) ||
        !dds_write((uint16_t)(0x4000u | (word & 0x3fffu))) ||
        !dds_write((uint16_t)(0x4000u | ((word >> 14) & 0x3fffu)))) {
        dds_initialized = 0;
        return 0;
    }
    return 1;
}

static int mute_callback(void *context, int muted) {
    (void)context;
    if (!dds_initialized || !dds_write((uint16_t)(muted ? 0x2100u : 0x2000u))) return 0;
    dds_muted = muted;
    return 1;
}
#endif /* LCR_EXCITATION_DAC */

static void relay_callback(void *context, uint8_t mask) {
    (void)context;
    const uint32_t outputs = mask & 0xffu;
    GPIOD->BSRR = outputs | ((0xffu & ~outputs) << 16);
    __DSB();
}

lcr_frontend_io lcr_board_frontend_io(void) {
    const lcr_frontend_io io = {0, mute_callback, relay_callback};
    return io;
}

int lcr_board_init(void) {
    early_gpio();
    dds_initialized = 0;
    if (!clock_init()) return 0;
    if (SysTick_Config(SystemCoreClock / 1000u)) return 0;
    uart_init();
#if LCR_EXCITATION_DAC
    if (!dac_init()) return 0;
#else
    spi_init();
#endif
    delay_ms(20u);
#if !LCR_EXCITATION_DAC
    if (!dds_write(0x2100u)) return 0; /* Hold RESET until unmuted. */
#endif
    dds_initialized = 1;
    dds_muted = 1;
    return lcr_board_dds_frequency(1000u);
}

void lcr_board_led(int on) {
    GPIOC->BSRR = 1u << (on ? (13u + 16u) : 13u);
}

unsigned lcr_board_buttons(void) {
    const uint32_t level = (key_gpio()->IDR >> LCR_KEY_PIN) & 1u;
    if (LCR_KEY_ACTIVE_HIGH) return level ? 1u : 0u;
    return level ? 0u : 1u;
}
