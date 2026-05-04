#!/usr/bin/env python3
"""
Mellon BLE console — host-side helper.

Talks to a Mellon over Nordic UART Service. Equivalent in spirit to nRF
Connect's NUS terminal but scriptable from the command line.

Requirements:
    pip install bleak

Examples:
    # Interactive REPL
    python tools/ble_console.py --address AA:BB:CC:DD:EE:FF

    # One-shot status check
    python tools/ble_console.py --address ... --once "status"

    # Stream to a file (for soak runs)
    python tools/ble_console.py --address ... --once "mode sniff" --tee sniff.log
"""

from __future__ import annotations

import argparse
import asyncio
import sys
from typing import Optional

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    print("error: install bleak first  (pip install bleak)", file=sys.stderr)
    sys.exit(1)

NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  # write
NUS_TX_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  # notify

DEFAULT_NAME = "Mellon"


async def find_device(address: Optional[str]) -> str:
    if address:
        return address
    print(f"scanning for '{DEFAULT_NAME}'...")
    devs = await BleakScanner.discover(timeout=5.0)
    for d in devs:
        if d.name == DEFAULT_NAME:
            print(f"  found {d.address}")
            return d.address
    print(f"  no device named '{DEFAULT_NAME}' visible — pass --address explicitly", file=sys.stderr)
    sys.exit(2)


async def run(args: argparse.Namespace) -> None:
    address = await find_device(args.address)
    tee = open(args.tee, "a", encoding="utf-8", buffering=1) if args.tee else None

    line_buf = b""

    def on_notify(_handle: int, data: bytearray) -> None:
        nonlocal line_buf
        line_buf += bytes(data)
        while b"\n" in line_buf:
            line, line_buf = line_buf.split(b"\n", 1)
            text = line.decode(errors="replace").rstrip("\r")
            print(text)
            if tee:
                tee.write(text + "\n")

    async with BleakClient(address) as client:
        if not client.is_connected:
            print("connect failed", file=sys.stderr)
            sys.exit(3)

        await client.start_notify(NUS_TX_CHAR_UUID, on_notify)

        async def send(line: str) -> None:
            payload = (line.rstrip() + "\n").encode()
            await client.write_gatt_char(NUS_RX_CHAR_UUID, payload, response=False)

        if args.once is not None:
            await send(args.once)
            await asyncio.sleep(args.timeout)
        else:
            print(f"connected — type 'help' or ^D to quit")
            loop = asyncio.get_running_loop()
            while True:
                try:
                    line = await loop.run_in_executor(None, sys.stdin.readline)
                except (EOFError, KeyboardInterrupt):
                    break
                if not line:
                    break
                await send(line)
                await asyncio.sleep(0.2)

        await client.stop_notify(NUS_TX_CHAR_UUID)


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--address", help="BLE address (skip auto-scan)")
    p.add_argument("--once", help="run a single command and exit")
    p.add_argument("--timeout", type=float, default=2.0,
                   help="seconds to wait for response after --once")
    p.add_argument("--tee", help="append all received lines to this file")
    args = p.parse_args()

    try:
        asyncio.run(run(args))
    except KeyboardInterrupt:
        sys.exit(130)


if __name__ == "__main__":
    main()
