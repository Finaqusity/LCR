/* L4 串口协议（§8.5、§13.4）。ASCII 行命令 + RAW 二进制快照 + SCAN 扫频。
 * 纪律：
 *  - 浮点一律经 results_format_* 转整数字符串再 %s 输出，绝不给 uart_printf
 *    传 float（-Wdouble-promotion / nano printf 无 %f）。
 *  - 测量期间 RX 只入环；本模块只在主循环空闲时被 proto_poll 调用。
 *  - 4 KB 的 adc_block_t 用 static，不上 4 KB 栈。 */

#include "proto.h"
#include "board.h"
#include "board_config.h"
#include "uart.h"
#include "dds.h"
#include "relay.h"
#include "adc.h"
#include "dsp.h"
#include "cal.h"
#include "meas.h"
#include "results.h"
#include "selftest.h"
#include "log.h"
#include "error.h"
#include "i2c_oled.h"

#include <string.h>
#include <stdlib.h>

/* ---- 状态 ---------------------------------------------------------------- */
static proto_result_cb s_cb;
static char     s_line[CFG_UART_LINE_MAX + 1U];
static uint16_t s_len;
static int      s_ovf;            /* 本行超长 → 终止符处整行丢弃 */
static int      s_busy_echo;      /* 自动测量后，缓冲进来的 A/M 回 BUSY */
static uint32_t s_raw_seq;

/* RAW 流式发送的小 staging（256 B），避免 4 KB 整帧缓冲。 */
static uint8_t  s_rtx[256];
static uint16_t s_rlen;
static uint16_t s_rcrc;

static const cfg_band_t s_bands[CFG_FREQ_NUM] = CFG_BANDS_INITIALIZER;

/* ---- 发送helpers --------------------------------------------------------- */
static void tx_wait(void)
{
    while (uart_tx_idle() == 0) {
        board_iwdg_feed();
    }
}

/* 长文本（横幅/帮助/CSV）：入环后等发完，避免 512 B TX 环溢出丢字。 */
static void puts_slow(const char *s)
{
    uart_write(s);
    tx_wait();
}

/* ---- 行解析helpers ------------------------------------------------------- */
static void str_upper(char *s)
{
    for (; *s != '\0'; ++s) {
        if ((*s >= 'a') && (*s <= 'z')) { *s = (char)(*s - 'a' + 'A'); }
    }
}

/* 取下一个空格分隔 token（就地改写 line）；无 token 返回 NULL。 */
static char *next_tok(char **p)
{
    while ((**p == ' ') || (**p == '\t')) { ++(*p); }
    if (**p == '\0') { return 0; }
    char *start = *p;
    while ((**p != '\0') && (**p != ' ') && (**p != '\t')) { ++(*p); }
    if (**p != '\0') { **p = '\0'; ++(*p); }
    return start;
}

/* 无符号十进制整词解析；成功返回 1 并写 *out。 */
static int parse_u32(const char *t, uint32_t *out)
{
    if ((t == 0) || (*t == '\0')) { return 0; }
    char *end = 0;
    const unsigned long v = strtoul(t, &end, 10);
    if ((end == t) || (*end != '\0')) { return 0; }
    *out = (uint32_t)v;
    return 1;
}

static int freq_valid(uint32_t f)
{
    return (f == 100U) || (f == 1000U) || (f == 10000U) || (f == 100000U);
}

/* ---- CRC16-CCITT（poly=0x1021 init=0xFFFF refin/refout=false）------------- */
static uint16_t crc16_byte(uint16_t crc, uint8_t b)
{
    crc ^= (uint16_t)((uint16_t)b << 8);
    for (int i = 0; i < 8; ++i) {
        crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U)
                              : (uint16_t)(crc << 1);
    }
    return crc;
}

/* ---- RAW 流式字节 -------------------------------------------------------- */
static void raw_flush(void)
{
    if (s_rlen == 0U) { return; }
    uart_write_bytes(s_rtx, s_rlen);
    s_rlen = 0U;
    tx_wait();
}

static void raw_put(uint8_t b, int covered)
{
    if (covered != 0) { s_rcrc = crc16_byte(s_rcrc, b); }
    s_rtx[s_rlen++] = b;
    if (s_rlen >= (uint16_t)sizeof(s_rtx)) { raw_flush(); }
}

