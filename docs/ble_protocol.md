# BLE protocol — v1 (NUS console)

The v1 transport is a line-oriented ASCII command channel running over
Nordic UART Service. v2+ adds a structured GATT service (spec §3.5)
layered on top of the same dispatcher.

## Discovery

- Device name: `Mellon`
- Advertised service UUID: NUS — `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- Advertising interval: ~1 Hz default (room to be reachable, no battery
  budget concern; see spec §7.4)

## Pairing

**Passkey-only**, six digits. The default passkey is `123456` (built into
the firmware so the operator can connect on first boot). The passkey is
persisted in NVS and can be changed via the `passkey` command (TODO:
not yet wired in this commit; tracked as a follow-up).

The firmware will **never** advertise "just-works" pairing. If your client
shows "no PIN required", something is wrong — check the build flags.

## Wire format

- One command per line, terminated by `\n` (CR is stripped, so `\r\n` works too)
- Responses end with `\n`
- Multi-line responses end with `ok` or `err: <reason>` on the final line

## Commands

| Command | Args | Response | Status |
|---------|------|----------|--------|
| `help` | — | command list | always |
| `status` | — | build, mode, frame counters, poll stats, libosdp present | always |
| `mode <name>` | `idle\|sniff\|keyset_watch\|weak_keys` (active modes return err) | `ok mode=<name>` | M2+ |
| `dump <n>` | count | `<n>` lines, one per captured frame | M3 |
| `preserved` | — | one line per NVS-preserved (KEYSET) frame | M5 |
| `capture` | `on\|off` | `ok` | M3 |
| `wipe` | — | `ok` (clears FCB **and** preserved slots) | M3 |
| `keys` | — | `addr=0xNN sc=y/n scbk=captured/-` per known PD | M2+ |
| `attack weak_keys` | — | progress lines, `* MATCH …` on hit, `ok` or `err: not found` | M4 |
| `arm <mode> <ttl>` | mode name + TTL seconds (≤300) | `ok armed nonce=NNNNNNNN` | M6/M7 (disabled) |
| `fire <mode> <nonce>` | mode + matching nonce | `ok` or `err: not armed` | M6/M7 (disabled) |

## Response format examples

```
mellon> status
build:    0.1.0-dev (a1b2c3d)
uptime:   00:14:22
mode:     sniff
frames:   total=2841 bad_crc=2 resync=0 oversize=0
polls:    n=2823 avg=10ms min=8ms max=21ms
libosdp:  absent
ok
```

```
mellon> mode keyset_watch
ok mode=keyset_watch
```

```
mellon> mode install_mode
err: install_mode — v1.1/v2 — not implemented in this build
```

## Frame dump format (M3)

One frame per line, intended to be `awk`-able from the Python helper:

```
[t=00:01:23.456] dir=cp2pd addr=0x00 cmd=0x60 len=8 flags=0  53 00 08 00 00 60 ?? ??
```

Fields:

- `t=`  uptime when the frame was captured (HH:MM:SS.mmm)
- `dir=`  `cp2pd`, `pd2cp`, or `?`
- `addr=`  byte 1 (high bit cleared in display)
- `cmd=`  byte 5 (command or reply code)
- `len=`  total frame length
- `flags=`  bitmask: 1=encrypted, 2=bad_crc, 4=bad_mac, 8=truncated, 16=oversize
- raw bytes follow as space-separated hex

## Asynchronous notifications

Some events stream asynchronously (no command required):

- `* KEYSET seen at t=… addr=0xNN scbk=…` — KEYSET watcher hit (M5)
- `* MATCH after N candidates: ……` — weak-key decryptor hit (M4)
- `* ARMED expired` — arming TTL fired without a `fire` (M6/M7)

These start with `*` so the operator (and the Python tool) can filter them.

## Authentication / authorisation

There is exactly one authentication mechanism: BLE passkey pairing. There
is no per-command auth — once paired, a peer can issue any command. This
is appropriate for v1's threat model (operator's phone in the same room
as the board on a bench). For v2 we should:

- Pin a whitelist of long-term keys
- Require the magic value on `wipe`
- Rate-limit `attack weak_keys` so a hostile peer can't pin the CPU

These are tracked as v2 work.
