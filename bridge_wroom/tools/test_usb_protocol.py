#!/usr/bin/env python3
"""音频桥 USB 配置协议的快速上板测试。"""

from __future__ import annotations

import argparse
import sys
import time

import serial


def collect(port: serial.Serial, seconds: float, stop_prefix: str | None = None) -> list[str]:
    result: list[str] = []
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        raw = port.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
        result.append(line)
        print(line)
        if stop_prefix and line.startswith(stop_prefix):
            break
    return result


def send(port: serial.Serial, command: str) -> None:
    print(f">>> {command}")
    port.write((command + "\n").encode("ascii"))
    port.flush()


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(errors="backslashreplace")

    parser = argparse.ArgumentParser()
    parser.add_argument("port", nargs="?", default="COM9")
    args = parser.parse_args()

    found: list[str] = []
    with serial.Serial(args.port, 115200, timeout=0.2, write_timeout=2, dsrdtr=False, rtscts=False) as port:
        port.dtr = False
        port.rts = False
        time.sleep(1)
        found.extend(collect(port, 10, "@CFG READY "))
        if not any(line.startswith("@CFG READY ") for line in found):
            print("MISSING: @CFG READY")
            return 1

        send(port, "CFG HELLO")
        found.extend(collect(port, 2, "@CFG STATUS "))
        send(port, "CFG WIFI_SCAN")
        found.extend(collect(port, 12, "@CFG WIFI_END "))
        send(port, "CFG BT_LIST")
        found.extend(collect(port, 3, "@CFG BT_END "))
        send(port, "CFG LOG_STATUS")
        found.extend(collect(port, 2, "LOG_STATUS "))

    required = ("@CFG READY ", "@CFG HELLO ", "@CFG STATUS ", "@CFG WIFI_END ", "@CFG BT_END ", "LOG_STATUS ")
    missing = [prefix for prefix in required if not any(line.startswith(prefix) for line in found)]
    if missing:
        print("MISSING: " + ", ".join(missing))
        return 1
    print("USB_PROTOCOL_TEST_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
