#ifndef LCR_DDS_H
#define LCR_DDS_H

/* AD9833 激励源（§4.2）。MCLK 由 MCU 的 MCO1(PA8)=HSE 8 MHz 提供，
 * 与采样时钟同源 → f0 与 fs 严格成整数比 → 相干采样，DFT 不加窗。
 * AD9833 是只写器件，所有状态由固件缓存。 */

#include <stdint.h>

typedef enum {
    DDS_SINE = 0,
    DDS_TRIANGLE,
    DDS_SQUARE
} dds_wave_t;

int      dds_init(void);                 /* 0=OK，−1=SPI/上电失败 */
int      dds_set_freq_hz(uint32_t f_hz); /* 返回实际 28 位频率字；−1=失败（§3.4） */
int      dds_set_freqreg(uint32_t reg);  /* 扫频用：直接给频率字 */
uint32_t dds_get_freqreg(void);
uint32_t dds_get_freq_hz(void);          /* 实际 f_dds 的整数部分 */
uint32_t dds_get_freq_mhz(void);         /* 实际 f_dds，单位 mHz（C/L 换算用） */
int      dds_set_wave(dds_wave_t w);
void     dds_hold_reset(int hold);       /* 1=静音（换档/校准前），0=运行 */
int      dds_is_muted(void);

#endif /* LCR_DDS_H */