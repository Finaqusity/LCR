#ifndef LCR_MEAS_H
#define LCR_MEAS_H

/* L3 测量内核：S0 粗测 → S1 类型初判 → S2 选频选档 → S3 精测 → S4 结算 →
 * S5 双频复核（§6.1–§6.8）。所有计算在主循环，ISR 只搬运。
 *
 * 两处与架构文档的口径差异（已按协作说明 §2.1 上报 Lead，不擅自改文档）：
 *  ① §3.4 的 meas_err_t 枚举顺序与 §13.4 的串口错误码编号不一致
 *     （枚举 RANGE=1，而 §13.4 的 1 是 OPEN）。本文件保留 §3.4 的冻结枚举，
 *     另用 meas_err_code() 映射到 §13.4 的线上编号，两者都不破坏。
 *  ② §3.4 的 diag 是 int16_t，但 §13.4 明确 diag 是 32 位掩码（bit16–18 已用）。
 *     保留冻结的 int16_t diag（存低 16 位），在结构体末尾**新增** diag32
 *     （§7 允许新增字段，不改已有字段顺序/类型）。results.c 打印 diag32。 */

#include <stdint.h>
#include "board_config.h"
#include "dsp.h"          /* cfloat_t */
#include "relay.h"        /* range_t  */

/* ---- §3.4 冻结枚举（顺序不得改；末尾两个是新增）------------------------- */
typedef enum {
    MEAS_OK = 0, MEAS_ERR_RANGE, MEAS_ERR_OPEN, MEAS_ERR_SHORT, MEAS_ERR_CLIP,
    MEAS_ERR_SIGNAL, MEAS_ERR_CAL, MEAS_ERR_UNCERTAIN, MEAS_ERR_TIMEOUT,
    MEAS_ERR_CAL_S_UNSUPPORTED,   /* 新增：CAL S → 线上 ERR 9 */
    MEAS_ERR_HEADROOM             /* 新增：无档可退 → 线上 ERR 10 */
} meas_err_t;

typedef enum { ETYPE_R = 'R', ETYPE_C = 'C', ETYPE_L = 'L', ETYPE_UNKNOWN = '?' } elem_type_t;

typedef struct {
    cfloat_t zm;            /* 原始测量阻抗（未校准） */
    cfloat_t zx;            /* 校准后复阻抗 */
    float    z_mag, z_phase_deg;
    float    rs, xs, d, q;
    float    value;         /* R/C/L 主参数（串联模型） */
    elem_type_t type;
    uint32_t f_hz;
    range_t  range;
    uint8_t  cal_level, confidence;
    float    spread;        /* 块间离散度（相对） */
    float    h2, h3;
    uint16_t blocks;
    meas_err_t err;
    int16_t  diag;          /* 冻结字段：diag32 的低 16 位 */
    uint32_t diag32;        /* 新增：完整 32 位诊断掩码（§13.4） */
    uint32_t time_ms;       /* 新增：本次测量耗时 */
    uint16_t retries;       /* 新增：降档/升档重试次数 */
    float    rho;           /* 新增：Rf/|Zx|，量程裕量诊断 */
    float    vin_rms, vo_rms;   /* 新增：电平诊断（LVL / OLED P1） */
    float    slope;         /* 新增：双频 |Z| 斜率（0=未做） */
} meas_result_t;

void       meas_init(void);
meas_err_t meas_auto(meas_result_t *res);
meas_err_t meas_at(const uint32_t f_hz, const range_t r, uint16_t blocks, meas_result_t *res);
meas_err_t meas_sweep(const uint32_t *f_list, uint8_t nf, meas_result_t *out, uint8_t nmax);

/* ---- 新增 --------------------------------------------------------------- */

/* 枚举 → §13.4 线上错误码（0–10）。 */
int         meas_err_code(meas_err_t e);
const char *meas_err_name(meas_err_t e);

/* S0–S5 结构化中间结果，串口 `T` 命令打印（§6.1）。 */
typedef struct {
    uint32_t t_start_ms, t_total_ms;
    uint32_t f_seq[8];
    uint8_t  r_seq[8];
    float    rho_seq[8];
    uint8_t  steps;
    uint16_t clip_count, retry_count, blocks;
    float    z_rough, phi_rough;
    float    slope;
    uint8_t  type_primary, type_slope, type_final;
    uint8_t  state;             /* 最后到达的状态 0..5 */
    uint8_t  aborted;           /* 超时/AWD 中止 */
} meas_trace_t;

const meas_trace_t *meas_trace(void);
const meas_result_t *meas_last(void);

/* 测量中拒绝新请求（§8.5：返回 BUSY，不排队重测）。 */
int  meas_busy(void);
void meas_abort(void);          /* 请求中止（AWD/超时内部用，也可由 proto 调） */

/* 当前手动设置的频率/档位（F / R 命令写入，M 命令使用）。 */
void     meas_set_manual(uint32_t f_hz, range_t r);
uint32_t meas_manual_freq(void);
range_t  meas_manual_range(void);

/* 纯数学：ρ 选档。返回最优合法档，找不到返回 −1。
 * zmag = 粗测 |Zx|，band = 目标频点索引。 */
int meas_pick_range(float zmag, uint32_t band);

/* 纯数学：按类型选频（§6.3）。返回 band 索引。 */
int meas_pick_band(elem_type_t type, float zmag);

#endif /* LCR_MEAS_H */