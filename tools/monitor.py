#!/usr/bin/env python3
"""串口监视器：带时间戳、可存盘、可定时发命令。

用法:
    python tools/monitor.py --list
    python tools/monitor.py --port COM5                 # 115200 8N1，Ctrl+C 退出
    python tools/monitor.py --port COM5 --log logs/uart.log
    python tools/monitor.py --port COM5 --send "raw on" --duration 10
    python tools/monitor.py --port COM5 --hex            # 原始字节（十六进制）显示

依赖: pyserial (已装在运行期 Python 里)
"""

from __future__ import annotations

import argparse
import datetime as dt
import os
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover
    sys.exit("缺少 pyserial：pip install pyserial -i https://mirrors.aliyun.com/pypi/simple")


def cmd_list() -> int:
    ports = list(list_ports.comports())
    if not ports:
        print("没有发现串口设备。检查 USB-TTL 驱动 / 换数据线。")
        return 0
    for p in ports:
        print(f"{p.device:8s} {p.description}")
    return 0


def now() -> str:
    return dt.datetime.now().strftime("%H:%M:%S.%f")[:-3]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true", help="列出可用串口后退出")
    ap.add_argument("--port", default=None)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--log", default=None, help="日志文件路径")
    ap.add_argument("--send", default=None, help="打开后发送一行命令（自动加 \\n）")
    ap.add_argument("--duration", type=float, default=None, help="运行多少秒后退出")
    ap.add_argument("--hex", action="store_true", help="以十六进制显示原始字节")
    ap.add_argument("--no-ts", action="store_true", help="不打印时间戳")
    args = ap.parse_args()

    if args.list:
        return cmd_list()
    if not args.port:
        ap.error("需要 --port（或用 --list 查看）")

    logf = None
    if args.log:
        os.makedirs(os.path.dirname(os.path.abspath(args.log)) or ".", exist_ok=True)
        logf = open(args.log, "a", encoding="utf-8")
        logf.write(f"\n===== {dt.datetime.now():%Y-%m-%d %H:%M:%S} open {args.port} @{args.baud} =====\n")
        logf.flush()

    ser = serial.Serial(args.port, args.baud, timeout=0.2)
    print(f"[monitor] {args.port} @ {args.baud} 8N1 就绪，Ctrl+C 退出")

    if args.send:
        time.sleep(0.3)
        ser.write((args.send + "\n").encode())
        print(f"[monitor] -> {args.send}")

    t0 = time.time()
    try:
        while True:
            data = ser.read(4096)
            if data:
                if args.hex:
                    txt = " ".join(f"{b:02X}" for b in data)
                else:
                    txt = data.decode("utf-8", errors="replace").rstrip("\r\n")
                if args.no_ts:
                    print(txt, flush=True)
                else:
                    print(f"[{now()}] {txt}", flush=True)
                if logf:
                    logf.write(data.decode("utf-8", errors="replace"))
                    logf.flush()
            if args.duration and (time.time() - t0) > args.duration:
                break
    except KeyboardInterrupt:
        print("\n[monitor] 已退出")
    finally:
        ser.close()
        if logf:
            logf.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
