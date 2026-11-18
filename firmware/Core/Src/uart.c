/* USART1 驱动：RX 环 + 溢出计数，TX 环 + TXE 中断（非阻塞）。
 * 依据 docs/软件架构.md §4.5、§8.5。
 * 单生产者（主循环）/单消费者（ISR）环形缓冲：推送无锁，只在开 TXEIE 时
 * 关一下中断（线程模式不会被 ISR 打断，故不存在"开了又被关"的竞态）。 */

#include "uart.h"
#include "board.h"
#include "board_config.h"

#include <stdarg.h>
#include <stdio.h>

static volatile uint8_t  uart_rx_buf[CFG_UART_RX_SIZE];
static volatile uint16_t uart_rx_head = 0U;
static volatile uint16_t uart_rx_tail = 0U;
static volatile uint32_t uart_rx_ovf = 0U;

static volatile uint8_t  uart_tx_buf[CFG_UART_TX_SIZE];
static volatile uint16_t uart_tx_head = 0U;
static volatile uint16_t uart_tx_tail = 0U;
static volatile uint32_t uart_tx_drop = 0U;

static uint32_t uart_pclk(void)
{
    return board_pclk2_hz();   /* USART1 挂 APB2 */
}

void uart_init(uint32_t baud)
{
    GPIO_TypeDef *txp = board_gpio(CFG_UART_TX_PORT);
    GPIO_TypeDef *rxp = board_gpio(CFG_UART_RX_PORT);
    const uint32_t tx = CFG_UART_TX_PIN;
    const uint32_t rx = CFG_UART_RX_PIN;

    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;

    /* TX/RX 复用推挽 + 上拉（RX 空闲高） */
    txp->MODER = (txp->MODER & ~(3UL << (tx * 2U))) | (2UL << (tx * 2U));
    txp->AFR[tx >> 3U] = (txp->AFR[tx >> 3U] & ~(0xFUL << ((tx & 7U) * 4U)))
                       | ((uint32_t)CFG_UART_AF << ((tx & 7U) * 4U));
    txp->OSPEEDR |= (3UL << (tx * 2U));
    txp->PUPDR = (txp->PUPDR & ~(3UL << (tx * 2U))) | (1UL << (tx * 2U));

    rxp->MODER = (rxp->MODER & ~(3UL << (rx * 2U))) | (2UL << (rx * 2U));
    rxp->AFR[rx >> 3U] = (rxp->AFR[rx >> 3U] & ~(0xFUL << ((rx & 7U) * 4U)))
                       | ((uint32_t)CFG_UART_AF << ((rx & 7U) * 4U));
    rxp->PUPDR = (rxp->PUPDR & ~(3UL << (rx * 2U))) | (1UL << (rx * 2U));

    /* BRR = PCLK/baud 四舍五入：84 MHz/115200 → 0x2D9（实测 115226，+0.02%） */
    USART1->BRR = (uart_pclk() + (baud / 2U)) / baud;
    USART1->CR2 = 0U;
    USART1->CR3 = 0U;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    NVIC_SetPriority(USART1_IRQn, 2U);
    NVIC_EnableIRQ(USART1_IRQn);
}

/* ---- TX ------------------------------------------------------------------ */
static void uart_tx_kick(void)
{
    const uint32_t prim = __get_PRIMASK();
    __disable_irq();
    USART1->CR1 |= USART_CR1_TXEIE;
    __set_PRIMASK(prim);
}

static void uart_push(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0U; i < len; ++i) {
        const uint16_t next = (uint16_t)((uart_tx_head + 1U) % CFG_UART_TX_SIZE);
        if (next == uart_tx_tail) {
            uart_tx_drop += (len - i);
            return;                      /* 满了就丢，绝不阻塞采集 */
        }
        uart_tx_buf[uart_tx_head] = data[i];
        uart_tx_head = next;
    }
    uart_tx_kick();
}

void uart_write(const char *s)
{
    uint32_t n = 0U;
    while (s[n] != '\0') {
        ++n;
    }
    uart_push((const uint8_t *)s, n);
}

void uart_write_bytes(const uint8_t *data, uint32_t len)
{
    uart_push(data, len);
}

int uart_printf(const char *fmt, ...)
{
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        uart_push((const uint8_t *)buf, (uint32_t)((n < (int)sizeof(buf)) ? n : (int)sizeof(buf) - 1));
    }
    return n;
}

void uart_write_blocking(const char *s)
{
    USART1->CR1 &= ~USART_CR1_TXEIE;
    /* 先把环里已有的字节发完，保证顺序 */
    while (uart_tx_tail != uart_tx_head) {
        while ((USART1->SR & USART_SR_TXE) == 0U) { /* 等 TXE */ }
        USART1->DR = uart_tx_buf[uart_tx_tail];
        uart_tx_tail = (uint16_t)((uart_tx_tail + 1U) % CFG_UART_TX_SIZE);
    }
    while (*s != '\0') {
        while ((USART1->SR & USART_SR_TXE) == 0U) { /* 等 TXE */ }
        USART1->DR = (uint8_t)(*s++);
    }
    while ((USART1->SR & USART_SR_TC) == 0U) { /* 等最后一位移位完成 */ }
}

int uart_tx_idle(void)
{
    return (uart_tx_tail == uart_tx_head) && ((USART1->SR & USART_SR_TC) != 0U);
}

uint32_t uart_tx_dropped(void)
{
    return uart_tx_drop;
}

/* ---- RX ------------------------------------------------------------------ */
int uart_available(void)
{
    return (uart_rx_head != uart_rx_tail);
}

int uart_read_byte(void)
{
    if (uart_rx_head == uart_rx_tail) {
        return -1;
    }
    const uint8_t b = uart_rx_buf[uart_rx_tail];
    uart_rx_tail = (uint16_t)((uart_rx_tail + 1U) % CFG_UART_RX_SIZE);
    return (int)b;
}

void uart_rx_flush(void)
{
    uart_rx_tail = uart_rx_head;
}

uint32_t uart_rx_overflow(void)
{
    return uart_rx_ovf;
}

void USART1_IRQHandler(void)
{
    const uint32_t sr = USART1->SR;

    if ((sr & USART_SR_RXNE) != 0U) {
        const uint8_t b = (uint8_t)(USART1->DR & 0xFFU);   /* 读 DR 清 RXNE */
        const uint16_t next = (uint16_t)((uart_rx_head + 1U) % CFG_UART_RX_SIZE);
        if (next != uart_rx_tail) {
            uart_rx_buf[uart_rx_head] = b;
            uart_rx_head = next;
        } else {
            uart_rx_ovf++;
        }
    }
    if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) != 0U) {
        (void)USART1->DR;      /* 清错误标志 */
        uart_rx_ovf++;
    }
    if (((sr & USART_SR_TXE) != 0U) && ((USART1->CR1 & USART_CR1_TXEIE) != 0U)) {
        if (uart_tx_tail != uart_tx_head) {
            USART1->DR = uart_tx_buf[uart_tx_tail];
            uart_tx_tail = (uint16_t)((uart_tx_tail + 1U) % CFG_UART_TX_SIZE);
        } else {
            USART1->CR1 &= ~USART_CR1_TXEIE;   /* 发完关中断，避免空转 */
        }
    }
}