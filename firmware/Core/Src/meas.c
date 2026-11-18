/* L3 测量内核：S0 粗测 → S1 类型初判 → S2 选频选档 → S3 精测 → S4 结算 →
 * S5 双频复核（§6.1–§6.8）。
 *
 * 硬规则（§3.7）：任何失败都必须归类到 OPEN/SHORT/CLIP/RANGE/CAL/SIGNAL/
 * TIMEOUT/UNCERTAIN/HEADROOM 之一，绝不静默返回一个看似正常的数。
 * 实时性：本文件全部在主循环跑；ISR 只搬运（adc.c）。
 * 时间预算：总超时 CFG_MEAS_TIMEOUT_MS=950 ms（§9.3），每块采集前都检查。 */

#include "meas.h"
#include "results.h"
#include "cal.h"
#include "board.h"
#include "dds.h"
#include "relay.h"
#include "adc.h"
#include "dsp.h"
#include "error.h"
#include "uart.h"

#include <math.h>
#include <string.h>

#define MEAS_BLK_MAX 32U

static const cfg_band_t m_bands[CFG_FREQ_NUM] = CFG_BANDS_INITIALIZER;
static const float m_rf[CFG_RANGE_NUM] = CFG_RF_NOM_INITIALIZER;
static const float m_rho_max[CFG_RANGE_NUM] = CFG_RHO_MAX_INITIALIZER;
static const uint8_t m_legal[CFG_RANGE_NUM][CFG_FREQ_NUM] = CFG_RANGE_FREQ_LEGAL_INITIALIZER;

static meas_result_t m_last;
static meas_trace_t  m_trace;
static volatile int  m_busy;
static volatile int  m_abort_req;
static uint32_t      m_t0;
static uint32_t      m_diag;
static uint32_t      m_manual_f = 1000U;
static range_t       m_manual_r = RANGE_10K;
/* 只缓存每块复比值（8 B/块 ×32 = 256 B）+ 单块谱 scratch，
 * 不缓存整谱数组，满足 §8.3 DFT 工作区 ≤4 KB。 */
static cfloat_t      m_ratio[MEAS_BLK_MAX];
static dsp_result_t  m_scratch;

/* ---- 错误码映射（§3.4 冻结枚举 → §13.4 线上编号）----------------------- */
int meas_err_code(meas_err_t e)
{
    switch (e) {
    case MEAS_OK:                  return 0;
    case MEAS_ERR_OPEN:            return 1;
    case MEAS_ERR_SHORT:           return 2;
    case MEAS_ERR_CLIP:            return 3;
    case MEAS_ERR_RANGE:           return 4;
    case MEAS_ERR_CAL:             return 5;
    case MEAS_ERR_SIGNAL:          return 6;
    case MEAS_ERR_TIMEOUT:         return 7;
    case MEAS_ERR_UNCERTAIN:       return 8;
    case MEAS_ERR_CAL_S_UNSUPPORTED: return 9;
    case MEAS_ERR_HEADROOM:        return 10;
    default:                       return 8;
    }
}

const char *meas_err_name(meas_err_t e)
{
    switch (meas_err_code(e)) {
    case 0:  return "OK";
    case 1:  return "OPEN";
    case 2:  return "SHORT";
    case 3:  return "CLIP";
    case 4:  return "RANGE";
    case 5:  return "CAL";
    case 6:  return "SIGNAL";
    case 7:  return "TIMEOUT";
    case 8:  return "UNCERTAIN";
    case 9:  return "CAL_S_UNSUPPORTED";
    case 10: return "HEADROOM";
    default: return "UNCERTAIN";
    }
}

/* ---- 时间 / 中止 -------------------------------------------------------- */
static uint32_t m_elapsed(void) { return board_millis() - m_t0; }

static int m_out_of_time(void)
{
    if (m_elapsed() >= CFG_MEAS_TIMEOUT_MS) {
        m_diag |= DIAG_TIMEOUT;
        m_trace.aborted = 1U;
        return 1;
    }
    return 0;
}