static void raw_u16(uint16_t v)
{
    raw_put((uint8_t)(v & 0xFFU), 1);
    raw_put((uint8_t)(v >> 8), 1);
}

static void raw_u32(uint32_t v)
{
    raw_put((uint8_t)(v & 0xFFU), 1);
    raw_put((uint8_t)((v >> 8) & 0xFFU), 1);
    raw_put((uint8_t)((v >> 16) & 0xFFU), 1);
    raw_put((uint8_t)((v >> 24) & 0xFFU), 1);
}

/* ---- 测量执行 ------------------------------------------------------------ */
static void run_meas(int automatic)
{
    meas_result_t res;
    if (automatic != 0) {
        (void)meas_auto(&res);
    } else {
        (void)meas_at(meas_manual_freq(), meas_manual_range(), 0U, &res);
    }
    char buf[192];
    const int n = results_format_line(&res, res.time_ms, buf, (uint32_t)sizeof(buf));
    if (n > 0) {
        uart_write(buf);
        uart_write("\r\n");
    } else {
        uart_printf("ERR %d %s\r\n", meas_err_code(res.err), meas_err_name(res.err));
    }
    log_event(LOG_MEAS, (uint32_t)meas_err_code(res.err), res.diag32);
    if (s_cb != 0) { s_cb(&res); }
}

/* ---- RAW 快照 ------------------------------------------------------------ */
static void do_raw(uint16_t blocks)
{
    const uint32_t f = meas_manual_freq();
    const range_t r = meas_manual_range();
    int band = cal_band_of(f);
    if (band < 0) { band = (int)CFG_BAND_1KHZ; }

    (void)dds_set_freq_hz(f);
    (void)relay_select_range(r);
    board_delay_ms(CFG_RELAY_SETTLE_MS + CFG_RELAY_POST_MS);
    dds_hold_reset(0);
    board_delay_ms(CFG_RELAY_UNMUTE_MS);

    acq_cfg_t cfg;
    if (adc_make_cfg((uint32_t)band, 64U, &cfg) != 0) {
        uart_write("ERR 4 RANGE\r\n");
        dds_hold_reset(1);
        return;
    }
    static adc_block_t blk;   /* 4 KB 放 .bss */
    adc_clear_errors();
    int rc = ADC_OK;
    for (uint16_t i = 0U; i < blocks; ++i) {
        rc = adc_acquire(&cfg, &blk);
        if (rc != ADC_OK) { break; }
    }
    adc_stop();
    dds_hold_reset(1);
    if (rc != ADC_OK) {
        uart_printf("ERR 3 CLIP\r\n");
        return;
    }

    uart_printf("OK RAW N=%u\r\n", (unsigned int)blk.n);

    uint8_t flags = 0U;
    if (blk.sat_count > 0U) { flags |= 0x01U; }
    if (blk.dma_err != 0U)  { flags |= 0x02U; }
    if (adc_awd_tripped() != 0) { flags |= 0x04U; }

    s_rcrc = 0xFFFFU;
    s_rlen = 0U;
    raw_put(CFG_RAW_SYNC0, 0);          /* A5，不计 CRC */
    raw_put(CFG_RAW_SYNC1, 0);          /* 5A */
    raw_put((uint8_t)CFG_PROTO_VERSION, 1);   /* ver=1 */
    raw_put(flags, 1);
    raw_u16((uint16_t)(4U * blk.n));    /* len = data 字节数 */
    raw_u32(++s_raw_seq);
    raw_u32(dds_get_freq_mhz());        /* 实际 f_dds（mHz） */
    raw_u32(blk.fs_hz);
    raw_put((uint8_t)blk.s, 1);
    raw_u16(blk.n);
    for (uint16_t i = 0U; i < blk.n; ++i) {
        raw_u32(blk.cdr[i]);            /* CDR：低16=Vin 高16=Vo */
    }
    raw_flush();
    raw_put((uint8_t)(s_rcrc & 0xFFU), 0);   /* crc16 本身不计 */
    raw_put((uint8_t)(s_rcrc >> 8), 0);
    raw_flush();
}

