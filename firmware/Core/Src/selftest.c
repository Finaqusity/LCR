/* 板上自检（§10.3）。上电跑快速子集，`I` 命令跑全量。
 * 失败位用 error.h 的 ST_*；看门狗是信息位，OLED 失败只降级。
 * 所有采集都走 adc/dsp 正常通路，自检本身也就验证了采集链。 */

#include "selftest.h"
#include "error.h"
#include "board.h"
#include "board_config.h"
#include "dds.h"
#include "relay.h"
#include "adc.h"
#include "dsp.h"
#include "cal.h"
#include "i2c_oled.h"
#include "uart.h"
#include "log.h"

#include <math.h>
#include <string.h>

static uint32_t st_status;
static const cfg_band_t st_bands[CFG_FREQ_NUM] = CFG_BANDS_INITIALIZER;
static const float st_rf[CFG_RANGE_NUM] = CFG_RF_NOM_INITIALIZER;

/* ---- 时钟：DWT 计 1 ms 与 SysTick 比较（±2%）--------------------------- */
static int st_clock(void)
{
    const uint32_t t0 = board_millis();
    const uint32_t c0 = board_cycles();
    while ((board_millis() - t0) < 1U) { /* 等一个 SysTick */ }
    const uint32_t dc = board_cycles() - c0;
    const uint32_t expect = board_sysclk_hz() / 1000U;
    /* ±2% */
    return (dc > expect - expect / 50U && dc < expect + expect / 50U) ? 0 : -1;
}

/* ---- DFT 一致性：合成已知正弦，比对峰值/相位（不依赖外部向量文件）------ */
static int st_dft(void)
{
    static adc_block_t blk;   /* 4 KB 放 .bss，不上栈 */
    memset(&blk, 0, sizeof(blk));
    const uint16_t n = 1024U;
    const uint16_t s = 16U;
    const uint16_t m = (uint16_t)(n / s);      /* 基波在第 M 号 bin */
    blk.n = n;
    blk.s = s;
    blk.f0_hz = 1000U;
    blk.fs_hz = 16000U;
    /* Vin = 2048 + 1000·cos(2π·M·k/N)，Vo = 2048 + 500·cos(2π·M·k/N − 60°) */
    for (uint16_t k = 0U; k < n; ++k) {
        const double ang = 2.0 * 3.14159265358979323846 * (double)m * (double)k / (double)n;
        const int32_t v1 = 2048 + (int32_t)(1000.0 * cos(ang));
        const int32_t v2 = 2048 + (int32_t)(500.0 * cos(ang - 3.14159265358979323846 / 3.0));
        blk.cdr[k] = ((uint32_t)v2 << 16U) | (uint32_t)v1;
    }
    dsp_result_t r;
    dsp_analyze(&blk, &r);
    /* 峰值应 = 1000 / 500（±0.1%），相位差应 = +60°（±0.05°） */
    if (fabsf(r.vin.peak - 1000.0f) > 1.0f) { return -1; }
    if (fabsf(r.vo.peak - 500.0f) > 0.5f) { return -1; }
    const cfloat_t ratio = dsp_ratio(&r);
    const float ph = cf_arg_deg(ratio);
    if (fabsf(ph - 60.0f) > 0.05f) { return -1; }
    if (fabsf(cf_mag(ratio) - 2.0f) > 0.002f) { return -1; }
    return 0;
}

