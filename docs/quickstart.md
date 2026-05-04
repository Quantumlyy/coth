# Quickstart

The goal of this document: take you from board-in-hand to first OSDP capture
in ≤30 minutes, per spec §10.

## 0. You will need

- Mellon v0.1 board, populated and tested
- J-Link or CMSIS-DAP probe, plus 5 jumpers to J1
- A way to deliver 12 V to J3 — bench supply, or a real OSDP wall reader's
  cable (only on a bus you are authorized to test, see `WARNING.md`)
- For M2 onward: a USB↔RS-485 adapter on a Linux laptop running upstream's
  `vulnserver.py`, or a real PD/CP pair on a bench

## 1. Build and flash (M0 acceptance)

```sh
git clone <this repo>
cd <this repo>
git submodule update --init --recursive
pip install -U platformio

pio run -e mellon
pio run -e mellon -t upload
```

Open SEGGER RTT or pyOCD's RTT terminal — you should see:

```
Mellon firmware 0.1.0-dev (<short git hash>)
M0 bring-up — heartbeat only. See docs/quickstart.md.
[mellon_log] [00:00:00.012,000] <inf> board: board init ok (D3=act, D4=status)
```

Visually, **D3 (activity)** blinks once per second — short on, long off.
**D4 (status)** is solid on. **D2** is the +3.3V power-on LED and is not
firmware-controlled.

If you see this, M0 is green.

## 2. Loopback test (M1 acceptance)

Tie A and B on J2 with a short jumper, *or* connect two Mellon boards.

Drop into the BLE console (procedure in `ble_protocol.md`, available from
M2 commit onwards):

```
mellon> mode loopback
mellon> tx 53 00 09 00 00 04 ff 00 7e
RX  53 00 09 00 00 04 ff 00 7e
ok
```

A logic analyzer on P0.04 (DE) should show:
- DE rises *before* the first byte starts shifting out
- DE falls *after* the last bit of the last byte fully clocks
- DE stays low otherwise

Timing detail per spec §7.1.

## 3. First sniff (M2 acceptance)

With the Mellon connected to a bench OSDP bus and `vulnserver.py` running
on the other end:

```
mellon> mode sniff
mellon> dump 10
[t=00:01:23.456 PD->CP len=8 flags=0]  53 00 06 00 00 60 ...
[t=00:01:23.467 CP->PD len=12 flags=0] 53 80 0a 00 04 41 ...
...
ok
```

`D3` flickers per frame. `D4` stays solid.

## 4. Capture-to-flash (M3 acceptance)

```
mellon> mode sniff
mellon> capture on
mellon> status
mode=SNIFF capture=on count=2841 flash=12% used
mellon> wipe
ok
```

Pull power, wait, re-power. Reconnect over BLE. `dump` should still produce
the captured frames recorded before the power-cycle.

## 5. KEYSET watch (M5 acceptance)

```
mellon> mode keyset_watch
```

Trigger the upstream `vulnserver.py` to issue an `osdp_KEYSET`. The
captured frame is flagged "preserve" and never evicted, even when the FCB
fills. The BLE console prints a one-shot notification:

```
* KEYSET seen at t=00:14:22.103 — frame preserved (idx 47)
```

## 6. Weak-key trial decrypt (M4 acceptance)

After capturing some encrypted Secure Channel ciphertext:

```
mellon> attack weak_keys
trying 256 single-byte fills, 1 monotonic asc, 1 monotonic desc, 14 vendor defaults...
* MATCH after 4 candidates: 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04 04
ok
```

Per spec §6 M4 acceptance, this should run in the background and not block sniffing.

## Hand-off checklist

This firmware build (the one in front of you) was developed without access to
hardware. The author has verified it compiles and that host-side unit tests
pass; the operator is responsible for the on-device acceptance walkthroughs
above. Specifically, the operator should confirm:

| Milestone | What to verify on hardware |
|-----------|----------------------------|
| M0 | Boot banner shows; D3 blinks; D4 solid |
| M1 | Logic-analyzer trace shows correct DE timing; loopback round-trips |
| M2 | 30-minute soak with no watchdog reset; ≤0% frame loss at 200 Hz |
| M3 | Survives power-cycle; FCB fills and evicts cleanly |
| M5 | KEYSET frame is flagged and never evicted |
| M4 | Trial decrypt finds known-weak SCBK |

If any acceptance walkthrough fails, file an issue with: mode, console
transcript, RTT log capture, and (where applicable) logic-analyzer
screenshot.
