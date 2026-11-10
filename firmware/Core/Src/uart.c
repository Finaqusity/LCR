#include "uart.h"
#include "board.h"
#include "stm32f4xx.h"

#include <stdarg.h>
#include <stdio.h>

/* 接收环形缓冲：中断里收，主循环里取 */
#define RX_BUF_SIZE 256U
static volatile uint8_t  s_rx_buf[RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0U;
static volatile uint16_t s_rx_tail = 0U;

static uint32_t uart_pclk(void)
{
    /* USART1 挂在 APB2 上：PCLK2 = HCLK / 2^PPRE2 */
    const uint32_t ppre2 = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    uint32_t div = 1U;
    if ((ppre2 & 0x4U) != 0U) {
        div = 1U << (((ppre2 & 0x3U) + 1U) * 1U);
    }
    return SystemCoreClock / div;
}

void uart_init(uint32_t baud)
{
    BOARD_UART_GPIO_CLK();
    BOARD_UART_CLK();

    /* TX / RX 复用推挽 + 上拉（RX 空闲高电平） */
    const uint32_t tx = BOARD_UART_TX_PIN;
    const uint32_t rx = BOARD_UART_RX_PIN;
    const uint32_t afr_tx = (BOARD_UART_TX_PIN < 8U) ? 0U : 1U;
    const uint32_t afr_rx = (BOARD_UART_RX_PIN < 8U) ? 0U : 1U;

    BOARD_UART_TX_PORT->MODER &= ~(3UL << (tx * 2U));
    BOARD_UART_TX_PORT->MODER |=  (2UL << (tx * 2U));
    BOARD_UART_TX_PORT->AFR[afr_tx] &= ~(0xFUL << ((tx & 7U) * 4U));
    BOARD_UART_TX_PORT->AFR[afr_tx] |=  (BOARD_UART_AF << ((tx & 7U) * 4U));
    BOARD_UART_TX_PORT->OSPEEDR |= (3UL << (tx * 2U));
    BOARD_UART_TX_PORT->PUPDR &= ~(3UL << (tx * 2U));
    BOARD_UART_TX_PORT->PUPDR |=  (1UL << (tx * 2U));

    BOARD_UART_RX_PORT->MODER &= ~(3UL << (rx * 2U));
    BOARD_UART_RX_PORT->MODER |=  (2UL << (rx * 2U));
    BOARD_UART_RX_PORT->AFR[afr_rx] &= ~(0xFUL << ((rx & 7U) * 4U));
    BOARD_UART_RX_PORT->AFR[afr_rx] |=  (BOARD_UART_AF << ((rx & 7U) * 4U));
    BOARD_UART_RX_PORT->PUPDR &= ~(3UL << (rx * 2U));
    BOARD_UART_RX_PORT->PUPDR |=  (1UL << (rx * 2U));

    /* 波特率：BRR = PCLK / baud 的定点表示，四舍五入 */
    const uint32_t pclk = uart_pclk();
    BOARD_UART->BRR = (pclk + (baud / 2U)) / baud;

    BOARD_UART->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;
    NVIC_EnableIRQ(BOARD_UART_IRQn);
    NVIC_SetPriority(BOARD_UART_IRQn, 2U);
}

void uart_write(const char *s)
{
    while (*s != '\0') {
        while ((BOARD_UART->SR & USART_SR_TXE) == 0U) {
            /* 等发送寄存器空 */
        }
        BOARD_UART->DR = (uint8_t)(*s++);
    }
}

int uart_printf(const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    uart_write(buf);
    return n;
}

void USART1_IRQHandler(void)
{
    if ((BOARD_UART->SR & USART_SR_RXNE) != 0U) {
        const uint8_t b = (uint8_t)(BOARD_UART->DR & 0xFFU);   /* 读 DR 同时清标志 */
        const uint16_t next = (uint16_t)((s_rx_head + 1U) % RX_BUF_SIZE);
        if (next != s_rx_tail) {
            s_rx_buf[s_rx_head] = b;
            s_rx_head = next;
        }
    }
    if ((BOARD_UART->SR & USART_SR_ORE) != 0U) {
        (void)BOARD_UART->DR;   /* 清溢出错误 */
    }
}

int uart_available(void)
{
    return (s_rx_head != s_rx_tail);
}

int uart_read_byte(void)
{
    if (s_rx_head == s_rx_tail) {
        return -1;
    }
    const uint8_t b = s_rx_buf[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1U) % RX_BUF_SIZE);
    return (int)b;
}
