#ifndef LCR_PROTO_H
#define LCR_PROTO_H

/* L4 串口协议（§8.5、§13.4）：ASCII 行命令 + RAW 二进制快照帧。
 * 行缓冲 ≤96 B，超长整行丢弃；测量期间 RX 只入环不解析，结束后按序补解析。
 * 本模块只依赖 uart/meas/cal/results/dds/relay/adc/selftest/log，不依赖 ui：
 * 测量结果通过 init 时注册的回调外送（main 把 ui_on_result 挂上）。 */

#include <stdint.h>
#include "meas.h"

typedef void (*proto_result_cb)(const meas_result_t *res);

void proto_init(proto_result_cb cb);
void proto_poll(void);                 /* 主循环非阻塞：取行、分发命令 */
void proto_banner(void);               /* 启动横幅（阻塞发送，仅上电用） */
void proto_help(void);

/* main 在"上电自动测量/周期重测"结束后调用：
 * 下一条在测量期间缓冲进来的 A/M 回 BUSY，不排队重测（§8.5）。 */
void proto_note_auto_meas(void);

#endif /* LCR_PROTO_H */