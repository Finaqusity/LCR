#ifndef LCR_TEST_H
#define LCR_TEST_H

/* 板上金标准回归（§8.6/§10.2，仅在定义 LCR_SELFTEST 时编译进固件）。
 * 自包含：合成整数波形 → 与 double 参考 DFT/闭式解逐项比对，
 * 容差幅度 1e-4 相对、相位 1e-3°、阻抗 1e-4 相对。不依赖外部向量文件，
 * 因此构建不需要 numpy。返回失败项数，0=全过；经串口打印 PASS/FAIL。 */

int test_run_golden(void);

#endif /* LCR_TEST_H */