static int m_aborted(void)
{
    return (m_abort_req != 0) || (m_out_of_time() != 0);
}

void meas_abort(void) { m_abort_req = 1; }
int  meas_busy(void) { return m_busy; }

/* ---- trace -------------------------------------------------------------- */
static void m_step(uint32_t f, range_t r, float rho)
{
    if (m_trace.steps < 8U) {
        m_trace.f_seq[m_trace.steps] = f;
        m_trace.r_seq[m_trace.steps] = (uint8_t)r;
        m_trace.rho_seq[m_trace.steps] = rho;
        ++m_trace.steps;
    }
}

/* ---- 选频 / 选档（纯数学，可离线验证）----------------------------------- */
/* 由 1 kHz 粗测 |Z| 外推其它频点的 |Z|（R 平坦、C ∝ 1/f、L ∝ f）。 */
static float m_predict_z(elem_type_t type, float z1k, uint32_t band)
{
    const double ratio = (double)m_bands[band].f_nom / 1000.0;
    if (type == ETYPE_C) {
        return (float)((double)z1k / ratio);
    }
    if (type == ETYPE_L) {
        return (float)((double)z1k * ratio);
    }
    return z1k;
}

/* 该 (档,频) 组合当前是否真的可用：合法表 + 校准前置条件（§6.3）。 */
static int m_combo_usable(range_t r, uint32_t band)
{
    const uint8_t leg = m_legal[(uint32_t)r][band];
    if (leg == 0U) {
        return 0;
    }
    const cal_record_t *rec = cal_record();
    const int open_ok = (rec->net[band][(uint32_t)r].valid != 0U);
    if (leg == 2U) {
        /* "仅校准后"：必须有本组合的开路数据且夹具未动 */
        return (open_ok != 0 && cal_is_stale() == 0) ? 1 : 0;
    }
    if (band == CFG_BAND_100KHZ) {
        /* 100 kHz 额外要求开路数据是本频率的（§6.3） */
        return (open_ok != 0 && cal_is_stale() == 0) ? 1 : 0;
    }
    return 1;
}

int meas_pick_range(float zmag, uint32_t band)
{
    if ((band >= CFG_FREQ_NUM) || !(zmag > 0.0f) || !isfinite(zmag)) {
        return -1;
    }
    int best = -1;
    float best_dist = 1e30f;
    int best_in_window = 0;
    for (uint32_t r = 0U; r < CFG_RANGE_NUM; ++r) {
        if (m_combo_usable((range_t)r, band) == 0) {
            continue;
        }
        const float rho = m_rf[r] / zmag;
        const int in_window = (rho >= CFG_RHO_MIN && rho <= m_rho_max[r]) ? 1 : 0;
        /* 以 log 距离衡量"离 ρ=0.3 有多远"，跨 6 个数量级时比线性距离合理 */
        const float dist = fabsf(logf(rho / CFG_RHO_TARGET));
        if ((in_window > best_in_window)
            || ((in_window == best_in_window) && (dist < best_dist))) {
            best = (int)r;
            best_dist = dist;
            best_in_window = in_window;
        }
    }
    return best;
}

int meas_pick_band(elem_type_t type, float zmag)
{
    if (!(zmag > 0.0f)) {
        return (int)CFG_BAND_1KHZ;
    }
    if (type == ETYPE_C) {
        for (int b = (int)CFG_FREQ_NUM - 1; b >= 0; --b) {   /* 最高可用频率 */
            if (meas_pick_range(m_predict_z(type, zmag, (uint32_t)b), (uint32_t)b) >= 0) {
                return b;
            }
        }
    } else if (type == ETYPE_L) {
        for (uint32_t b = 0U; b < CFG_FREQ_NUM; ++b) {        /* 最低可用频率 */
            if (meas_pick_range(m_predict_z(type, zmag, b), b) >= 0) {
                return (int)b;
            }
        }
    }
    return (int)CFG_BAND_1KHZ;                                /* R / 未知：1 kHz */
}

