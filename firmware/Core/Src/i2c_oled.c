/* L1 OLED：I2C1 主机 + SSD1306 128×64，400 kHz。
 * 依据 docs/软件架构.md §4.6、§8.5。
 *
 * 时钟：I2C1 挂 APB1 = 42 MHz → CR2.FREQ = 42。
 * 快速模式 400 kHz、占空比 2:1 → CCR = 42e6/(3×400e3) = 35；
 * TRISE = 42e6×300 ns + 1 = 13.6 → 13（§13.2）。
 * 所有等待都带 10 ms 超时；失败先 SWRST + 9 时钟恢复，再不行就降级，
 * 绝不阻塞测量（§4.6：OLED 无模块时不影响出数）。 */

#include "i2c_oled.h"
#include "board.h"
#include "board_config.h"

#include <string.h>

static uint8_t  oled_fb[CFG_OLED_FB_SIZE];
static uint8_t  oled_addr;
static int      oled_ok;
static uint32_t oled_err;

/* ---- 5×7 字库（ASCII 32..126，每字符 5 列，列内 bit0=顶行）-------------- */
static const uint8_t font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* space */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x14,0x08,0x3E,0x08,0x14}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x08,0x14,0x22,0x41,0x00}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x00,0x41,0x22,0x14,0x08}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x7F,0x41,0x41,0x00}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* backslash */
    {0x00,0x41,0x41,0x7F,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x00,0x01,0x02,0x04,0x00}, /* ` */
    {0x20,0x54,0x54,0x54,0x78}, /* a */
    {0x7F,0x48,0x44,0x44,0x38}, /* b */
    {0x38,0x44,0x44,0x44,0x20}, /* c */
    {0x38,0x44,0x44,0x48,0x7F}, /* d */
    {0x38,0x54,0x54,0x54,0x18}, /* e */
    {0x08,0x7E,0x09,0x01,0x02}, /* f */
    {0x0C,0x52,0x52,0x52,0x3E}, /* g */
    {0x7F,0x08,0x04,0x04,0x78}, /* h */
    {0x00,0x44,0x7D,0x40,0x00}, /* i */
    {0x20,0x40,0x44,0x3D,0x00}, /* j */
    {0x7F,0x10,0x28,0x44,0x00}, /* k */
    {0x00,0x41,0x7F,0x40,0x00}, /* l */
    {0x7C,0x04,0x18,0x04,0x78}, /* m */
    {0x7C,0x08,0x04,0x04,0x78}, /* n */
    {0x38,0x44,0x44,0x44,0x38}, /* o */
    {0x7C,0x14,0x14,0x14,0x08}, /* p */
    {0x08,0x14,0x14,0x18,0x7C}, /* q */
    {0x7C,0x08,0x04,0x04,0x08}, /* r */
    {0x48,0x54,0x54,0x54,0x20}, /* s */
    {0x04,0x3F,0x44,0x40,0x20}, /* t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* w */
    {0x44,0x28,0x10,0x28,0x44}, /* x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* y */
    {0x44,0x64,0x54,0x4C,0x44}, /* z */
    {0x00,0x08,0x36,0x41,0x00}, /* { */
    {0x00,0x00,0x7F,0x00,0x00}, /* | */
    {0x00,0x41,0x36,0x08,0x00}, /* } */
    {0x08,0x04,0x08,0x10,0x08}  /* ~ */
};

/* ---- I2C1 底层 ---------------------------------------------------------- */
#define OLED_SCL_PORT CFG_I2C_SCL_PORT
#define OLED_SCL_PIN  CFG_I2C_SCL_PIN
#define OLED_SDA_PORT CFG_I2C_SDA_PORT
#define OLED_SDA_PIN  CFG_I2C_SDA_PIN

