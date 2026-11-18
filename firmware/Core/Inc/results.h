#ifndef LCR_RESULTS_H
#define LCR_RESULTS_H

/* L3 结果：参数提取（§6.6）+ 定点格式化（§8.4：不开 -u_printf_float）。
 * 本文件不依赖 CMSIS，可在 PC 上直接编译跑金标准向量。
 * 浮点只参与数值计算；所有输出用整数手工排版。 */

#include <stdint.h>
#include "meas.h"

/* Zx → Rs/Xs/|Z|/φ/D/Q/主参数/类型。f_mhz = DDS 实际频率（mHz，不能用整数 Hz
 * 截断：100 Hz 格点实际 99 986.792 mHz，截断会给 C/L 带来 0.013% 偏置）。 */
void results_from_zx(meas_result_t *res, cfloat_t zx, uint32_t f_mhz);

/* 类型被双频斜率复核改写后，按新类型重算主参数（单位换算仍集中在 results.c）。 */
void results_set_type(meas_result_t *res, elem_type_t t, uint32_t f_mhz);

/* 主判据：|φ|<5.7° → R；φ<−84° → C；φ>+84° → L；其余 UNKNOWN（§6.5、D9）。 */
elem_type_t results_type_of(float phi_deg);

/* 辅判据：|Z|(10k)/|Z|(1k) <0.2 → C；>5 → L；0.5–2 → R；其余 UNKNOWN。 */
elem_type_t results_type_of_slope(float slope);

/* confidence：判据清晰 + cal_level≥3 + spread<0.1% → 3，缺一项降 1（§6.5）。 */
uint8_t results_confidence(int type_clear, uint8_t cal_level, float spread);

const char *results_unit(elem_type_t t);      /* "ohm" | "F" | "H" */

/* 4 位有效数字 + 工程前缀（p/n/u/m/空/k/M），OLED 与人类可读输出用。 */
int results_format_eng(float v, char *buf, uint32_t cap);

/* 4 位有效数字、基本单位（ohm/F/H）。[1e-3,1e6) 用普通小数，之外用 d.dddE±xx
 * （Excel 可直接解析）。串口结果行的 val/|Z|/R/X 用这个。 */
int results_format_sig(float v, char *buf, uint32_t cap);

/* 定点小数（dec 位），D/Q 用 3 位、φ 用 1 位。 */
int results_format_fixed(float v, uint8_t dec, char *buf, uint32_t cap);

/* 完整结果行（§8.5）：Z,<|Z|>,<phi>,R=,X=,type=,val=,unit=,D=,Q=,f=,range=,
 * cal=,conf=,spread=,h2=,t=,err=  ；返回写入字符数，buf 不足时截断并返回负值。 */
int results_format_line(const meas_result_t *res, uint32_t time_ms,
                        char *buf, uint32_t cap);

/* OLED P0 主行：类型 + 数值 + 单位（工程前缀）。 */
int results_format_main(const meas_result_t *res, char *buf, uint32_t cap);

#endif /* LCR_RESULTS_H */