/* ---- 采集：逐块累加 + §5.5 复比平均 + 提前停止 --------------------------- */
typedef struct {
    cfloat_t ratio;
    float    spread_lin, spread_ph;
    float    vin_rms, vo_rms;
    float    h2, h3;
    uint16_t blocks, sat;
    uint8_t  quality;
    int      adc_err;
} m_acq_t;

static void m_acq_init(m_acq_t *a)
{
    memset(a, 0, sizeof(*a));
    a->spread_lin = 1e30f;
    a->spread_ph = 1e30f;
}

/* 采 k 块（逐块，便于提前停止与超时检查）。返回 0=OK，负=ADC 错误码。 */
static int m_acquire(uint32_t band, uint16_t n, uint16_t k, m_acq_t *a)
{
    m_acq_init(a);
    acq_cfg_t cfg;
    if (adc_make_cfg(band, n, &cfg) != 0) {
        return ADC_ERR_CFG;
    }
    if ((k == 0U) || (k > MEAS_BLK_MAX)) {
        k = 1U;
    }
    adc_clear_errors();
    float vin_peak = 0.0f;
    float vo_peak = 0.0f;

    for (uint16_t i = 0U; i < k; ++i) {
        if (m_aborted() != 0) {
            a->adc_err = ADC_ERR_TIMEOUT;
            break;
        }
        static adc_block_t blk;   /* 4 KB，放 .bss 不上栈（§8.1 栈仅 4 KB） */
        const int rc = adc_acquire(&cfg, &blk);
        if (rc != ADC_OK) {
            a->adc_err = rc;
            if (rc == ADC_ERR_AWD)  { m_diag |= DIAG_AWD_ABORT; }
            if (rc == ADC_ERR_CLIP) { m_diag |= DIAG_CLIP; ++m_trace.clip_count; }
            if (rc == ADC_ERR_OVR)  { m_diag |= DIAG_ADC_OVR; }
            if (rc == ADC_ERR_TIMEOUT) { m_diag |= DIAG_TIMEOUT; }
            break;
        }
        dsp_analyze(&blk, &m_scratch);
        if (blk.dma_err != 0U) {
            m_diag |= DIAG_DMA_ERR;
        }
        a->sat = (uint16_t)(a->sat + blk.sat_count);
        a->quality |= m_scratch.quality;
        vin_peak += m_scratch.vin.peak;
        vo_peak += m_scratch.vo.peak;
        a->blocks = (uint16_t)(i + 1U);
        a->h2 = m_scratch.h2_ratio;
        a->h3 = m_scratch.h3_ratio;
        m_ratio[i] = dsp_ratio(&m_scratch);

        a->ratio = dsp_ratio_mean_arr(m_ratio, a->blocks, &a->spread_lin, &a->spread_ph);
        /* 提前停止（§5.5）：K≥2 且幅度散差<0.1% 且相位散差<0.02° */
        if ((a->blocks >= 2U)
            && (a->spread_lin < CFG_SPREAD_EARLY)
            && (a->spread_ph < CFG_SPREAD_PH_EARLY)) {
            break;
        }
    }
    if (a->blocks == 0U) {
        return (a->adc_err != 0) ? a->adc_err : ADC_ERR_TIMEOUT;
    }
    const float inv = 1.0f / (float)a->blocks;
    a->vin_rms = vin_peak * inv * CFG_ADC_LSB_V * 0.70710678f;
    a->vo_rms = vo_peak * inv * CFG_ADC_LSB_V * 0.70710678f;
    if (a->sat > 0U) {
        m_diag |= DIAG_CLIP;
        ++m_trace.clip_count;
    }
    if ((a->quality & DSP_Q_SIG_LOW) != 0U) { m_diag |= DIAG_SIG_LOW; }
    if ((a->quality & DSP_Q_COHERENCE) != 0U) { m_diag |= DIAG_DDS_SILENT; }
    if (a->spread_lin > CFG_SPREAD_WARN) { m_diag |= DIAG_SPREAD_HIGH; }
    return 0;
}

