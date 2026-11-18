/* AD9833 DDS 驱动（§4.2）。SPI1 16 bit 软 NSS，MCLK = MCO1(PA8) = HSE 8 MHz。
 * 频率字 FREQREG = round(f·2^28/8e6)：100/1k/10k/100k → 3355/33554/335544/3355443，
 * 实际 f_dds = 99.98679/999.98713/9999.99046/99999.99404 Hz。
 * C/L 换算必须用 f_dds（dds_get_freq_mhz），不是标称值。
 * AD9833 只写，控制字与频率字全部由固件缓存。 */

#include "dds.h"
#include "board.h"
#include "board_config.h"

/* 控制字位（D15/D14=00 选中控制寄存器） */
#define AD_B28      (1UL << 13)   /* 频率字分两次 14 位写 */
#define AD_RESET    (1UL << 8)    /* 1=输出中点电平（静音） */
#define AD_OPBITEN  (1UL << 5)    /* 方波输出 */
#define AD_DIV2     (1UL << 3)    /* 方波取 DAC MSB/2 */
#define AD_MODE     (1UL << 1)    /* 1=三角波 */
#define AD_FREQ0_L  0x4000UL
#define AD_FREQ0_H  0x8000UL
#define AD_PHASE0   0xC000UL

#define AD_FREG_MAX 33554432UL    /* 1 MHz @ MCLK 8 MHz */

static uint32_t dds_ctrl = AD_B28 | AD_RESET;
static uint32_t dds_freqreg = 0U;
static int      dds_muted = 1;
static int      dds_ready = 0;

/* ---- SPI1 ---------------------------------------------------------------- */
static void dds_fsync(int low)
{
    GPIO_TypeDef *port = board_gpio(CFG_DDS_FSYNC_PORT);
    port->BSRR = low ? (1UL << (CFG_DDS_FSYNC_PIN + 16U)) : (1UL << CFG_DDS_FSYNC_PIN);
}

static void dds_spi_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;

    GPIO_TypeDef *sck = board_gpio(CFG_SPI_SCK_PORT);
    GPIO_TypeDef *mosi = board_gpio(CFG_SPI_MOSI_PORT);
    const uint32_t pins[2] = { CFG_SPI_SCK_PIN, CFG_SPI_MOSI_PIN };
    GPIO_TypeDef *ports[2] = { sck, mosi };
    for (uint32_t i = 0U; i < 2U; ++i) {
        const uint32_t p = pins[i];
        ports[i]->MODER = (ports[i]->MODER & ~(3UL << (p * 2U))) | (2UL << (p * 2U));
        ports[i]->AFR[p >> 3U] = (ports[i]->AFR[p >> 3U] & ~(0xFUL << ((p & 7U) * 4U)))
                               | ((uint32_t)CFG_SPI_AF << ((p & 7U) * 4U));
        ports[i]->OSPEEDR |= (3UL << (p * 2U));
        ports[i]->OTYPER &= ~(1UL << p);
        ports[i]->PUPDR &= ~(3UL << (p * 2U));
    }

    /* FSYNC：普通推挽输出，空闲高 */
    GPIO_TypeDef *fs = board_gpio(CFG_DDS_FSYNC_PORT);
    dds_fsync(0);
    fs->MODER = (fs->MODER & ~(3UL << (CFG_DDS_FSYNC_PIN * 2U)))
              | (1UL << (CFG_DDS_FSYNC_PIN * 2U));
    fs->OTYPER &= ~(1UL << CFG_DDS_FSYNC_PIN);
    fs->OSPEEDR |= (3UL << (CFG_DDS_FSYNC_PIN * 2U));
    fs->PUPDR &= ~(3UL << (CFG_DDS_FSYNC_PIN * 2U));

    uint32_t cr1 = SPI_CR1_MSTR | SPI_CR1_DFF | SPI_CR1_SSM | SPI_CR1_SSI
                 | ((uint32_t)CFG_DDS_SPI_BR << SPI_CR1_BR_Pos);
#if (CFG_DDS_SPI_MODE == 2U)
    cr1 |= SPI_CR1_CPOL;                  /* 模式 2：CPOL=1/CPHA=0 */
#else
    cr1 |= SPI_CR1_CPHA;                  /* 模式 1：CPOL=0/CPHA=1（定稿，R7 待上板） */
