#ifndef LCR_SELFTEST_H
#define LCR_SELFTEST_H

/* 板上自检（§10.3）。返回失败位掩码（ST_*，见 error.h），0=全过。
 * verbose=0：上电快速子集（时钟/HSE/栈/Flash/校准/DFT/OLED），控制在 boot 预算内；
 * verbose=1：`I` 命令全量（加 DDS 四频点、ADC 噪声、α 一致性、继电器逐档）。
 * 看门狗复位原因是信息位，不计入失败。OLED 失败只降级，不阻塞测量。 */

#include <stdint.h>

int      selftest_run(int verbose);
uint32_t selftest_status(void);      /* 最近一次的 boot_status 位图 */
void     selftest_print(int verbose);/* 串口逐项 PASS/FAIL + 位图 */

#endif /* LCR_SELFTEST_H */