/* ---- 硬件准备：设频 + 换档 + 静音解除 + ≥15 ms 稳定 --------------------- */
static int m_prepare(uint32_t f_hz, range_t r)
{
    if (board_clock_degraded != 0U) {
        return -3;
    }
    if (dds_set_freq_hz(f_hz) < 0) {
        return -2;
    }
    const int ms = relay_select_range(r);      /* 内部先静音，返回稳定耗时 */
    if (ms < 0) {
        return -1;
    }
    if (relay_verify() != 0) {
        m_diag |= DIAG_RELAY_ERR;
    }
    /* 切档后 ≥15 ms 才采（§6.1 S3）；relay 已花掉 ms，补足到 15 */
    if (ms < 15) {
        board_delay_ms((uint32_t)(15 - ms));
    }
    dds_hold_reset(0);
    board_delay_ms(CFG_RELAY_UNMUTE_MS);
    return 0;
}

/* 信号有效性：DDS 是否真的在输出、DUT 是否开路（§6.8）。 */
static meas_err_t m_check_signal(const m_acq_t *a)
{
    if (a->vin_rms < CFG_VIN_RMS_MIN) {
        m_diag |= DIAG_DDS_SILENT;
        return MEAS_ERR_SIGNAL;
    }
    if (a->vo_rms < CFG_VO_RMS_MIN) {
        return MEAS_ERR_OPEN;
    }
    if ((a->quality & DSP_Q_DC_WARN) != 0U) {
        return MEAS_ERR_SIGNAL;
    }
    return MEAS_OK;
}

/* 把一次采集结算成 meas_result_t（不含类型复核）。 */
static void m_settle(meas_result_t *res, uint32_t f_hz, range_t r, const m_acq_t *a)
{
    const cfloat_t zm = cal_raw_impedance(f_hz, r, a->ratio);
    cfloat_t zx = cf_make(0.0f, 0.0f);
    const int crc = cal_apply_impedance(f_hz, r, zm, &zx);
    m_diag |= cal_take_diag();
    if (crc < 0) {
        m_diag |= DIAG_CAL_STALE;
    }

    res->zm = zm;
    const uint32_t f_mhz = dds_get_freq_mhz();
    results_from_zx(res, zx, f_mhz);
    res->f_hz = f_hz;
    res->range = r;
    res->cal_level = (uint8_t)cal_get_level();
    res->spread = a->spread_lin;
    res->h2 = a->h2;
    res->h3 = a->h3;
    res->blocks = a->blocks;
    res->vin_rms = a->vin_rms;
    res->vo_rms = a->vo_rms;
    res->rho = m_rf[(uint32_t)r] / ((res->z_mag > 0.0f) ? res->z_mag : 1e-30f);
    res->retries = m_trace.retry_count;
    res->time_ms = m_elapsed();

    if (res->cal_level < 3U) {
        m_diag |= DIAG_CAL_LEVEL_LOW;
    }
    if (cal_is_stale() != 0) {
        m_diag |= DIAG_CAL_STALE;
    }
    if (board_stack_free() < CFG_STACK_NEAR_B) {
        m_diag |= DIAG_STACK_NEAR;
    }
    if (uart_rx_overflow() > 0U) {
        m_diag |= DIAG_RX_OVERFLOW;
    }
}

static void m_finish(meas_result_t *res, meas_err_t err)
{
    res->err = err;
    res->diag32 = m_diag;
    res->diag = (int16_t)(m_diag & 0xFFFFU);   /* 冻结字段：低 16 位 */
    res->time_ms = m_elapsed();
    m_trace.t_total_ms = res->time_ms;
    m_last = *res;
    m_busy = 0;
    m_abort_req = 0;
    adc_stop();
    dds_hold_reset(1);                          /* 测量结束回到静音安全态 */
    adc_awd_enable(0);
}