static void oled_gpio_af(void)
{
    GPIO_TypeDef *p = board_gpio(CFG_PORT_B);
    const uint32_t scl = OLED_SCL_PIN;
    const uint32_t sda = OLED_SDA_PIN;
    const uint32_t mask = (3UL << (scl * 2U)) | (3UL << (sda * 2U));
    /* 复用开漏（模块自带 4.7–10 kΩ 上拉），高速 */
    p->MODER = (p->MODER & ~mask) | (2UL << (scl * 2U)) | (2UL << (sda * 2U));
    p->OTYPER |= (1UL << scl) | (1UL << sda);
    p->OSPEEDR |= mask;
    p->PUPDR &= ~mask;                 /* 外部已有上拉，不再内部并联 */
    p->AFR[scl >> 3U] = (p->AFR[scl >> 3U] & ~(0xFUL << ((scl & 7U) * 4U)))
                      | ((uint32_t)CFG_I2C_AF << ((scl & 7U) * 4U));
    p->AFR[sda >> 3U] = (p->AFR[sda >> 3U] & ~(0xFUL << ((sda & 7U) * 4U)))
                      | ((uint32_t)CFG_I2C_AF << ((sda & 7U) * 4U));
}

static void oled_i2c_config(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;
    I2C1->CR1 = I2C_CR1_SWRST;         /* 软复位，清掉任何卡死状态 */
    I2C1->CR1 = 0U;
    I2C1->CR2 = (uint32_t)(CFG_PCLK1_HZ / 1000000U);          /* FREQ = 42 */
    I2C1->CCR = I2C_CCR_FS | I2C_CCR_DUTY | 35U;              /* 400 kHz, 2:1 */
    I2C1->TRISE = 13U;                                        /* 300 ns × 42 MHz + 1 */
    I2C1->OAR1 = (1UL << 14U) | (0x00U << 1U);                /* 自身地址 0（主机无关） */
    I2C1->CR1 = I2C_CR1_PE;
}

void i2c_oled_bus_recover(void)
{
    /* 从机可能卡在拉低 SDA：把两脚切成 GPIO 开漏，手动打 9 个 SCL 脉冲，
     * 再补一个 STOP，然后重新初始化外设。 */
    I2C1->CR1 &= ~I2C_CR1_PE;
    GPIO_TypeDef *p = board_gpio(CFG_PORT_B);
    const uint32_t scl = OLED_SCL_PIN;
    const uint32_t sda = OLED_SDA_PIN;
    const uint32_t mask = (3UL << (scl * 2U)) | (3UL << (sda * 2U));
    p->MODER = (p->MODER & ~mask) | (1UL << (scl * 2U)) | (1UL << (sda * 2U));
    p->OTYPER |= (1UL << scl) | (1UL << sda);
    p->BSRR = (1UL << scl) | (1UL << sda);          /* 释放两线 */
    for (int i = 0; i < 9; ++i) {
        p->BSRR = (1UL << (scl + 16U));             /* SCL 低 */
        board_delay_us(5U);
        p->BSRR = (1UL << scl);                     /* SCL 高 */
        board_delay_us(5U);
    }
    /* STOP：SCL 高时 SDA 由低变高 */
    p->BSRR = (1UL << (sda + 16U));
    board_delay_us(5U);
    p->BSRR = (1UL << scl);
    board_delay_us(5U);
    p->BSRR = (1UL << sda);
    board_delay_us(5U);
    oled_gpio_af();
    oled_i2c_config();
}

static int oled_wait(uint32_t flag, int set)
{
    const uint32_t t0 = board_millis();
    for (;;) {
        const uint32_t sr1 = I2C1->SR1;
        if (((sr1 & flag) != 0U) == (set != 0)) {
            return 0;
        }
        if ((sr1 & (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR)) != 0U) {
            I2C1->SR1 = ~(I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR);
            ++oled_err;
            return -1;
        }
        if ((board_millis() - t0) >= CFG_I2C_TIMEOUT_MS) {
            ++oled_err;
            return -1;
        }
    }
}