/* ---- SCAN 扫频（发挥/调试，粗测 N=4S，CSV 输出）-------------------------- */
static void do_scan(uint32_t f1, uint32_t f2, uint32_t npts)
{
    if (npts == 0U) { npts = 1U; }
    if (npts > 64U) { npts = 64U; }
    if (f1 < 10U) { f1 = 10U; }
    if (f2 > 116000U) { f2 = 116000U; }   /* S=12 → fs≤1.392 MSPS */
    if (f2 < f1) { const uint32_t t = f1; f1 = f2; f2 = t; }

    const range_t r = meas_manual_range();
    (void)relay_select_range(r);
    puts_slow("# f_mHz,|Z|,phi_deg,type,val,unit,D,Q\r\n");

    static adc_block_t blk;   /* 4 KB 放 .bss */
    static dsp_result_t sp;
    const cal_record_t *rec = cal_record();

    for (uint32_t i = 0U; i < npts; ++i) {
        const uint32_t f = (npts == 1U) ? f1
                         : (uint32_t)((uint64_t)f1 + ((uint64_t)(f2 - f1) * i) / (npts - 1U));
        (void)dds_set_freq_hz(f);
        board_delay_ms(CFG_RELAY_UNMUTE_MS + 2U);
        dds_hold_reset(0);

        /* 相干采样：fs = S·f，N = 4S（粗测）。f>80 kHz 用 S=12 守 ADC 上限。 */
        const uint16_t s = (f <= 80000U) ? 16U : 12U;
        acq_cfg_t cfg;
        cfg.f0_hz = f;
        cfg.s = s;
        cfg.n = (uint16_t)(4U * s);
        cfg.fs_hz = (uint32_t)s * f;
        adc_clear_errors();
        const int rc = adc_acquire(&cfg, &blk);
        adc_stop();
        if (rc != ADC_OK) {
            dds_hold_reset(1);
            puts_slow("# point acquire fail\r\n");
            continue;
        }
        dsp_analyze(&blk, &sp);
        const cfloat_t ratio = dsp_ratio(&sp);

        /* 名义 Zf_eff(f) + 最近格点 α；格点上再走完整校准反解。 */
        int near = 0;
        uint32_t best = 0xFFFFFFFFU;
        for (int b = 0; b < (int)CFG_FREQ_NUM; ++b) {
            const uint32_t d = (f > s_bands[b].f_nom) ? (f - s_bands[b].f_nom)
                                                      : (s_bands[b].f_nom - f);
            if (d < best) { best = d; near = b; }
        }
        cfloat_t alpha = rec->alpha[near].alpha;
        if ((alpha.re == 0.0f) && (alpha.im == 0.0f)) { alpha = cf_make(1.0f, 0.0f); }
        const cfloat_t zf = cal_zf_eff(f, r);
        cfloat_t zm = cf_neg(cf_div(cf_mul(zf, ratio), alpha));
        cfloat_t zx = zm;
        if (freq_valid(f)) {
            (void)cal_apply_impedance(f, r, zm, &zx);
        }
        meas_result_t res;
        results_from_zx(&res, zx, dds_get_freq_mhz());
        dds_hold_reset(1);

        char zb[24], vb[24], pb[16], db[16], qb[16];
        results_format_sig(res.z_mag, zb, (uint32_t)sizeof(zb));
        results_format_sig(res.value, vb, (uint32_t)sizeof(vb));
        results_format_fixed(res.z_phase_deg, 1U, pb, (uint32_t)sizeof(pb));
        results_format_fixed(res.d, 3U, db, (uint32_t)sizeof(db));
        results_format_fixed(res.q, 3U, qb, (uint32_t)sizeof(qb));
        uart_printf("%lu,%s,%s,%c,%s,%s,%s,%s\r\n",
                    (unsigned long)dds_get_freq_mhz(), zb, pb,
                    (char)res.type, vb, results_unit(res.type), db, qb);
        tx_wait();
        board_iwdg_feed();
    }
    puts_slow("# SCAN done\r\n");
}