/* ---- 单点测量（M / SCAN 用，不自动换档）-------------------------------- */
meas_err_t meas_at(const uint32_t f_hz, const range_t r, uint16_t blocks, meas_result_t *res)
{
    if (res == 0) {
        return MEAS_ERR_RANGE;
    }
    if (m_busy != 0) {
        m_diag |= DIAG_BUSY;
        res->diag32 = m_diag;
        res->err = MEAS_ERR_TIMEOUT;
        return MEAS_ERR_TIMEOUT;
    }
    const int band = cal_band_of(f_hz);
    if ((band < 0) || ((uint32_t)r >= (uint32_t)RANGE_NUM)) {
        res->err = MEAS_ERR_RANGE;
        res->diag32 = m_diag;
        return MEAS_ERR_RANGE;
    }
    m_busy = 1;
    m_abort_req = 0;
    m_diag = 0U;
    m_t0 = board_millis();
    memset(&m_trace, 0, sizeof(m_trace));
    m_trace.t_start_ms = m_t0;
    memset(res, 0, sizeof(*res));

    if (m_prepare(f_hz, r) != 0) {
        m_finish(res, MEAS_ERR_SIGNAL);
        return res->err;
    }
    adc_awd_enable(r == (range_t)CFG_AWD_RANGE);
    m_step(f_hz, r, 0.0f);
    m_trace.state = 3U;

    m_acq_t a;
    const uint16_t k = (blocks == 0U) ? m_bands[band].k_default : blocks;
    const int rc = m_acquire((uint32_t)band, m_bands[band].n_fine, k, &a);
    if (rc != 0) {
        meas_err_t e = MEAS_ERR_TIMEOUT;
        if (rc == ADC_ERR_AWD) {
            e = (r == RANGE_10R) ? MEAS_ERR_HEADROOM : MEAS_ERR_SHORT;
        } else if (rc == ADC_ERR_CLIP) {
            e = MEAS_ERR_CLIP;
        }
        m_finish(res, e);
        return res->err;
    }
    const meas_err_t sig = m_check_signal(&a);
    if (sig != MEAS_OK) {
        m_finish(res, sig);
        return res->err;
    }
    m_settle(res, f_hz, r, &a);
    m_trace.state = 4U;
    m_trace.z_rough = res->z_mag;
    m_trace.phi_rough = res->z_phase_deg;
    m_trace.type_primary = (uint8_t)res->type;
    m_trace.type_final = (uint8_t)res->type;
    m_trace.blocks = a.blocks;
    res->confidence = results_confidence(res->type != ETYPE_UNKNOWN,
                                         res->cal_level, res->spread);
    meas_err_t err = MEAS_OK;
    if (res->type == ETYPE_UNKNOWN) {
        m_diag |= DIAG_TYPE_UNCERTAIN;
        err = MEAS_ERR_UNCERTAIN;
    }
    if (res->spread > CFG_SPREAD_FAIL) {
        err = MEAS_ERR_SIGNAL;
    }
    m_finish(res, err);
    return res->err;
}

