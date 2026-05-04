# M1 hardware-in-the-loop: RS-485 loopback

The host-side `tests/unit/test_rs485_loopback.c` only exercises the DE
state machine. The full driver — including baud-rate timing, RX framing,
and the actual MAX485 — needs hardware. This document is the operator's
walkthrough to clear M1 acceptance (spec §6 M1).

## Setup

| Item | Why |
|------|-----|
| Mellon board flashed with current firmware | UUT |
| 12 V supply on J3 | parasitic power |
| Logic analyzer (Saleae Logic 8 / DSLogic / cheap clone) | observe DE timing |
| Either: short jumper A↔B on J2, *or* a second Mellon board | loopback path |
| J-Link / CMSIS-DAP | flashing & RTT |

## Wiring

### Single-board loopback (preferred)

Tie J2 pin 1 (B) to J2 pin 2 (A) with a short jumper. The MAX485's own
output drives its own input. The fail-safe bias resistors (R3, R4) keep
the bus from floating when DE is low.

### Two-board loopback (alternative)

Wire board A's J2 pin 1 to board B's J2 pin 1, A's pin 2 to B's pin 2.
Configure board B as `mode idle` (DE permanently low) and board A as the
active TX side.

## Logic-analyzer probe points

| Probe | Pin |
|-------|-----|
| Channel 1 | DE (P0.04 — also accessible on the MAX485 DE pin 3) |
| Channel 2 | DI (P0.06 — UART TX) |
| Channel 3 | RO (P0.08 — UART RX) |
| Channel 4 | A or B on J2 (differential view requires two channels; one is enough for sanity) |

Set sample rate ≥ 4× baud × 8 (so for 9600 baud, ≥ 308 kHz; for 230400
baud, ≥ 7.4 MHz).

## Acceptance walkthrough

Per spec §6 M1, all four bullets must be green.

### Bullet 1: DE direction control

Open the BLE NUS console (M2+) or use SWD-attached `pio device monitor`.
With the firmware built with `mode loopback` (TODO: gate this in the
mode dispatcher; not yet wired in M1), trigger a single TX:

```
mellon> tx 53 00 09 00 00 04 ff 00 7e
```

Logic-analyzer trace must show:

```
DI:  ___|‾‾‾‾‾‾byte0‾‾‾‾‾‾|‾‾‾‾‾‾byte1‾‾‾‾‾‾|...|‾‾‾‾‾‾byteN‾‾‾‾‾‾|___
DE:  ____|‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾|____
        ^DE rises before first start bit                   DE falls after last stop bit
```

**Fail conditions (each is a bug to file):**

- DE rises *during* byte 0 (race: tx_begin must precede uart_tx)
- DE falls *during* the last byte's stop bit (off-by-one: TX_DONE didn't fire)
- DE falls before the last bit (using BYTE_WRITTEN instead of TX_DONE)

### Bullet 2: Last byte not truncated

Capture the loopback output on RO with the analyzer. The captured byte
sequence must equal the transmitted sequence exactly. If the last byte is
0x?? where ? is any nibble of the previous byte, the DE timing is the
classic off-by-one bug.

### Bullet 3: All baud rates

Run the loopback at 9600, 19200, 38400, 57600, 115200, 230400. Each must
produce a clean RX. The RS-485 driver should reconfigure cleanly between
rates without resetting the rest of the firmware.

### Bullet 4: No bus contention with two-board loopback

Connect two boards. Set both to `mode idle`. On a logic-analyzer trace
of A and B (differential), observe a steady idle state — no spurious
toggling. Then activate one board, leaving the other idle. Only the
active board should drive the bus.

## Common failures and fixes

| Symptom | Likely cause |
|---------|-------------|
| Last byte garbled | DE deasserted on byte-written instead of TX_DONE; check `uart_callback()` in rs485.c |
| Nothing on RX | DE stuck HIGH (transceiver in TX mode permanently); check `pin_set` callback in `rs485_fsm.c` |
| Random byte drops | RX timeout too short; bump `RX_TIMEOUT_US` |
| Crash after a few TX | TX_DONE semaphore not drained; check `rs485_fsm_tx_done` ordering |
| Inverted bus polarity | A and B swapped on J2; check schematic, not firmware |

## Sign-off

Operator signs M1 by attaching to the issue tracker:
- Logic-analyzer screenshot showing all four bullets green
- Build hash from RTT boot banner
- Probe used (J-Link / CMSIS-DAP) and probe firmware version
