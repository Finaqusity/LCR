#ifndef LCR_ERROR_H
#define LCR_ERROR_H

/* 诊断位与错误码名称（架构文档 §13.4）。
 * diag 是 32 位掩码：低 16 位沿用旧定义，新增位从 bit16 起，不重排。 */

#include <stdint.h>

#define DIAG_CAL_LEVEL_LOW   0x00000001U  /* cal_level < 3 */
#define DIAG_CAL_STALE       0x00000002U  /* 夹具变化/开路残差超限 */
#define DIAG_OPEN_INVALID    0x00000004U  /* 开路数据无效（method=1） */
#define DIAG_LOAD_INVALID    0x00000008U  /* 双负载数据无效 */
#define DIAG_CLIP            0x00000010U
#define DIAG_SIG_LOW         0x00000020U
#define DIAG_DMA_ERR         0x00000040U
#define DIAG_ADC_OVR         0x00000080U
#define DIAG_RELAY_ERR       0x00000100U
#define DIAG_DDS_SILENT      0x00000200U
#define DIAG_TYPE_UNCERTAIN  0x00000400U
#define DIAG_SPREAD_HIGH     0x00000800U
#define DIAG_TIMEOUT         0x00001000U
#define DIAG_FLASH_ERR       0x00002000U
#define DIAG_STACK_NEAR      0x00004000U
#define DIAG_SELFTEST_FAIL   0x00008000U
#define DIAG_AWD_ABORT       0x00010000U
#define DIAG_RX_OVERFLOW     0x00020000U
#define DIAG_BUSY            0x00040000U

/* boot_status / selftest 位（§10.3） */
#define ST_CLOCK             0x00000001U
#define ST_HSE               0x00000002U
#define ST_DDS               0x00000004U
#define ST_ADC               0x00000008U
#define ST_ALPHA             0x00000010U
#define ST_RELAY             0x00000020U
#define ST_CAL               0x00000040U
#define ST_STACK             0x00000080U
#define ST_WDG               0x00000100U  /* 信息位：看门狗复位 */
#define ST_OLED              0x00000200U  /* 降级，不阻塞测量 */
#define ST_FLASH             0x00000400U
#define ST_DFT               0x00000800U

/* 错误码 → 名称（结果行 err= 与 ERR 行共用一套编号）。
 * 数值必须与 meas.h 的 meas_err_t 一致。 */
const char *error_name(int code);

#endif /* LCR_ERROR_H */