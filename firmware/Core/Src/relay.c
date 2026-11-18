/* 量程继电器驱动（§4.3）。ULN2803 灌电流驱动，线圈 5 V/21.1 mA，稳定 ≤3 ms。
 * 互锁：同一时刻最多 2 只线圈吸合（目标档 + PD3），7 只全开 148 mA 未实测（R17）。
 * 换档全程 DDS 静音，避免 I-V 开环 / BUF634A 过冲；解除静音由调用方负责。 */

#include "relay.h"
#include "board.h"
#include "board_config.h"
#include "dds.h"

static range_t  relay_range = RANGE_10K;
static int      relay_cal = 0;
static uint32_t relay_count = 0U;

static const char *const relay_names[RANGE_NUM] = {
    "10R", "100R", "1K", "10K", "100K", "1M"
};

const char *relay_range_name(range_t r)
{
    return ((uint32_t)r < (uint32_t)RANGE_NUM) ? relay_names[r] : "?";
}

static void relay_pins(uint32_t set_mask, uint32_t clr_mask)
{
    GPIO_TypeDef *port = board_gpio(CFG_RELAY_PORT);
    /* BSRR 一次写入：置位低 16 位、复位高 16 位，无中间毛刺 */
    port->BSRR = (set_mask & 0xFFFFUL) | ((clr_mask & 0xFFFFUL) << 16);
}

static uint32_t relay_pin(range_t r)
{
    return 1UL << (CFG_RELAY_PIN_FIRST + (uint32_t)r);
}

static uint32_t relay_cal_pin(void)
{
    return 1UL << (CFG_RELAY_PIN_FIRST + CFG_RANGE_CAL_PIN);
}

static uint32_t relay_range_mask(void)
{
    uint32_t m = 0U;
    for (uint32_t i = 0U; i < CFG_RANGE_NUM; ++i) {
        m |= relay_pin((range_t)i);
    }
    return m;
}

void relay_init(void)
{
    /* board_relay_pins_low() 已在最早期把 PD0–PD6 配成输出低；这里只同步状态。 */
    relay_pins(0U, relay_range_mask() | relay_cal_pin());
    relay_range = RANGE_10K;      /* 常闭触点在位 */
    relay_cal = 0;
    relay_count = 0U;
}

void relay_all_off(void)
{
    relay_pins(0U, relay_range_mask() | relay_cal_pin());
    relay_range = RANGE_10K;
    relay_cal = 0;
    relay_count++;
}

int relay_select_range(range_t r)
{
    if ((uint32_t)r >= (uint32_t)RANGE_NUM) {
        relay_all_off();          /* 非法组合 → 全断（失效安全）+ 报错 */
        return -1;
    }

    dds_hold_reset(1);            /* 换档前必须静音 */

    uint32_t elapsed;
    if (r == RANGE_10K) {
        /* 常闭档：所有线圈断电，10 kΩ 自动在位 */
        relay_pins(0U, relay_range_mask());
        board_delay_ms(CFG_RELAY_MUTE_MS);
        board_delay_ms(CFG_RELAY_SETTLE_MS);
        elapsed = CFG_RELAY_MUTE_MS + CFG_RELAY_SETTLE_MS;
    } else {
        /* 先吸目标档（此时常闭 10 kΩ 仍并联，但 DDS 已静音），再断常闭 */
        const uint32_t target = relay_pin(r);
        relay_pins(target, relay_range_mask() & ~target);
        board_delay_ms(CFG_RELAY_SETTLE_MS);
        relay_pins(relay_pin(RANGE_10K), 0U);   /* PD3 吸合 → 常闭断开 */
        board_delay_ms(CFG_RELAY_POST_MS);
        elapsed = CFG_RELAY_SETTLE_MS + CFG_RELAY_POST_MS;
    }

    relay_range = r;
    relay_count++;
    return (int)elapsed;          /* 契约 §3.4：返回稳定后经过的 ms */
}

void relay_cal_enable(int on)
{
    relay_cal = on ? 1 : 0;
    if (on) {
        relay_pins(relay_cal_pin(), 0U);
    } else {
        relay_pins(0U, relay_cal_pin());
    }
    board_delay_ms(CFG_RELAY_POST_MS);
}

range_t relay_current_range(void)
{
    return relay_range;
}

int relay_cal_is_on(void)
{
    return relay_cal;
}

uint32_t relay_switch_count(void)
{
    return relay_count;
}

int relay_verify(void)
{
    uint32_t expect = 0U;
    if (relay_range != RANGE_10K) {
        expect |= relay_pin(relay_range) | relay_pin(RANGE_10K);
    }
    if (relay_cal) {
        expect |= relay_cal_pin();
    }
    const uint32_t mask = relay_range_mask() | relay_cal_pin();
    const uint32_t got = board_gpio(CFG_RELAY_PORT)->ODR & mask;
    return (got == expect) ? 0 : -1;
}