/* ---- CAL 命令 ------------------------------------------------------------ */
static void do_cal(char *sub)
{
    if (sub == 0) { uart_write("# usage: CAL A|O|L|S\r\n"); return; }
    str_upper(sub);
    if (strcmp(sub, "S") == 0) {
        uart_write("ERR 9 CAL_S_UNSUPPORTED\r\n");
        puts_slow("# 0.5 Vrms 短路 → 10 Ω 档约 5 A，烧 Rf/BUF634A；改用双负载两点法 CAL L\r\n");
        log_event(LOG_ERR, 9U, 0U);
        return;
    }
    if (strcmp(sub, "A") == 0) {
        int ok = 1;
        for (int b = 0; b < (int)CFG_FREQ_NUM; ++b) {
            cal_alpha_t a;
            if (cal_measure_alpha(s_bands[b].f_nom, &a) != 0) { ok = 0; }
        }
        if (ok != 0) {
            uart_printf("OK CAL_A level=%d\r\n", cal_get_level());
            log_event(LOG_CAL, 1U, (uint32_t)cal_get_level());
        } else {
            uart_write("ERR 5 CAL\r\n");
        }
        return;
    }
    if (strcmp(sub, "O") == 0) {
        const range_t r = meas_manual_range();
        int ok = 1;
        for (int b = 0; b < (int)CFG_FREQ_NUM; ++b) {
            cal_net_t net;
            if (cal_range_legal(r, s_bands[b].f_nom) == 0) { continue; }
            if (cal_measure_open(s_bands[b].f_nom, r, &net) != 0) { ok = 0; }
        }
        if (ok != 0) {
            uart_printf("OK CAL_O level=%d\r\n", cal_get_level());
            log_event(LOG_CAL, 2U, (uint32_t)cal_get_level());
        } else {
            uart_write("ERR 5 CAL\r\n");
        }
        return;
    }
    if (strcmp(sub, "L") == 0) {
        const range_t r = meas_manual_range();
        const uint32_t f = meas_manual_freq();
        const float *std = cal_load_std_table(r);
        if (std == 0) { uart_write("ERR 4 RANGE\r\n"); return; }
        cal_load_t ld;
        if (cal_measure_load(f, r, std[0], std[1], &ld) == 0) {
            uart_printf("OK CAL_L level=%d\r\n", cal_get_level());
            log_event(LOG_CAL, 3U, (uint32_t)cal_get_level());
        } else {
            uart_write("ERR 5 CAL\r\n");
        }
        return;
    }
    uart_write("# usage: CAL A|O|L|S\r\n");
}

/* ---- 诊断命令 ------------------------------------------------------------ */
static void do_lvl(void)
{
    const meas_result_t *m = meas_last();
    const cal_record_t *rec = cal_record();
    char a[24], b[24], c[24], d[24], e[24];
    results_format_sig(m->vin_rms, a, (uint32_t)sizeof(a));
    results_format_sig(m->vo_rms, b, (uint32_t)sizeof(b));
    results_format_sig(m->rho, c, (uint32_t)sizeof(c));
    results_format_fixed(m->spread * 100.0f, 3U, d, (uint32_t)sizeof(d));
    results_format_fixed(m->h2 * 100.0f, 3U, e, (uint32_t)sizeof(e));
    uart_printf("LVL Vin=%s Vo=%s rho=%s spread=%s%% h2=%s%% cal=%d clip_diag=%08lX\r\n",
                a, b, c, d, e, cal_get_level(), (unsigned long)m->diag32);
    char ar[24], ai[24];
    results_format_sig(rec->alpha[CFG_BAND_1KHZ].alpha.re, ar, (uint32_t)sizeof(ar));
    results_format_sig(rec->alpha[CFG_BAND_1KHZ].alpha.im, ai, (uint32_t)sizeof(ai));
    uart_printf("alpha(1k)=%s+j%s k_in=%d k_out=%d stale=%d\r\n",
                ar, ai, (int)(rec->k_in * 1000.0f), (int)(rec->k_out * 1000.0f),
                cal_is_stale());
}

