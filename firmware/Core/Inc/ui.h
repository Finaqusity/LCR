#ifndef LCR_UI_H
#define LCR_UI_H

/* L4 OLED 用户界面（§8.5）：P0 主读数 / P1 诊断 / P2 校准 / P3 自检。
 * 无按键（CFG_KEY_ENABLE=0）→ 空闲时自动轮页；测量中不整屏刷新。
 * 模块不在位（i2c_oled_present()=0）时全部静默降级，绝不阻塞测量。 */

#include "meas.h"

void ui_init(void);
void ui_on_result(const meas_result_t *res);   /* 注册为 proto 的结果回调 */
void ui_poll(void);                            /* 主循环慢路径：空闲才刷整页 */

#endif /* LCR_UI_H */