/* 发一串字节（含 SSD1306 的控制字节）。返回 0=OK。 */
static int oled_tx(const uint8_t *data, uint32_t len)
{
    if (oled_ok == 0) {
        return -1;
    }
    if ((I2C1->SR2 & I2C_SR2_BUSY) != 0U) {
        i2c_oled_bus_recover();
    }
    I2C1->CR1 |= I2C_CR1_START;
    if (oled_wait(I2C_SR1_SB, 1) != 0) {
        goto fail;
    }
    I2C1->DR = (uint32_t)(oled_addr << 1U);         /* 写方向 */
    if (oled_wait(I2C_SR1_ADDR, 1) != 0) {
        goto fail;
    }
    (void)I2C1->SR2;                                /* 读 SR2 清 ADDR */
    for (uint32_t i = 0U; i < len; ++i) {
        if (oled_wait(I2C_SR1_TXE, 1) != 0) {
            goto fail;
        }
        I2C1->DR = data[i];
    }
    if (oled_wait(I2C_SR1_BTF, 1) != 0) {
        goto fail;
    }
    I2C1->CR1 |= I2C_CR1_STOP;
    return 0;

fail:
    I2C1->CR1 |= I2C_CR1_STOP;
    i2c_oled_bus_recover();
    /* 恢复后重试一次；仍失败则降级，绝不阻塞测量 */
    return -1;
}

static int oled_cmd(uint8_t c)
{
    const uint8_t b[2] = { 0x00U, c };              /* Co=0, D/C#=0 → 命令 */
    return oled_tx(b, 2U);
}

static int oled_probe(uint8_t addr)
{
    oled_addr = addr;
    I2C1->CR1 |= I2C_CR1_START;
    if (oled_wait(I2C_SR1_SB, 1) != 0) {
        return -1;
    }
    I2C1->DR = (uint32_t)(addr << 1U);
    if (oled_wait(I2C_SR1_ADDR, 1) != 0) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return -1;
    }
    (void)I2C1->SR2;
    I2C1->CR1 |= I2C_CR1_STOP;
    return 0;
}

int i2c_oled_init(void)
{
    oled_err = 0U;
    oled_ok = 0;
    oled_gpio_af();
    oled_i2c_config();

    /* SSD1306 模块地址两种：0x3C（常见）/0x3D。逐个探测，不猜死。 */
    if (oled_probe(CFG_OLED_ADDR_LO) != 0) {
        if (oled_probe(CFG_OLED_ADDR_HI) != 0) {
            oled_ok = 0;
            return -1;                              /* 降级：无模块 */
        }
    }
    oled_ok = 1;

    static const uint8_t init_seq[] = {
        0xAE,             /* display off */
        0xD5, 0x80,       /* clock divide = 1, osc 默认 */
        0xA8, 0x3F,       /* multiplex = 64 */
        0xD3, 0x00,       /* display offset = 0 */
        0x40,             /* start line = 0 */
        0x8D, 0x14,       /* charge pump on（模块内部升压） */
        0x20, 0x00,       /* 水平寻址模式 */
        0xA1,             /* segment remap（左右翻转，配合安装方向） */
        0xC8,             /* COM scan dec（上下翻转） */
        0xDA, 0x12,       /* COM pins = alternative, 128×64 */
        0x81, 0xCF,       /* contrast */
        0xD9, 0xF1,       /* pre-charge */
        0xDB, 0x40,       /* VCOMH deselect */
        0xA4,             /* 输出跟随 RAM */
        0xA6,             /* 正常（非反显） */
        0xAF              /* display on */
    };
    for (uint32_t i = 0U; i < sizeof(init_seq); ++i) {
        if (oled_cmd(init_seq[i]) != 0) {
            oled_ok = 0;
            return -1;
        }
    }
    memset(oled_fb, 0, sizeof(oled_fb));
    i2c_oled_flush();
    return 0;
}

int i2c_oled_present(void) { return oled_ok; }
uint8_t i2c_oled_addr(void) { return oled_addr; }
uint32_t i2c_oled_errors(void) { return oled_err; }

/* ---- 帧缓冲绘制 --------------------------------------------------------- */
void i2c_oled_clear(void) { memset(oled_fb, 0, sizeof(oled_fb)); }

void i2c_oled_pixel(uint8_t x, uint8_t y, int on)
{
    if ((x >= CFG_OLED_WIDTH) || (y >= CFG_OLED_HEIGHT)) {
        return;
    }
    const uint32_t idx = (uint32_t)(y >> 3U) * CFG_OLED_WIDTH + x;
    const uint8_t bit = (uint8_t)(1U << (y & 7U));
    if (on != 0) {
        oled_fb[idx] |= bit;
    } else {
        oled_fb[idx] = (uint8_t)(oled_fb[idx] & (uint8_t)~bit);
    }
}

