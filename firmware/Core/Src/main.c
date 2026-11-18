/* LCR 自动平衡电桥 RLC 表 —— 应用入口（§4.7 启动顺序、§8.5 主循环）。
 * 无 RTOS、无 heap、无 VLA。单线程事件循环：proto_poll → ui_poll →
 * 自动测量 → 喂狗。测量阻塞期间 RX 只入环，结束后由 proto 补解析。
 *
 * 一键启动：上电自动测量（默认）+ 串口 A/M + 可选 PD7 按键（CFG_KEY_ENABLE）。
 * 致命时钟失败仍保留串口交互，但拒绝给出测量值（meas 内部 guard）。 */

#include "board.h"
#include "board_config.h"
#include "uart.h"
#include "dds.h"
#include "relay.h"
#include "adc.h"
#include "i2c_oled.h"
#include "cal.h"
#include "meas.h"
#include "selftest.h"
#include "log.h"
#include "proto.h"
#include "ui.h"
#include "error.h"
#include "stm32f4xx.h"

#ifdef LCR_SELFTEST
#include "test.h"
#endif

static uint32_t s_next_auto;
static int      s_auto_on;
static uint32_t s_led_ms;

/* 上电自动测量 / 周期重测（§8.5）。结果只上 OLED，不刷串口（避免洪水）。 */
static void auto_measure_poll(void)
{
    if (s_auto_on == 0) { return; }
    const uint32_t now = board_millis();
    if ((int32_t)(now - s_next_auto) < 0) { return; }

    if (board_clock_degraded == 0U) {
        meas_result_t res;
        const meas_err_t e = meas_auto(&res);
        ui_on_result(&res);
        proto_note_auto_meas();        /* 测量期间缓冲的 A/M → BUSY */
        log_event(LOG_MEAS, (uint32_t)meas_err_code(e), res.diag32);
    }
    if (CFG_AUTO_MEAS_MS > 0U) {
        s_next_auto = board_millis() + CFG_AUTO_MEAS_MS;
    } else {
        s_auto_on = 0;                 /* 0 = 只测一次 */
    }
}

int main(void)
{
    /* 最早期安全态：PD0–PD6 输出低 → 常闭 10 kΩ 档独自在位（D16/R3）。 */
    board_relay_pins_low();

    board_init();                      /* HSE/PLL 168 MHz、Flash 5WS、SysTick、DWT */
    uart_init(CFG_UART_BAUD);
    log_init();
    proto_init(ui_on_result);
    proto_banner();

    board_delay_ms(CFG_DDS_POWERUP_MS);   /* AD9833 上电 ≥20 ms */
    const int dds_ok = dds_init();
    relay_init();
    const int adc_ok = adc_init();
    const int oled_ok = i2c_oled_init();  /* 失败降级，不致命 */
    const int cal_ok = cal_init();
    meas_init();
    ui_init();

    board_iwdg_start(CFG_IWDG_TIMEOUT_MS);

    const int st = selftest_run(0);       /* 上电快速子集，守 boot 预算 */
    uart_printf("# boot: dds=%d adc=%d oled=%d cal=%d level=%d boot_status=%08lX\r\n",
                dds_ok, adc_ok, oled_ok, cal_ok, cal_get_level(),
                (unsigned long)selftest_status());
    if (board_clock_degraded != 0U) {
        uart_write("# WARN: HSE failed, running HSI 16 MHz -- measurement refused\r\n");
    }
    if ((st & ~(ST_WDG | ST_OLED)) != 0) {
        uart_printf("# WARN: selftest fail mask=%08lX (type I for detail)\r\n",
                    (unsigned long)st);
    }
    if (cal_get_level() == 0) {
        uart_write("# uncalibrated: run CAL A / CAL O / CAL L then SAVE\r\n");
    }
    log_event(LOG_BOOT, (uint32_t)st, (uint32_t)cal_get_level());

#ifdef LCR_SELFTEST
    (void)test_run_golden();              /* 板上金标准回归（§8.6/§10.2） */
#endif

    s_next_auto = board_millis() + 100U;  /* 上电稍候即自动测一次 */
    s_auto_on = 1;
    s_led_ms = board_millis();

    for (;;) {
        proto_poll();                     /* 非阻塞：解析 UART 行命令 */
        ui_poll();                        /* 慢路径：空闲才整页刷 OLED */
        auto_measure_poll();              /* 上电自动 / 周期重测 */

        const uint32_t now = board_millis();
        if ((now - s_led_ms) >= CFG_LED_BLINK_MS) {
            board_led_toggle();
            s_led_ms = now;
        }
        board_iwdg_feed();
    }
}