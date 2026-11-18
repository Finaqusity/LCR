#ifndef LCR_RELAY_H
#define LCR_RELAY_H

/* 六档 Rf 量程继电器 + CAL 开关（§4.3、D14/D15/D16）。
 * PD0=10 Ω, PD1=100 Ω, PD2=1 k, PD3=10 k(常闭默认), PD4=100 k, PD5=1 M, PD6=CAL。
 * 10 kΩ 档接常闭触点：全部线圈断电时它自动在位，I-V 不会开环冲轨。
 * 选其他档必须"目标档 + PD3"两只同时吸合（~42 mA），顺序先吸目标再断常闭。
 * 位号映射（K18=K2 等）无网表依据，固件只认 GPIO 功能表（D15/R15）。 */

#include <stdint.h>

typedef enum {
    RANGE_10R = 0,
    RANGE_100R,
    RANGE_1K,
    RANGE_10K,
    RANGE_100K,
    RANGE_1M,
    RANGE_NUM
} range_t;

void     relay_init(void);
void     relay_all_off(void);          /* 全断 = 回到常闭 10 kΩ 档（失效安全） */
int      relay_select_range(range_t r);/* 返回稳定耗时 ms；−1=非法档位；内部先静音 DDS */
void     relay_cal_enable(int on);     /* PD6：1=两路同切 CAL，0=正常测量 */
range_t  relay_current_range(void);
int      relay_cal_is_on(void);
uint32_t relay_switch_count(void);
int      relay_verify(void);           /* 回读 ODR 与期望一致？0=OK */
const char *relay_range_name(range_t r);

#endif /* LCR_RELAY_H */