/* ---- ADC 噪声/DC：CAL 通路采 1024 点，DC 2048±80、σ<1.5 LSB ------------ */
static int st_adc(void)
{
    if (dds_set_freq_hz(1000U) < 0) { return -1; }
    if (relay_select_range(RANGE_10K) < 0) { return -1; }
    relay_cal_enable(1);
    dds_hold_reset(0);
    board_delay_ms(CFG_CAL_SETTLE_MS);
    acq_cfg_t cfg;
    if (adc_make_cfg(CFG_BAND_1KHZ, 1024U, &cfg) != 0) {
        relay_cal_enable(0);
        dds_hold_reset(1);
        return -1;
    }
    static adc_block_t blk;   /* 4 KB 放 .bss，不上栈 */
    const int rc = adc_acquire(&cfg, &blk);
    relay_cal_enable(0);
    dds_hold_reset(1);
    if (rc != ADC_OK) { return -1; }

    /* CAL 时两路同信号，含 1 kHz 正弦；DC 与 σ 看基波以外的残差。
     * 这里直接统计码值均值与标准差（正弦会让 σ 变大，故用去基波后的残差）。 */
    dsp_result_t r;
    dsp_analyze(&blk, &r);
    if (fabsf(r.vin.dc - 2048.0f) > (float)CFG_ADC_DC_TOL) { return -1; }
    if (fabsf(r.vo.dc - 2048.0f) > (float)CFG_ADC_DC_TOL) { return -1; }
    /* 残差 σ：逐点减去 DC 与基波重构 */
    const uint16_t s = blk.s;
    const uint16_t m = (uint16_t)(blk.n / s);
    double acc = 0.0;
    for (uint16_t k = 0U; k < blk.n; ++k) {
        const double ang = 2.0 * 3.14159265358979323846 * (double)m * (double)k / (double)blk.n;
        const double fit = (double)r.vin.dc
                         + (double)r.vin.h[1].re * cos(ang)
                         - (double)r.vin.h[1].im * sin(ang);
        const double d = (double)(blk.cdr[k] & 0xFFFFU) - fit;
        acc += d * d;
    }
    const float sigma = (float)sqrt(acc / (double)blk.n);
    return (sigma < CFG_ADC_NOISE_MAX) ? 0 : -1;
}

/* ---- DDS 四频点：N=64，|Vin| 0.3–0.7 Vrms，相干偏差 <0.1% -------------- */
static int st_dds(void)
{
    for (uint32_t b = 0U; b < CFG_FREQ_NUM; ++b) {
        if (dds_set_freq_hz(st_bands[b].f_nom) < 0) { return -1; }
        if (relay_select_range(RANGE_10K) < 0) { return -1; }
        dds_hold_reset(0);
        board_delay_ms(CFG_RELAY_UNMUTE_MS + 2U);
        acq_cfg_t cfg;
        if (adc_make_cfg(b, st_bands[b].n_coarse, &cfg) != 0) { return -1; }
        static adc_block_t blk;   /* 4 KB 放 .bss，不上栈 */
        const int rc = adc_acquire(&cfg, &blk);
        dds_hold_reset(1);
        if (rc != ADC_OK) { return -1; }
        dsp_result_t r;
        dsp_analyze(&blk, &r);
        const float vrms = r.vin.peak * CFG_ADC_LSB_V * 0.70710678f;
        if ((vrms < CFG_VIN_RMS_MIN) || (vrms > CFG_VIN_RMS_MAX)) { return -1; }
        /* 频率偏差：实际 f_dds 与标称差 <0.1%，且采样相干 |M_act−M|<0.02 bin */
        const uint32_t f_mhz = dds_get_freq_mhz();
        const float dev = fabsf((float)f_mhz / 1000.0f - (float)st_bands[b].f_nom)
                        / (float)st_bands[b].f_nom;
        if (dev > 0.001f) { return -1; }
        if (dsp_coherence_err(blk.n, blk.s, blk.fs_hz, f_mhz) > CFG_COHERENCE_TOL) {
            return -1;
        }
    }
    return 0;
}

/* ---- α 一致性：|α−1|<2%、|arg α|<1° ----------------------------------- */
static int st_alpha(void)
{
    cal_alpha_t a;
    const int rc = cal_measure_alpha(1000U, &a);
    if (rc != 0) { return -1; }
    if (fabsf(cf_mag(a.alpha) - 1.0f) > CFG_ALPHA_GAIN_TOL) { return -1; }
    if (fabsf(cf_arg_deg(a.alpha)) > CFG_ALPHA_PH_TOL) { return -1; }
    return 0;
}

/* ---- 继电器逐档：ODR 回读 + 开路阻抗落在该档预期窗口 ------------------- */
static int st_relay(void)
{
    for (uint32_t r = 0U; r < CFG_RANGE_NUM; ++r) {
        if (relay_select_range((range_t)r) < 0) { return -1; }
        if (relay_verify() != 0) { return -1; }
        if (dds_set_freq_hz(1000U) < 0) { return -1; }
        dds_hold_reset(0);
        board_delay_ms(CFG_RELAY_UNMUTE_MS + 2U);
        acq_cfg_t cfg;
        if (adc_make_cfg(CFG_BAND_1KHZ, 64U, &cfg) != 0) { return -1; }
        static adc_block_t blk;   /* 4 KB 放 .bss，不上栈 */
        const int rc = adc_acquire(&cfg, &blk);
        dds_hold_reset(1);
        if (rc != ADC_OK) { return -1; }
        dsp_result_t res;
        dsp_analyze(&blk, &res);
        const cfloat_t ratio = dsp_ratio(&res);
        const cfloat_t zm = cal_raw_impedance(1000U, (range_t)r, ratio);
        const float rho = cf_mag(zm) / st_rf[r];
        /* 开路时 ρ 应落在 [1,100]（§7.3 有效性窗口） */
        if ((rho < CFG_OPEN_RHO_LO) || (rho > CFG_OPEN_RHO_HI)) { return -1; }
    }
    (void)relay_select_range(RANGE_10K);
    return 0;
}

