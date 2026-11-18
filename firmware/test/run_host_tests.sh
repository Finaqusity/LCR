#!/bin/sh
# 宿主侧金标准自测（§11.1）。用本机 cc（clang/gcc）编译纯算法模块并运行。
# 用法：sh firmware/test/run_host_tests.sh   （退出码 0=全过）
set -e
cd "$(dirname "$0")"
CC="${CC:-cc}"
OUT="${TMPDIR:-/tmp}"
CFLAGS="-std=gnu11 -O2 -Wall -ffunction-sections -fdata-sections"

echo "== test_dsp (G1/G2/G3/G4/G8/G9) =="
$CC $CFLAGS -I../Core/Inc test_dsp.c ../Core/Src/dsp.c ../Core/Src/results.c \
    -lm -Wl,-dead_strip -o "$OUT/lcr_test_dsp"
"$OUT/lcr_test_dsp"

echo "== test_cal (G5/G6/G7) =="
$CC $CFLAGS -Wno-int-to-pointer-cast -Ihost -I../Core/Inc test_cal.c \
    ../Core/Src/cal.c ../Core/Src/meas.c ../Core/Src/dsp.c \
    -lm -Wl,-dead_strip -o "$OUT/lcr_test_cal"
"$OUT/lcr_test_cal"

echo "ALL HOST TESTS PASS"