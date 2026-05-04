#!/usr/bin/env python3
"""
Decode a Mellon capture stream.

Two input modes:

1. **BLE dump** — `tools/ble_console.py --once 'dump 1000' --tee dump.log`
   then `parse_capture.py --ble dump.log`. The console emits ASCII-hex per
   record; we re-parse it.

2. **Raw flash dump** — if the operator extracts the capture partition
   over SWD (e.g. `nrfjprog --readuicr --readram` style), the binary is
   a stream of length-prefixed records with the layout in
   `src/storage/capture_rec.h`. Use `--bin dump.hex`.

Output: one frame per line, matching the format described in
`docs/ble_protocol.md`.
"""

from __future__ import annotations

import argparse
import struct
import sys
from typing import Iterator, BinaryIO, TextIO

CAPTURE_REC_HDR_LEN = 8
DIR_NAMES = {0: "?", 1: "cp2pd", 2: "pd2cp"}
FLAG_BITS = {
    0: "encrypted", 1: "bad_crc", 2: "bad_mac",
    3: "truncated", 4: "oversize", 5: "preserve",
}


def parse_binary(stream: BinaryIO) -> Iterator[dict]:
    while True:
        hdr = stream.read(CAPTURE_REC_HDR_LEN)
        if len(hdr) < CAPTURE_REC_HDR_LEN:
            return
        ts_us, length, direction, flags = struct.unpack("<IHBB", hdr)
        body = stream.read(length)
        if len(body) < length:
            sys.stderr.write(f"warn: truncated record at end of stream "
                             f"(needed {length}, got {len(body)})\n")
            return
        yield {
            "timestamp_us": ts_us,
            "length": length,
            "direction": direction,
            "flags": flags,
            "frame": body,
        }


def parse_ble(stream: TextIO) -> Iterator[dict]:
    """Re-parse the BLE 'dump' command output. Format per ble_protocol.md:

    [t=00:01:23.456] dir=cp2pd addr=0x00 cmd=0x60 len=8 flags=0  53 00 08 00 00 60 ?? ??
    """
    for line in stream:
        line = line.strip()
        if not line.startswith("[t="):
            continue
        try:
            head, body = line.split("  ", 1)
        except ValueError:
            continue
        parts = head.split()
        kv = {}
        for p in parts:
            if "=" in p:
                k, v = p.split("=", 1)
                kv[k.strip("[]")] = v.rstrip("]")
        try:
            frame = bytes(int(b, 16) for b in body.split())
        except ValueError:
            continue
        flags = int(kv.get("flags", "0"))
        yield {
            "timestamp_us": _decode_t(kv.get("t", "0")),
            "length": int(kv.get("len", str(len(frame)))),
            "direction": _decode_dir(kv.get("dir", "?")),
            "flags": flags,
            "frame": frame,
        }


def _decode_t(s: str) -> int:
    """HH:MM:SS.mmm → microseconds since boot (best-effort)."""
    try:
        hms, ms = s.split(".") if "." in s else (s, "0")
        h, m, sec = (int(x) for x in hms.split(":"))
        ms_int = int(ms.ljust(3, "0")[:3])
        return ((h * 3600 + m * 60 + sec) * 1000 + ms_int) * 1000
    except (ValueError, AttributeError) as e:
        sys.stderr.write(f"warn: malformed timestamp {s!r} ({e})\n")
        return 0


def _decode_dir(s: str) -> int:
    return next((k for k, v in DIR_NAMES.items() if v == s), 0)


def _flags_str(f: int) -> str:
    return ",".join(name for bit, name in FLAG_BITS.items() if f & (1 << bit)) or "-"


def _ts_str(us: int) -> str:
    s_total = us // 1_000_000
    h, rem = divmod(s_total, 3600)
    m, s = divmod(rem, 60)
    ms = (us // 1000) % 1000
    return f"{h:02d}:{m:02d}:{s:02d}.{ms:03d}"


def render(rec: dict) -> str:
    f = rec["frame"]
    addr = f[1] & 0x7F if len(f) >= 2 else 0
    cmd = f[5] if len(f) >= 6 else 0
    body_hex = " ".join(f"{b:02X}" for b in f)
    return (f"[t={_ts_str(rec['timestamp_us'])}] "
            f"dir={DIR_NAMES.get(rec['direction'], '?')} "
            f"addr=0x{addr:02X} cmd=0x{cmd:02X} "
            f"len={rec['length']} flags={_flags_str(rec['flags'])}  "
            f"{body_hex}")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--bin", type=argparse.FileType("rb"),
                   help="raw flash-dump binary")
    g.add_argument("--ble", type=argparse.FileType("r"),
                   help="text capture from BLE 'dump' command")
    args = p.parse_args()

    if args.bin:
        records = parse_binary(args.bin)
    else:
        records = parse_ble(args.ble)

    for rec in records:
        print(render(rec))


if __name__ == "__main__":
    main()
