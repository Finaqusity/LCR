#include "lcr_board.h"
#include "lcr_ui.h"
#include "stm32f4xx.h"
#include <string.h>
#include "lcr_config.h"

static uint8_t oled_address;
static uint8_t wire_pixels[1024];
static int wait_i2c(volatile uint32_t *reg, uint32_t mask, uint32_t expected) {
    const uint32_t start = lcr_board_millis();
    for (unsigned limit = 0u; limit < 200000u; ++limit) {
        if (I2C1->SR1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR)) return 0;
        if ((*reg & mask) == expected) return 1;
        if (lcr_board_millis() - start >= 5u) break;
    }
    return 0;
}
static int transmit(uint8_t address, uint8_t control, const uint8_t *bytes, size_t size) {
    I2C1->SR1 = 0u;
    if (!wait_i2c(&I2C1->SR2, I2C_SR2_BUSY, 0u)) return 0;
    I2C1->CR1 |= I2C_CR1_START;
    if (!wait_i2c(&I2C1->SR1, I2C_SR1_SB, I2C_SR1_SB)) goto fail;
    I2C1->DR = (uint32_t)address << 1;
    if (!wait_i2c(&I2C1->SR1, I2C_SR1_ADDR, I2C_SR1_ADDR)) goto fail;
    (void)I2C1->SR2;
    if (!wait_i2c(&I2C1->SR1, I2C_SR1_TXE, I2C_SR1_TXE)) goto fail;
    I2C1->DR = control;
    for (size_t i = 0u; i < size; ++i) {
        if (!wait_i2c(&I2C1->SR1, I2C_SR1_TXE, I2C_SR1_TXE)) goto fail;
        I2C1->DR = bytes[i];
    }
    if (!wait_i2c(&I2C1->SR1, I2C_SR1_BTF, I2C_SR1_BTF)) goto fail;
    I2C1->CR1 |= I2C_CR1_STOP;
    return wait_i2c(&I2C1->SR2, I2C_SR2_BUSY, 0u);
fail:
    I2C1->CR1 |= I2C_CR1_STOP;
    return 0;
}
static void bus_delay(void) { for (unsigned i = 0u; i < 1000u; ++i) __NOP(); }
int lcr_board_display_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;
    I2C1->CR1 = 0u;
    GPIOB->BSRR = (1u << 6) | (1u << 7);
    GPIOB->OTYPER |= (1u << 6) | (1u << 7);
    GPIOB->PUPDR &= ~((3u << 12) | (3u << 14));
    GPIOB->MODER = (GPIOB->MODER & ~((3u << 12) | (3u << 14))) | (1u << 12) | (1u << 14);
    /* Bounded bus recovery before enabling AF; module has external pull-ups. */
    for (unsigned i = 0u; i < 9u && !(GPIOB->IDR & (1u << 7)); ++i) {
        GPIOB->BSRR = 1u << (6u + 16u); bus_delay();
        GPIOB->BSRR = 1u << 6; bus_delay();
    }
    GPIOB->BSRR = 1u << (7u + 16u); bus_delay();
    GPIOB->BSRR = 1u << 6; bus_delay();
    GPIOB->BSRR = 1u << 7; bus_delay();
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~((15u << 24) | (15u << 28))) | (4u << 24) | (4u << 28);
    GPIOB->MODER = (GPIOB->MODER & ~((3u << 12) | (3u << 14))) | (2u << 12) | (2u << 14);
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;
    I2C1->CR2 = 42u;
    I2C1->CCR = I2C_CCR_FS | 35u;
    I2C1->TRISE = 13u;
    I2C1->CR1 = I2C_CR1_PE;
    /* SSD1306 charge-pump module, logical base orientation; rotate at flush. */
    static const uint8_t init[] = {0xae,0xd5,0x80,0xa8,0x3f,0xd3,0,0x40,0x8d,0x14,
        0x20,0,0xa1,0xc8,0xda,0x12,0x81,0x7f,0xd9,0xf1,0xdb,0x20,0xa4,0xa6,0x2e,0xaf};
    for (unsigned i = 0u; i < 2u; ++i) {
        const uint8_t address = (uint8_t)(0x3cu + i);
        if (transmit(address, 0u, init, sizeof(init))) { oled_address = address; return 1; }
    }
    oled_address = 0u;
    return 0;
}
int lcr_board_display(const uint8_t *logical_pixels) {
    if (!oled_address || !logical_pixels) return 0;
    lcr_canvas canvas;
    memcpy(canvas.pixels, logical_pixels, sizeof(canvas.pixels));
    lcr_canvas_wire(&canvas, wire_pixels, LCR_OLED_ROTATE_180);
    static const uint8_t position[] = {0x21,0,127,0x22,0,7};
    if (!transmit(oled_address, 0u, position, sizeof(position)) ||
        !transmit(oled_address, 0x40u, wire_pixels, sizeof(wire_pixels))) {
        oled_address = 0u;
        return 0;
    }
    return 1;
}