/* ---- S5 双频斜率复核（§6.5）-------------------------------------------- */
static void m_recheck(meas_result_t *res)
{
    /* 1 kHz 点复用 S0 粗测；只需再测 10 kHz。时间不够就跳过并标注。 */
    const float z1k = m_trace.z_rough;
    if (!(z1k > 0.0f)) {
        return;
    }
    float z10k;
    if (res->f_hz == m_bands[CFG_BAND_10KHZ].f_nom) {
        z10k = res->z_mag;
    } else {
        if ((m_elapsed() + 60U) >= CFG_MEAS_TIMEOUT_MS) {
            m_diag |= DIAG_TIMEOUT;
            return;
        }
        const float zp = m_predict_z(res->type, z1k, CFG_BAND_10KHZ);
        int r = meas_pick_range(zp, CFG_BAND_10KHZ);
        if (r < 0) {
            r = (int)res->range;
        }
        if (m_prepare(m_bands[CFG_BAND_10KHZ].f_nom, (range_t)r) != 0) {
            return;
        }
        m_step(m_bands[CFG_BAND_10KHZ].f_nom, (range_t)r, 0.0f);
        m_acq_t a;
        if (m_acquire(CFG_BAND_10KHZ, m_bands[CFG_BAND_10KHZ].n_fine, 4U, &a) != 0) {
            return;
        }
        const cfloat_t zm = cal_raw_impedance(m_bands[CFG_BAND_10KHZ].f_nom, (range_t)r, a.ratio);
        cfloat_t zx = cf_make(0.0f, 0.0f);
        (void)cal_apply_impedance(m_bands[CFG_BAND_10KHZ].f_nom, (range_t)r, zm, &zx);
        m_diag |= cal_take_diag();
        z10k = cf_mag(zx);
        /* 复核完回到主测频率/档位，保证 res 与硬件状态一致 */
        (void)m_prepare(res->f_hz, res->range);
    }
    if (!(z10k > 0.0f)) {
        return;
    }
    const float slope = z10k / z1k;
    res->slope = slope;
    m_trace.slope = slope;
    const elem_type_t ts = results_type_of_slope(slope);
    m_trace.type_slope = (uint8_t)ts;
    const elem_type_t tp = (elem_type_t)m_trace.type_primary;

    if ((ts != ETYPE_UNKNOWN) && (tp != ETYPE_UNKNOWN) && (ts != tp)) {
        /* 冲突 → 取斜率结论，confidence 降为 1（§6.5） */
        results_set_type(res, ts, dds_get_freq_mhz());
        res->confidence = 1U;
        m_diag |= DIAG_TYPE_UNCERTAIN;
        res->err = MEAS_ERR_UNCERTAIN;
    } else if (tp == ETYPE_UNKNOWN) {
        if (ts != ETYPE_UNKNOWN) {
            results_set_type(res, ts, dds_get_freq_mhz());
            res->confidence = 1U;
        }
        m_diag |= DIAG_TYPE_UNCERTAIN;
        res->err = MEAS_ERR_UNCERTAIN;
    }
    m_trace.type_final = (uint8_t)res->type;
}

