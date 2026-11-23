#include "lcr_board.h"
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

static void early_gpio(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                    RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN;
    (void)RCC->AHB1ENR;
    /* Load output latches BEFORE changing direction: NC 10k feedback, CAL off. */
    GPIOD->BSRR = 0x7fu << 16;
    GPIOD->OTYPER &= ~0x7fu;
    GPIOD->PUPDR &= ~0x3fffu;
    GPIOD->OSPEEDR &= ~0x3fffu;
    GPIOD->MODER = (GPIOD->MODER & ~0x3fffu) | 0x1555u;
    GPIOC->BSRR = 1u << 13; /* LED off. */
    mode(GPIOC, 13u, 1u);
    GPIOA->BSRR = (1u << 4) | (1u << 5) | (1u << (7u + 16u));
    mode(GPIOA, 4u, 1u); /* FSYNC high. */
    mode(GPIOA, 5u, 1u); /* SCLK high BEFORE the first FSYNC falling edge. */
    mode(GPIOA, 7u, 1u);
    mode(GPIOB, 0u, 0u);
    mode(GPIOB, 1u, 0u);
    GPIOB->PUPDR = (GPIOB->PUPDR & ~15u) | 5u;
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

static void relay_callback(void *context, uint8_t mask) {
    (void)context;
    const uint32_t outputs = mask & 0x7fu;
    GPIOD->BSRR = outputs | ((0x7fu & ~outputs) << 16);
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
    spi_init();
    delay_ms(20u);
    if (!dds_write(0x2100u)) return 0;
    dds_initialized = 1;
    dds_muted = 1;
    return lcr_board_dds_frequency(1000u);
}

void lcr_board_led(int on) {
    GPIOC->BSRR = 1u << (on ? (13u + 16u) : 13u);
}

unsigned lcr_board_buttons(void) {
    const uint32_t pressed = ~GPIOB->IDR;
    return ((pressed >> 1) & 1u) | ((pressed & 1u) << 1);
}
