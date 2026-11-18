#ifndef LCR_I2C_OLED_H
#define LCR_I2C_OLED_H

/* L1 OLED：I2C1 PB6(SCL)/PB7(SDA) AF4，400 kHz，SSD1306 类 128×64。
 * 依据 docs/软件架构.md §4.6、§8.5。
 * 模块不在位/通信失败 → 降级：i2c_oled_present()=0，ui 不阻塞测量。 */

#include <stdint.h>

int      i2c_oled_init(void);      /* 0=OK；−1=探测不到模块（降级，不算致命） */
int      i2c_oled_present(void);
uint8_t  i2c_oled_addr(void);      /* 实测 0x3C 或 0x3D */
uint32_t i2c_oled_errors(void);    /* 超时/AF/BERR 累计 */

void i2c_oled_clear(void);
void i2c_oled_pixel(uint8_t x, uint8_t y, int on);
/* 5×7 字库，字符间隔 1 px；page 0..7，col 0..127。超出右边界自动截断。 */
void i2c_oled_text(uint8_t page, uint8_t col, const char *s);
/* 2× 缩放（占 2 页），P0 主数值用。 */
void i2c_oled_text_big(uint8_t page, uint8_t col, const char *s);
void i2c_oled_flush(void);                            /* 整屏 1024 B ≈23 ms */
void i2c_oled_flush_pages(uint8_t first, uint8_t count);  /* 局部 ≈5.8 ms/2页 */

/* 总线恢复：SWRST + 9 个 SCL 脉冲 + STOP（从机卡在拉低 SDA 时用）。 */
void i2c_oled_bus_recover(void);

#endif /* LCR_I2C_OLED_H */