/* ---- 一键自动测量（§6.1 S0–S5）----------------------------------------- */
meas_err_t meas_auto(meas_result_t *res)
{
    if (res == 0) {
        return MEAS_ERR_RANGE;
    }
    if (m_busy != 0) {
        /* §8.5：测量中收到的 A/M 回 BUSY，不排队重测 */
        m_diag |= DIAG_BUSY;
        res->diag32 = m_diag;
        res->err = MEAS_ERR_TIMEOUT;
        return MEAS_ERR_TIMEOUT;
    }
    m_busy = 1;
    m_abort_req = 0;
    m_diag = 0U;
    m_t0 = board_millis();
    memset(&m_trace, 0, sizeof(m_trace));
    m_trace.t_start_ms = m_t0;
    memset(res, 0, sizeof(*res));

    /* ---- S0 粗测：1 kHz / 10 kΩ（NC 默认档）/ N=64 / K=1（4 ms）---- */
    m_trace.state = 0U;
    const uint32_t f0 = m_bands[CFG_BAND_1KHZ].f_nom;
    if (m_prepare(f0, RANGE_10K) != 0) {
        m_finish(res, MEAS_ERR_SIGNAL);
        return res->err;
    }
    m_step(f0, RANGE_10K, 0.0f);
    m_acq_t coarse;
    int rc = m_acquire(CFG_BAND_1KHZ, m_bands[CFG_BAND_1KHZ].n_coarse, 1U, &coarse);
    if (rc != 0) {
        meas_err_t e = MEAS_ERR_TIMEOUT;
        if (rc == ADC_ERR_AWD) {
            e = MEAS_ERR_SHORT;
        } else if (rc == ADC_ERR_CLIP) {
            e = MEAS_ERR_SHORT;      /* 默认 10 kΩ 档就削顶 → 疑似短路 */
        }
        m_finish(res, e);
        return res->err;
    }
    const meas_err_t sig = m_check_signal(&coarse);
    if (sig != MEAS_OK) {
        m_finish(res, sig);
        return res->err;
    }
    m_settle(res, f0, RANGE_10K, &coarse);
    const float z_rough = res->z_mag;
    const float phi_rough = res->z_phase_deg;
    m_trace.z_rough = z_rough;
    m_trace.phi_rough = phi_rough;

    /* ---- S1 类型/量级初判 ---- */
    m_trace.state = 1U;
    elem_type_t type = results_type_of(phi_rough);
    m_trace.type_primary = (uint8_t)type;
    if (type == ETYPE_UNKNOWN) {
        type = ETYPE_R;                /* 未知先按 R 处理，S5 再复核 */
    }

    /* ---- S2 选频 + 选档 ---- */
    m_trace.state = 2U;
    int band = meas_pick_band(type, z_rough);
    if (band < 0) {
        band = (int)CFG_BAND_1KHZ;
    }
    float z_pred = m_predict_z(type, z_rough, (uint32_t)band);
    int r = meas_pick_range(z_pred, (uint32_t)band);
    if (r < 0) {
        /* 无合法组合：逐档扫描兜底（§6.4） */
        for (uint32_t rr = 0U; rr < CFG_RANGE_NUM; ++rr) {
            if (m_legal[rr][band] != 0U) {
                r = (int)rr;
                break;
            }
        }
        if (r < 0) {
            m_finish(res, MEAS_ERR_RANGE);
            return res->err;
        }
    }

    /* ---- S3 精测：K 块，削顶/越界 → 降档重试 ≤3 ---- */
    m_trace.state = 3U;
    m_acq_t fine;
    meas_err_t err = MEAS_OK;
    int settled = 0;
    for (uint32_t retry = 0U; retry <= CFG_RANGE_RETRY_MAX; ++retry) {
        if (m_aborted() != 0) {
            err = MEAS_ERR_TIMEOUT;
            break;
        }
        if (m_prepare(m_bands[band].f_nom, (range_t)r) != 0) {
            err = MEAS_ERR_SIGNAL;
            break;
        }
        adc_awd_enable((uint32_t)r == CFG_AWD_RANGE);
        m_step(m_bands[band].f_nom, (range_t)r, m_rf[r] / z_pred);
        rc = m_acquire((uint32_t)band, m_bands[band].n_fine, m_bands[band].k_default, &fine);
        if (rc != 0) {
            ++m_trace.retry_count;
            if (rc == ADC_ERR_AWD) {
                /* AWD 越界：先静音再降档；已到最小档 → HEADROOM，不静默出数 */
                dds_hold_reset(1);
                adc_stop();
                if (r == (int)RANGE_10R) {
                    err = MEAS_ERR_HEADROOM;
                    break;
                }
                --r;
                continue;
            }
            if (rc == ADC_ERR_CLIP) {
                dds_hold_reset(1);
                if (r == (int)RANGE_10R) {
                    err = MEAS_ERR_HEADROOM;
                    break;
                }
                --r;
                continue;
            }
            err = MEAS_ERR_TIMEOUT;
            break;
        }
        const meas_err_t s2 = m_check_signal(&fine);
        if (s2 == MEAS_ERR_OPEN) {
            err = MEAS_ERR_OPEN;
            break;
        }
        if (s2 != MEAS_OK) {
            err = s2;
            break;
        }
        m_settle(res, m_bands[band].f_nom, (range_t)r, &fine);
        const float rho = res->rho;
        if (rho > m_rho_max[r]) {
            ++m_trace.retry_count;
            if (r == (int)RANGE_10R) {
                err = MEAS_ERR_HEADROOM;
                break;
            }
            --r;
            z_pred = res->z_mag;
            continue;
        }
        if (rho < CFG_RHO_MIN) {
            ++m_trace.retry_count;
            if (r == (int)RANGE_1M) {
                err = MEAS_ERR_RANGE;
                break;
            }
            ++r;
            z_pred = res->z_mag;
            continue;
        }
        settled = 1;
        break;
    }

    if (settled == 0) {
        if (err == MEAS_OK) {
            err = MEAS_ERR_RANGE;      /* 3 次仍不满足窗口 */
        }
        /* 保留最接近 ρ=0.3 的一次测量并附诊断码（§6.4） */
        if (res->blocks > 0U) {
            res->confidence = 0U;
            m_finish(res, err);
            return res->err;
        }
        m_finish(res, err);
        return res->err;
    }

    /* ---- S4 结算：类型确认 + 置信度 ---- */
    m_trace.state = 4U;
    m_trace.type_primary = (uint8_t)res->type;
    m_trace.blocks = fine.blocks;
    const int clear = (res->type != ETYPE_UNKNOWN) ? 1 : 0;
    res->confidence = results_confidence(clear, res->cal_level, res->spread);
    err = MEAS_OK;
    if (res->spread > CFG_SPREAD_FAIL) {
        err = MEAS_ERR_SIGNAL;
    } else if ((fine.quality & DSP_Q_H2_WARN) != 0U) {
        m_diag |= DIAG_SIG_LOW;
    }

    /* ---- S5 置信度低/类型边界 → 双频斜率复核 ---- */
    if ((clear == 0) || (res->confidence < 2U)) {
        m_trace.state = 5U;
        m_recheck(res);
        if (res->err == MEAS_ERR_UNCERTAIN) {
            err = MEAS_ERR_UNCERTAIN;
        }
    }
    m_trace.type_final = (uint8_t)res->type;
    if (res->cal_level < 2U) {
        /* 允许"临时未校准"结果，但必须标注（§6.8 MEAS_ERR_CAL） */
        if (err == MEAS_OK) {
            err = MEAS_ERR_CAL;
        }
    }
    m_finish(res, err);
    return res->err;
}

