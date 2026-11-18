# -*- coding: utf-8 -*-
"""构建期链接产物校验（docs/软件架构.md §8.2）。纯标准库，任何 Python3 可跑。

解析 GNU ld 的 .map（"Linker script and memory map" 段），做符号/段断言：
  1. _sidata + (_edata - _sdata) <= 0x08040000   代码+已初始化数据不越 256 KB 代码区
  2. _ebss <= __StackLimit                       .bss 不压 4 KB 栈保护区
  3. .dma_buf 段地址 ∈ [0x20000000, 0x20020000)  DMA 缓冲在 SRAM（非 CCM）
  4. .ccmram  段地址 ∈ [0x10000000, 0x10010000)  相位表/热循环在 CCM
  5. __StackLimit == _estack - 0x1000            栈保护区恰为 4 KB
任一失败 → exit 1（POST_BUILD 使构建失败，不允许"先烧了再说"）。

运行：python tools/check_map.py firmware/build/lcr-fw.map
"""
import re
import sys

CODE_LIMIT = 0x08040000
SRAM_LO, SRAM_HI = 0x20000000, 0x20020000
CCM_LO, CCM_HI = 0x10000000, 0x10010000
STACK_SIZE = 0x1000


def parse_map(text):
    # 只取内存映射部分，避开 Cross Reference / 归档成员里的同名符号。
    marker = "Linker script and memory map"
    idx = text.find(marker)
    body = text[idx:] if idx >= 0 else text

    syms = {}
    # 形如 "                0x20000000                _sdata = ."
    sym_re = re.compile(r"0x([0-9a-fA-F]+)\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:=|$)", re.M)
    for m in sym_re.finditer(body):
        name = m.group(2)
        if name not in syms:
            syms[name] = int(m.group(1), 16)

    secs = {}
    # 形如 ".dma_buf        0x20000...  0x2000 ..."（首个 hex 是运行地址）
    sec_re = re.compile(r"^(\.[A-Za-z_][A-Za-z0-9_.]*)\s+0x([0-9a-fA-F]+)", re.M)
    for m in sec_re.finditer(body):
        name = m.group(1)
        if name not in secs:
            secs[name] = int(m.group(2), 16)
    return syms, secs


def main():
    if len(sys.argv) < 2:
        print("usage: check_map.py <lcr-fw.map>")
        return 2
    path = sys.argv[1]
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError as e:
        print("FAIL: cannot read map: %s" % e)
        return 1

    syms, secs = parse_map(text)
    fails = []

    def need_sym(name):
        if name not in syms:
            fails.append("missing symbol %s in map" % name)
            return None
        return syms[name]

    sidata = need_sym("_sidata")
    sdata = need_sym("_sdata")
    edata = need_sym("_edata")
    ebss = need_sym("_ebss")
    stacklimit = need_sym("__StackLimit")
    estack = need_sym("_estack")

    if None not in (sidata, sdata, edata):
        flash_end = sidata + (edata - sdata)
        if flash_end > CODE_LIMIT:
            fails.append("code+data end 0x%08X > 0x%08X (256K code region)"
                         % (flash_end, CODE_LIMIT))
        else:
            print("ok: code+data end 0x%08X <= 0x%08X (%.1f KB used)"
                  % (flash_end, CODE_LIMIT, (flash_end - 0x08000000) / 1024.0))

    if None not in (ebss, stacklimit):
        if ebss > stacklimit:
            fails.append("_ebss 0x%08X > __StackLimit 0x%08X (RAM into stack guard)"
                         % (ebss, stacklimit))
        else:
            print("ok: _ebss 0x%08X <= __StackLimit 0x%08X (stack margin %lu B)"
                  % (ebss, stacklimit, stacklimit - ebss))

    if ".dma_buf" in secs:
        a = secs[".dma_buf"]
        if not (SRAM_LO <= a < SRAM_HI):
            fails.append(".dma_buf 0x%08X not in SRAM [0x%08X,0x%08X)"
                         % (a, SRAM_LO, SRAM_HI))
        else:
            print("ok: .dma_buf 0x%08X in SRAM" % a)
    else:
        fails.append(".dma_buf section absent (ADC DMA buffer must be placed)")

    if ".ccmram" in secs:
        a = secs[".ccmram"]
        if not (CCM_LO <= a < CCM_HI):
            fails.append(".ccmram 0x%08X not in CCM [0x%08X,0x%08X)"
                         % (a, CCM_LO, CCM_HI))
        else:
            print("ok: .ccmram 0x%08X in CCM" % a)
    else:
        fails.append(".ccmram section absent (phase tables must anchor CCM)")

    if None not in (stacklimit, estack):
        if stacklimit != estack - STACK_SIZE:
            fails.append("__StackLimit 0x%08X != _estack-0x1000 (0x%08X)"
                         % (stacklimit, estack - STACK_SIZE))
        else:
            print("ok: __StackLimit == _estack - 0x1000 (4K stack guard)")

    if fails:
        for f in fails:
            print("FAIL: " + f)
        return 1
    print("check_map: ALL PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())