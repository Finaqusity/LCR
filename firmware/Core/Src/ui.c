/* L4 OLED 界面（§8.5）。无按键 → 空闲自动轮页；测量中不整屏刷新。
 * 浮点全部经 results_format_* 转字符串，OLED 行用 snprintf 拼整数/%s。
 * 模块不在位时静默降级。 */

#include "ui.h"
#include "board.h"
#include "board_config.h"
#include "i2c_oled.h"
#include "results.h"
#include "cal.h"
#include "meas.h"
#include "selftest.h"
#include "relay.h"
#include "error.h"

#include <stdio.h>
#include <string.h>

#define UI_ROTATE_MS 4000U
#define UI_P0_DWELL_MS 6000U

static meas_result_t s_res;
static int      s_have;
static uint8_t  s_page;
static int      s_dirty;
static uint32_t s_last_ms;

void ui_init(void)
{
    memset(&s_res, 0, sizeof(s_res));
    s_have = 0;
    s_page = 0U;
    s_dirty = 1;
    s_last_ms = board_millis();
}

void ui_on_result(const meas_result_t *res)
{
    if (res == 0) { return; }
    s_res = *res;
    s_have = 1;
    s_page = 0U;            /* 新读数回到主页面 */
    s_dirty = 1;
    s_last_ms = board_millis();
}

/* ---- 各页面渲染 ---------------------------------------------------------- */
static void page_main(void)
{
    char main[24], l2[32], l3[48], l4[32];
    results_format_main(&s_res, main, (uint32_t)sizeof(main));
    i2c_oled_text_big(0U, 0U, main);

    snprintf(l2, sizeof(l2), "f=%lu R=%u CAL=%u",
             (unsigned long)s_res.f_hz, (unsigned int)s_res.range,
             (unsigned int)s_res.cal_level);
    i2c_oled_text(3U, 0U, l2);

    char db[16], qb[16];
    results_format_fixed(s_res.d, 3U, db, (uint32_t)sizeof(db));
    results_format_fixed(s_res.q, 1U, qb, (uint32_t)sizeof(qb));
    snprintf(l3, sizeof(l3), "D=%s Q=%s", db, qb);
    i2c_oled_text(5U, 0U, l3);

    char pb[16];
    results_format_fixed(s_res.z_phase_deg, 1U, pb, (uint32_t)sizeof(pb));
    snprintf(l4, sizeof(l4), "phi=%s conf=%u", pb, (unsigned int)s_res.confidence);
    i2c_oled_text(6U, 0U, l4);

    if (s_res.err != MEAS_OK) {
        char e[32];
        snprintf(e, sizeof(e), "ERR %d %s", meas_err_code(s_res.err),
                 meas_err_name(s_res.err));
        i2c_oled_text(7U, 0U, e);
    }
}

static void page_diag(void)
{
    char a[24], b[24], l[64];
    results_format_sig(s_res.vin_rms, a, (uint32_t)sizeof(a));
    results_format_sig(s_res.vo_rms, b, (uint32_t)sizeof(b));
    snprintf(l, sizeof(l), "Vin=%s Vo=%s", a, b);
    i2c_oled_text(0U, 0U, l);

    results_format_sig(s_res.rho, a, (uint32_t)sizeof(a));
    snprintf(l, sizeof(l), "rho=%s retry=%u", a, (unsigned int)s_res.retries);
    i2c_oled_text(1U, 0U, l);

    results_format_fixed(s_res.spread * 100.0f, 3U, a, (uint32_t)sizeof(a));
    snprintf(l, sizeof(l), "spread=%s%%", a);
    i2c_oled_text(2U, 0U, l);

    results_format_fixed(s_res.h2 * 100.0f, 3U, a, (uint32_t)sizeof(a));
    results_format_fixed(s_res.h3 * 100.0f, 3U, b, (uint32_t)sizeof(b));
    snprintf(l, sizeof(l), "h2=%s%% h3=%s%%", a, b);
    i2c_oled_text(3U, 0U, l);

    snprintf(l, sizeof(l), "blocks=%u clip=%u",
             (unsigned int)s_res.blocks,
             (unsigned int)((s_res.diag32 & DIAG_CLIP) ? 1U : 0U));
    i2c_oled_text(4U, 0U, l);

    snprintf(l, sizeof(l), "cal=%u stale=%d", (unsigned int)s_res.cal_level,
             cal_is_stale());
    i2c_oled_text(5U, 0U, l);

    snprintf(l, sizeof(l), "diag=%08lX", (unsigned long)s_res.diag32);
    i2c_oled_text(6U, 0U, l);

    snprintf(l, sizeof(l), "t=%lums", (unsigned long)s_res.time_ms);
    i2c_oled_text(7U, 0U, l);
}

static void page_cal(void)
{
    const cal_record_t *rec = cal_record();
    i2c_oled_text(0U, 0U, "CAL open/load @1k");
    for (uint8_t r = 0U; r < 6U; ++r) {
        char l[24];
        const uint8_t ov = rec->net[CFG_BAND_1KHZ][r].valid;
        const uint8_t lv = rec->load[r].valid;
        snprintf(l, sizeof(l), "R%u O:%u L:%u", (unsigned int)r,
                 (unsigned int)ov, (unsigned int)lv);
        i2c_oled_text((uint8_t)(1U + r), 0U, l);
    }
    char l[24];
    snprintf(l, sizeof(l), "stale=%d bench=%d", cal_is_stale(), cal_bench_ok());
    i2c_oled_text(7U, 0U, l);
}

static void st_line(uint8_t page, const char *name, uint32_t bit, int info)
{
    const uint32_t s = selftest_status();
    char l[24];
    const int set = ((s & bit) != 0U);
    snprintf(l, sizeof(l), "%s:%s", name,
             info ? (set ? "info" : "-") : (set ? "FAIL" : "ok"));
    i2c_oled_text(page, 0U, l);
}

static void page_selftest(void)
{
    char l[24];
    snprintf(l, sizeof(l), "SELFTEST %08lX", (unsigned long)selftest_status());
    i2c_oled_text(0U, 0U, l);
    st_line(1U, "CLK", ST_CLOCK, 0);
    st_line(2U, "HSE", ST_HSE, 0);
    st_line(3U, "DDS", ST_DDS, 0);
    st_line(4U, "ADC", ST_ADC, 0);
    st_line(5U, "RELAY", ST_RELAY, 0);
    st_line(6U, "CAL", ST_CAL, 0);
    st_line(7U, "OLED", ST_OLED, 1);
}

static void render(uint8_t page)
{
    i2c_oled_clear();
    switch (page) {
    case 1U:  page_diag();     break;
    case 2U:  page_cal();      break;
    case 3U:  page_selftest(); break;
    default:  page_main();     break;
    }
    i2c_oled_flush();
}

void ui_poll(void)
{
    if (i2c_oled_present() == 0) { return; }   /* 降级：不阻塞 */
    if (meas_busy() != 0) { return; }          /* 测量中不整屏刷新 */

    const uint32_t now = board_millis();
    if (s_dirty != 0) {
        render(s_page);
        s_dirty = 0;
        s_last_ms = now;
        return;
    }
    /* 空闲自动轮页：主页面多停留，让评委先看清读数。 */
    const uint32_t dwell = (s_page == 0U) ? UI_P0_DWELL_MS : UI_ROTATE_MS;
    if ((s_have != 0) && ((now - s_last_ms) >= dwell)) {
        s_page = (uint8_t)((s_page + 1U) & 3U);
        render(s_page);
        s_last_ms = now;
    }
}