/* ---- 扫频（SCAN）------------------------------------------------------- */
meas_err_t meas_sweep(const uint32_t *f_list, uint8_t nf, meas_result_t *out, uint8_t nmax)
{
    if ((f_list == 0) || (out == 0) || (nf == 0U) || (nmax == 0U)) {
        return MEAS_ERR_RANGE;
    }
    meas_err_t first_err = MEAS_OK;
    uint8_t n = 0U;
    float z_ref = 0.0f;
    for (uint8_t i = 0U; (i < nf) && (n < nmax); ++i) {
        const int band = cal_band_of(f_list[i]);
        if (band < 0) {
            continue;
        }
        int r;
        if (z_ref > 0.0f) {
            r = meas_pick_range(z_ref, (uint32_t)band);
        } else {
            r = (int)RANGE_10K;
        }
        if (r < 0) {
            r = (int)RANGE_10K;
        }
        meas_result_t tmp;
        const meas_err_t e = meas_at(f_list[i], (range_t)r, 0U, &tmp);
        if ((e != MEAS_OK) && (first_err == MEAS_OK)) {
            first_err = e;
        }
        if (tmp.z_mag > 0.0f) {
            z_ref = tmp.z_mag;
        }
        out[n++] = tmp;
    }
    return first_err;
}

/* ---- 初始化 / 访问器 ---------------------------------------------------- */
void meas_init(void)
{
    m_busy = 0;
    m_abort_req = 0;
    m_diag = 0U;
    memset(&m_last, 0, sizeof(m_last));
    memset(&m_trace, 0, sizeof(m_trace));
    m_manual_f = m_bands[CFG_BAND_1KHZ].f_nom;
    m_manual_r = RANGE_10K;
}

const meas_trace_t *meas_trace(void) { return &m_trace; }
const meas_result_t *meas_last(void) { return &m_last; }

void meas_set_manual(uint32_t f_hz, range_t r)
{
    if (cal_band_of(f_hz) >= 0) {
        m_manual_f = f_hz;
    }
    if ((uint32_t)r < (uint32_t)RANGE_NUM) {
        m_manual_r = r;
    }
}

uint32_t meas_manual_freq(void) { return m_manual_f; }
range_t  meas_manual_range(void) { return m_manual_r; }