static void do_trace(void)
{
    const meas_trace_t *t = meas_trace();
    uart_printf("T state=%u steps=%u total=%lums clip=%u retry=%u blocks=%u aborted=%u\r\n",
                (unsigned int)t->state, (unsigned int)t->steps,
                (unsigned long)t->t_total_ms, (unsigned int)t->clip_count,
                (unsigned int)t->retry_count, (unsigned int)t->blocks,
                (unsigned int)t->aborted);
    char zr[24], pr[24], sl[24];
    results_format_sig(t->z_rough, zr, (uint32_t)sizeof(zr));
    results_format_fixed(t->phi_rough, 1U, pr, (uint32_t)sizeof(pr));
    results_format_sig(t->slope, sl, (uint32_t)sizeof(sl));
    uart_printf("rough |Z|=%s phi=%s slope=%s type p=%c s=%c f=%c\r\n",
                zr, pr, sl, (char)t->type_primary, (char)t->type_slope, (char)t->type_final);
    for (uint8_t i = 0U; i < t->steps; ++i) {
        char rr[24];
        results_format_sig(t->rho_seq[i], rr, (uint32_t)sizeof(rr));
        uart_printf("  step%u f=%lu r=%u rho=%s\r\n", (unsigned int)i,
                    (unsigned long)t->f_seq[i], (unsigned int)t->r_seq[i], rr);
    }
}

static void do_info(void)
{
    const int fail = selftest_run(1);
    selftest_print(1);
    uart_printf("I fail_mask=%08lX cal_level=%d flash seq=%lu bank=%d slot=%d crc=%08lX writes=%lu\r\n",
                (unsigned long)fail, cal_get_level(),
                (unsigned long)cal_flash_seq(), cal_flash_bank(), cal_flash_slot(),
                (unsigned long)cal_flash_crc(), (unsigned long)cal_flash_writes());
    uart_printf("rx_overflow=%lu tx_dropped=%lu stack_free=%lu oled=%d err=%lu\r\n",
                (unsigned long)uart_rx_overflow(), (unsigned long)uart_tx_dropped(),
                (unsigned long)board_stack_free(), i2c_oled_present(),
                (unsigned long)i2c_oled_errors());
    const uint16_t n = log_count();
    uart_printf("log %u entries:\r\n", (unsigned int)n);
    for (uint16_t i = 0U; i < n; ++i) {
        uint8_t kind; uint32_t a, b, ts;
        if (log_get(i, &kind, &a, &b, &ts) == 0) {
            uart_printf("  [%lu] k=%u a=%lu b=%lu\r\n", (unsigned long)ts,
                        (unsigned int)kind, (unsigned long)a, (unsigned long)b);
        }
    }
}

static void do_ver(void)
{
    uart_printf("VER fw=%s proto=%u cal_blob=%u build=" __DATE__ " " __TIME__ "\r\n",
                CFG_FW_VERSION, (unsigned int)CFG_PROTO_VERSION,
                (unsigned int)CFG_CAL_BLOB_VERSION);
    uart_printf("clk=%luHz uptime=%lums reset=%08lX cal_level=%d degraded=%lu\r\n",
                (unsigned long)board_sysclk_hz(), (unsigned long)board_millis(),
                (unsigned long)board_reset_cause(), cal_get_level(),
                (unsigned long)board_clock_degraded);
}

void proto_help(void)
{
    puts_slow("# commands:\r\n");
    puts_slow("#  ?/VER        help / version\r\n");
    puts_slow("#  F f          set freq 100|1000|10000|100000\r\n");
    puts_slow("#  R r          set range 0..5 (10R/100R/1k/10k/100k/1M)\r\n");
    puts_slow("#  A / M        auto measure / measure at F,R\r\n");
    puts_slow("#  CAL A|O|L    alpha / open / dual-load cal\r\n");
    puts_slow("#  CAL S        unsupported -> ERR 9\r\n");
    puts_slow("#  RAW ON|n|OFF ADC snapshot frame (n=1..2)\r\n");
    puts_slow("#  SAVE         write cal to Flash\r\n");
    puts_slow("#  SCAN f1 f2 n sweep, CSV out\r\n");
    puts_slow("#  LVL / T / I  levels / trace / selftest+log\r\n");
}