void i2c_oled_text(uint8_t page, uint8_t col, const char *s)
{
    if ((page >= 8U) || (s == 0)) {
        return;
    }
    uint32_t x = col;
    while (*s != '\0') {
        const char c = *s++;
        if ((c < 32) || (c > 126)) {
            continue;
        }
        if (x + 5U > CFG_OLED_WIDTH) {
            break;
        }
        const uint8_t *g = font5x7[(uint32_t)(c - 32)];
        for (uint32_t i = 0U; i < 5U; ++i) {
            oled_fb[(uint32_t)page * CFG_OLED_WIDTH + x + i] = g[i];
        }
        x += 6U;                       /* 5 列 + 1 列间隔 */
        if (x < CFG_OLED_WIDTH) {
            oled_fb[(uint32_t)page * CFG_OLED_WIDTH + x - 1U] = 0U;
        }
    }
}

void i2c_oled_text_big(uint8_t page, uint8_t col, const char *s)
{
    if ((page >= 7U) || (s == 0)) {
        return;
    }
    uint32_t x = col;
    while (*s != '\0') {
        const char c = *s++;
        if ((c < 32) || (c > 126)) {
            continue;
        }
        if (x + 10U > CFG_OLED_WIDTH) {
            break;
        }
        const uint8_t *g = font5x7[(uint32_t)(c - 32)];
        for (uint32_t i = 0U; i < 5U; ++i) {
            /* 每列横向扩 2 倍、纵向扩 2 倍（占两页） */
            uint16_t v = 0U;
            for (uint32_t b = 0U; b < 7U; ++b) {
                if ((g[i] & (1U << b)) != 0U) {
                    v |= (uint16_t)(3U << (b * 2U));
                }
            }
            for (uint32_t k = 0U; k < 2U; ++k) {
                const uint32_t cx = x + i * 2U + k;
                oled_fb[(uint32_t)page * CFG_OLED_WIDTH + cx] = (uint8_t)(v & 0xFFU);
                oled_fb[(uint32_t)(page + 1U) * CFG_OLED_WIDTH + cx] = (uint8_t)(v >> 8U);
            }
        }
        x += 12U;                      /* 10 列 + 2 列间隔 */
    }
}

/* ---- 刷屏 --------------------------------------------------------------- */
static int oled_set_window(uint8_t page_first, uint8_t page_last,
                           uint8_t col_first, uint8_t col_last)
{
    /* 水平寻址模式（0x20,0x00）下用 0x21/0x22 设列/页范围 */
    const uint8_t colcmd[4] = { 0x00U, 0x21U, col_first, col_last };
    const uint8_t pagecmd[4] = { 0x00U, 0x22U, page_first, page_last };
    if (oled_tx(colcmd, sizeof(colcmd)) != 0) {
        return -1;
    }
    return oled_tx(pagecmd, sizeof(pagecmd));
}

void i2c_oled_flush_pages(uint8_t first, uint8_t count)
{
    if ((oled_ok == 0) || (count == 0U) || (first >= 8U)) {
        return;
    }
    if ((uint32_t)first + count > 8U) {
        count = (uint8_t)(8U - first);
    }
    if (oled_set_window(first, (uint8_t)(first + count - 1U), 0U,
                        (uint8_t)(CFG_OLED_WIDTH - 1U)) != 0) {
        return;
    }
    /* 数据帧：控制字节 0x40 + 最多 128 B 数据，分页发避免大栈缓冲 */
    uint8_t buf[1 + 16];
    for (uint32_t p = first; p < (uint32_t)first + count; ++p) {
        for (uint32_t c = 0U; c < CFG_OLED_WIDTH; c += 16U) {
            buf[0] = 0x40U;
            memcpy(&buf[1], &oled_fb[p * CFG_OLED_WIDTH + c], 16U);
            if (oled_tx(buf, sizeof(buf)) != 0) {
                return;
            }
        }
    }
}

void i2c_oled_flush(void)
{
    i2c_oled_flush_pages(0U, 8U);
}