#endif
    SPI1->CR1 = cr1;
    SPI1->CR2 = 0U;                       /* SSOE=0，无 DMA */
    SPI1->CR1 |= SPI_CR1_SPE;
}

static void dds_write16(uint32_t word)
{
    dds_fsync(1);
    /* 帧：FSYNC↓ → 写 DR → 等 TXE → 等 BSY=0 → FSYNC↑ */
    while ((SPI1->SR & SPI_SR_TXE) == 0U) { /* 等发送寄存器空 */ }
    SPI1->DR = (uint16_t)word;
    while ((SPI1->SR & SPI_SR_TXE) == 0U) { /* 等装入移位寄存器 */ }
    while ((SPI1->SR & SPI_SR_BSY) != 0U) { /* 等 16 位全部移出 */ }
    dds_fsync(0);
}

static void dds_write_ctrl(void)
{
    dds_write16(dds_ctrl);
}

static void dds_write_freq(void)
{
    dds_write16(AD_FREQ0_L | (dds_freqreg & 0x3FFFUL));
    dds_write16(AD_FREQ0_H | ((dds_freqreg >> 14) & 0x3FFFUL));
}

/* ---- API ----------------------------------------------------------------- */
int dds_init(void)
{
    /* AD9833 上电到可写 ≥20 ms（§4.2、D19） */
    if (board_millis() < CFG_DDS_POWERUP_MS) {
        board_delay_ms(CFG_DDS_POWERUP_MS - board_millis());
    }
    dds_spi_init();

    dds_ctrl = AD_B28 | AD_RESET;
    dds_muted = 1;
    dds_freqreg = 33554UL;             /* 默认 1 kHz */
    dds_write_ctrl();                  /* 0x2100：B28 + RESET */
    dds_write_freq();                  /* 0x4000|LSB14 → 0x8000|MSB14 */
    dds_write16(AD_PHASE0);            /* 相位 0 */
    dds_ctrl = AD_B28;
    dds_write_ctrl();                  /* 0x2000：解除 RESET，开始输出 */
    dds_muted = 0;
    dds_ready = 1;
    return 0;
}

int dds_set_freqreg(uint32_t reg)
{
    if ((dds_ready == 0) || (reg == 0U) || (reg > AD_FREG_MAX)) {
        return -1;
    }
    dds_freqreg = reg;
    dds_write_freq();
    return 0;
}

int dds_set_freq_hz(uint32_t f_hz)
{
    if (f_hz == 0U) {
        return -1;
    }
    if (f_hz > 1000000U) {
        f_hz = 1000000U;               /* MCLK 8 MHz 下的使用上限 */
    }
    const uint32_t reg = (uint32_t)(((uint64_t)f_hz * 268435456ULL + 4000000ULL) / 8000000ULL);
    if (dds_set_freqreg(reg) != 0) {
        return -1;
    }
    return (int)reg;                 /* 契约 §3.4：返回实际频率字 */
}

uint32_t dds_get_freqreg(void)
{
    return dds_freqreg;
}

uint32_t dds_get_freq_hz(void)
{
    return (uint32_t)(((uint64_t)dds_freqreg * 8000000ULL) >> 28);
}

uint32_t dds_get_freq_mhz(void)
{
    return (uint32_t)(((uint64_t)dds_freqreg * 8000000000ULL) >> 28);
}

int dds_set_wave(dds_wave_t w)
{
    if (dds_ready == 0) {
        return -1;
    }
    uint32_t bits = 0U;
    switch (w) {
    case DDS_TRIANGLE: bits = AD_MODE; break;
    case DDS_SQUARE:   bits = AD_OPBITEN | AD_DIV2; break;
    case DDS_SINE:
    default:           bits = 0U; break;
    }
    dds_ctrl = AD_B28 | bits | (dds_muted ? AD_RESET : 0U);
    dds_write_ctrl();
    return 0;
}

void dds_hold_reset(int hold)
{
    if (dds_ready == 0) {
        return;
    }
    dds_muted = hold ? 1 : 0;
    dds_ctrl = (dds_ctrl & ~AD_RESET) | (dds_muted ? AD_RESET : 0U);
    dds_write_ctrl();
}

int dds_is_muted(void)
{
    return dds_muted;
}