/* ---- 命令分发 ------------------------------------------------------------ */
static void dispatch(char *line, int echo, int *meas_ran)
{
    char *p = line;
    char *cmd = next_tok(&p);
    if (cmd == 0) { return; }
    str_upper(cmd);

    if (strcmp(cmd, "?") == 0) { proto_help(); return; }
    if (strcmp(cmd, "VER") == 0) { do_ver(); return; }
    if (strcmp(cmd, "I") == 0) { do_info(); return; }
    if (strcmp(cmd, "T") == 0) { do_trace(); return; }
    if (strcmp(cmd, "LVL") == 0) { do_lvl(); return; }

    if (strcmp(cmd, "F") == 0) {
        uint32_t f;
        if (parse_u32(next_tok(&p), &f) && freq_valid(f)) {
            meas_set_manual(f, meas_manual_range());
            uart_printf("OK F=%lu\r\n", (unsigned long)f);
        } else {
            uart_write("# usage: F 100|1000|10000|100000\r\n");
        }
        return;
    }
    if (strcmp(cmd, "R") == 0) {
        uint32_t r;
        if (parse_u32(next_tok(&p), &r) && (r < (uint32_t)RANGE_NUM)) {
            meas_set_manual(meas_manual_freq(), (range_t)r);
            uart_printf("OK R=%lu\r\n", (unsigned long)r);
        } else {
            uart_write("# usage: R 0..5\r\n");
        }
        return;
    }
    if ((strcmp(cmd, "A") == 0) || (strcmp(cmd, "M") == 0)) {
        if ((echo != 0) || (*meas_ran != 0)) {
            uart_write("BUSY\r\n");
            return;
        }
        run_meas(strcmp(cmd, "A") == 0);
        *meas_ran = 1;
        return;
    }
    if (strcmp(cmd, "CAL") == 0) { do_cal(next_tok(&p)); return; }
    if (strcmp(cmd, "SAVE") == 0) {
        if (cal_save(cal_record()) == 0) {
            uart_printf("OK SAVE seq=%lu\r\n", (unsigned long)cal_flash_seq());
            log_event(LOG_FLASH, cal_flash_seq(), 0U);
        } else {
            uart_write("ERR 5 CAL\r\n");
        }
        return;
    }
    if (strcmp(cmd, "RAW") == 0) {
        char *arg = next_tok(&p);
        if (arg == 0) { uart_write("# usage: RAW ON|n|OFF\r\n"); return; }
        str_upper(arg);
        if (strcmp(arg, "OFF") == 0) { uart_write("OK RAW OFF\r\n"); return; }
        uint32_t nb = 1U;
        if (strcmp(arg, "ON") != 0) {
            if (!parse_u32(arg, &nb) || (nb < 1U) || (nb > 2U)) {
                uart_write("# usage: RAW ON|1|2|OFF\r\n");
                return;
            }
        }
        do_raw((uint16_t)nb);
        return;
    }
    if (strcmp(cmd, "SCAN") == 0) {
        uint32_t f1, f2, n;
        if (parse_u32(next_tok(&p), &f1) && parse_u32(next_tok(&p), &f2)
            && parse_u32(next_tok(&p), &n)) {
            do_scan(f1, f2, n);
        } else {
            uart_write("# usage: SCAN f1 f2 n\r\n");
        }
        return;
    }
    uart_printf("# unknown command: %s (try ?)\r\n", cmd);
}

/* ---- 对外接口 ------------------------------------------------------------ */
void proto_init(proto_result_cb cb)
{
    s_cb = cb;
    s_len = 0U;
    s_ovf = 0;
    s_busy_echo = 0;
    s_raw_seq = 0U;
    s_rlen = 0U;
}

void proto_note_auto_meas(void)
{
    s_busy_echo = 1;
}

void proto_banner(void)
{
    puts_slow("\r\n# ===============================================\r\n");
    puts_slow("# LCR auto-balancing-bridge RLC meter  " CFG_FW_VERSION "\r\n");
    puts_slow("# STM32F407VET6  SYSCLK 168 MHz  HSE 8 MHz\r\n");
    puts_slow("# type ? for help, A to auto-measure\r\n");
    puts_slow("# ===============================================\r\n");
}

void proto_poll(void)
{
    const int echo = s_busy_echo;
    s_busy_echo = 0;
    int meas_ran = 0;
    int c;
    while ((c = uart_read_byte()) >= 0) {
        if ((c == '\r') || (c == '\n')) {
            if (s_ovf != 0) {           /* 超长行整体丢弃，不截断执行 */
                s_ovf = 0;
                s_len = 0U;
                continue;
            }
            if (s_len == 0U) { continue; }
            s_line[s_len] = '\0';
            dispatch(s_line, echo, &meas_ran);
            s_len = 0U;
        } else {
            if (s_len < CFG_UART_LINE_MAX) {
                s_line[s_len++] = (char)c;
            } else {
                s_ovf = 1;
            }
        }
    }
}