/* ---- 校准记录 + 开机开路残差（10 kHz/10 kΩ，~16 ms）-------------------- */
static int st_cal(void)
{
    const cal_record_t *rec = cal_record();
    if (rec->magic != CFG_CAL_MAGIC) { return -1; }
    if (rec->version != CFG_CAL_BLOB_VERSION) { return -1; }
    /* 夹具不变性：重新测开路，ΔC>0.2 pF → stale（§7.3） */
    cal_net_t n;
    const int rc = cal_measure_open(10000U, RANGE_10K, &n);
    if (rc < 0) { return -1; }
    if (cal_is_stale() != 0) { return -1; }
    return 0;
}

/* ---- 汇总 --------------------------------------------------------------- */
int selftest_run(int verbose)
{
    uint32_t fail = 0U;
    uint32_t info = 0U;

    if (st_clock() != 0) { fail |= ST_CLOCK; }
    if (board_clock_degraded != 0U) { fail |= ST_HSE; }
    if (board_stack_free() < CFG_STACK_NEAR_B) { fail |= ST_STACK; }
    if (st_dft() != 0) { fail |= ST_DFT; }

    /* Flash/校准记录：cal_init 已读并校验 CRC；这里看等级与 stale */
    if (cal_get_level() == 0) { fail |= ST_CAL; }
    if (cal_flash_bank() < 0) { fail |= ST_FLASH; }

    /* 看门狗复位原因：信息位，不算失败 */
    const uint32_t cause = board_reset_cause();
    if ((cause & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF)) != 0U) { info |= ST_WDG; }

    /* OLED：降级，不阻塞测量 */
    if (i2c_oled_present() == 0) { info |= ST_OLED; }

    if (verbose != 0) {
        if (st_dds() != 0) { fail |= ST_DDS; }
        if (st_adc() != 0) { fail |= ST_ADC; }
        if (st_alpha() != 0) { fail |= ST_ALPHA; }
        if (st_relay() != 0) { fail |= ST_RELAY; }
        if (st_cal() != 0) { fail |= ST_CAL; }
    }

    st_status = fail | info;
    log_event(LOG_SELFTEST, st_status, (uint32_t)verbose);
    /* 测量结束后回到安全态 */
    dds_hold_reset(1);
    adc_stop();
    (void)relay_select_range(RANGE_10K);
    return (int)fail;
}

uint32_t selftest_status(void) { return st_status; }

void selftest_print(int verbose)
{
    const uint32_t s = st_status;
    uart_printf("# SELFTEST %s bitmap=%08lX\r\n",
                (s & ~(ST_WDG | ST_OLED)) == 0U ? "PASS" : "FAIL",
                (unsigned long)s);
    static const struct { uint32_t bit; const char *name; } items[] = {
        { ST_CLOCK, "CLOCK" }, { ST_HSE, "HSE" }, { ST_DDS, "DDS" },
        { ST_ADC, "ADC" }, { ST_ALPHA, "ALPHA" }, { ST_RELAY, "RELAY" },
        { ST_CAL, "CAL" }, { ST_STACK, "STACK" }, { ST_WDG, "WDG(info)" },
        { ST_OLED, "OLED(degraded)" }, { ST_FLASH, "FLASH" }, { ST_DFT, "DFT" }
    };
    for (uint32_t i = 0U; i < sizeof(items) / sizeof(items[0]); ++i) {
        const int is_info = (items[i].bit == ST_WDG) || (items[i].bit == ST_OLED);
        const int hit = (s & items[i].bit) != 0U;
        if (is_info) {
            uart_printf("#   %-14s %s\r\n", items[i].name, hit ? "SET" : "clear");
        } else {
            uart_printf("#   %-14s %s\r\n", items[i].name, hit ? "FAIL" : "PASS");
        }
    }
    uart_printf("#   stack_free=%lu B  clk=%lu Hz  cal_level=%d\r\n",
                (unsigned long)board_stack_free(),
                (unsigned long)board_sysclk_hz(), cal_get_level());
    (void)verbose;
}