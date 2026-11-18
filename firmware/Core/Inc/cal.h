#ifndef LCR_CAL_H
#define LCR_CAL_H

/* L2 校准：α 通道失配 + 开路 + 双负载两点法（替代短路校准），Flash A/B 日志。
 * 依据 docs/软件架构.md §7.1–§7.7、§8.2、§13.4。
 *
 * 模型（§7.4）：  Zm = Zs + k·(Zx ∥ Zp)
 *   Zm  = −Zf_eff(f)·(m1/m2)/α   （名义 Rf/Cf 算出的原始阻抗）
 *   Zs  = 串联残余（引线/触点/继电器导通电阻）
 *   k   = 复增益（吸收 Rf 绝对公差 + Zf_eff 频响残差）
 *   Zp  = 夹具并联杂散（1–3 pF）
 * 反解：  X = (Zm − Zs)/k ;  Zx = Zp·X/(Zp − X)   （Zp 无效时 Zx = X，method=1）
 *
 * 本硬件不做短路校准：激励固定 0.5 Vrms，短路时 I≈0.5/Rf，10 Ω 档达 5 A，
 * 会烧 Rf/BUF634A。`CAL S` 必须返回 ERR 9 CAL_S_UNSUPPORTED（§7.4.1、D10）。
 *
 * 求解全程 double（§8.2）：10 Ω 档 Zm−Zs 相消需要 4.25 位十进制有效数字。 */

#include <stdint.h>
#include "board_config.h"
#include "dsp.h"          /* cfloat_t */
#include "relay.h"        /* range_t  */

#define FREQ_NUM CFG_FREQ_NUM

/* ---- §3.4 冻结结构（字段顺序/类型不得改动）------------------------------ */
typedef struct { cfloat_t alpha; } cal_alpha_t;

typedef struct {
    cfloat_t zopen, zload1, zload2, k_scale, zs;
    uint8_t  valid;
} cal_net_t;

typedef struct { cfloat_t k_scale, zs; uint8_t valid; } cal_load_t;

typedef struct {
    uint32_t    magic, version, crc, built_ts;
    cal_alpha_t alpha[FREQ_NUM];
    cal_net_t   net[FREQ_NUM][RANGE_NUM];
    cal_load_t  load[RANGE_NUM];
    float       k_in, k_out;
    uint16_t    build_count;
} cal_record_t;

int  cal_init(void);
int  cal_load(cal_record_t *rec);
int  cal_save(const cal_record_t *rec);
int  cal_erase(void);
int  cal_get_level(void);
int  cal_measure_alpha(uint32_t f_hz, cal_alpha_t *out);
int  cal_measure_open(uint32_t f_hz, range_t r, cal_net_t *out);
int  cal_measure_load(uint32_t f_hz, range_t r, float rstd1, float rstd2, cal_load_t *out);
int  cal_apply_impedance(uint32_t f_hz, range_t r, cfloat_t zm, cfloat_t *z_out);

/* ---- 新增（§7 允许新增函数/字段，不改已有签名）-------------------------- */

/* 频点 → band 索引（0..3）；不是四个标称频点之一返回 −1。 */
int cal_band_of(uint32_t f_hz);

/* 名义反馈阻抗 Zf_eff = Rf/(1+j2πf·Rf·Cf)（§6.2）。 */
cfloat_t cal_zf_eff(uint32_t f_hz, range_t r);

/* 原始阻抗：Zm = −Zf_eff·(m1/m2)/α。ratio 已是 dsp_ratio_mean 的块间平均。 */
cfloat_t cal_raw_impedance(uint32_t f_hz, range_t r, cfloat_t ratio);

/* 当前生效的校准记录（只读）。cal_init 之前是全零名义态。 */
const cal_record_t *cal_record(void);

/* 夹具变化检测：开路杂散电容相对上次校准变化 >0.2 pF → stale（§7.5）。 */
int  cal_is_stale(void);
void cal_set_stale(int stale);

/* 台架复检通过后置位，cal_level 才能到 4（§7.1）。 */
void cal_set_bench_ok(int ok);
int  cal_bench_ok(void);

/* 检查件复核：返回相对误差百分数（|Zx|−rstd)/rstd·100；失败返回负的错误码。 */
int cal_check_std(uint32_t f_hz, range_t r, float rstd, float *err_pct);

/* 电平系数 k_in/k_out（码值→Vrms），1 kHz 测一次，只影响显示与诊断（§7.1）。 */
int cal_measure_level(float *k_in, float *k_out);

/* 该 (频点,档位) 组合是否合法：1=可用 2=仅校准后 0=禁止（§6.3）。 */
int cal_range_legal(range_t r, uint32_t f_hz);

/* Flash 日志状态（SELFTEST / `I` 命令用）。 */
uint32_t cal_flash_seq(void);
int      cal_flash_slot(void);      /* 当前生效槽号 0..84，−1=无 */
int      cal_flash_bank(void);      /* 0=A(sector6) 1=B(sector7) −1=无 */
uint32_t cal_flash_crc(void);
uint32_t cal_flash_writes(void);    /* 累计写入次数（寿命统计） */

/* 本模块累计的诊断位（DIAG_*），取走后清零。meas/selftest 合并进结果。 */
uint32_t cal_take_diag(void);

/* 该档的双负载标准电阻对 {Z1,Z2}（§7.4 表）；非法档返回 NULL。 */
const float *cal_load_std_table(range_t r);

/* 双负载闭式解（纯数学，可在 PC 上跑金标准向量 G7/G8）。
 * 输入：两次标准件实测 Zm1/Zm2、开路 Zopen、标准件真值 rstd1/rstd2（纯电阻）。
 * 输出：k/zs/zp。返回 0=OK，−1=病态（|W1−W2| 过小），−2=Zp 非物理。 */
int cal_solve_two_load(double zm1_re, double zm1_im,
                       double zm2_re, double zm2_im,
                       double zopen_re, double zopen_im,
                       double rstd1, double rstd2,
                       double *k_re, double *k_im,
                       double *zs_re, double *zs_im,
                       double *zp_re, double *zp_im);

/* 反解（纯数学，double）：Zm → Zx。zp_valid=0 时走 method=1（Zx=X）。 */
int cal_solve_inverse(double zm_re, double zm_im,
                      double zs_re, double zs_im,
                      double k_re, double k_im,
                      double zp_re, double zp_im, int zp_valid,
                      double *zx_re, double *zx_im);

#endif /* LCR_CAL_H */