# Flashing

The Mellon module exposes only SWD; there is no USB or UART bootloader.
You'll need a debug probe and the firmware `.hex` from `pio run`.

## Required hardware

- Mellon board (J1 SWD header populated — Nucleo-style 5-pin pinout: VCC, SWCLK, GND, SWDIO, NRST)
- A debug probe — one of:
  - **J-Link** (any model; J-Link EDU Mini at ~$20 is sufficient)
  - **CMSIS-DAP** (e.g. DAPLink, Picoprobe) — works via pyOCD
- A 5-wire jumper from probe to J1
- 12 V supply on J3 *or* a bench supply applied to the +3.3V rail of the MDBT42Q

The board is normally powered parasitically off the OSDP cabling's 12 V rail.
For bench bring-up before you wire it to a real bus, feed 12 V into J3 from
a lab supply.

## J-Link path (default)

```sh
pip install -U platformio          # if not already installed
pio run -e mellon                  # build firmware -> .pio/build/mellon/firmware.hex
pio run -e mellon -t upload        # flashes via J-Link automatically
```

To watch logs live, point SEGGER's RTT viewer at the device:

```sh
JLinkRTTViewer
# Device: nRF52832_xxAA, Interface: SWD, Speed: 4000 kHz
```

The boot banner emits the firmware version and short git hash, e.g.

```
Mellon firmware 0.1.0-dev (a1b2c3d)
M0 bring-up — heartbeat only. See docs/quickstart.md.
```

## CMSIS-DAP path

```sh
pip install pyocd
pyocd flash -t nrf52832 .pio/build/mellon/firmware.hex
```

CMSIS-DAP probes typically expose RTT through pyOCD's `gdbserver`:

```sh
pyocd gdbserver -t nrf52832 --rtt
telnet localhost 19021    # RTT terminal
```

## Erase / recovery

If the chip becomes unresponsive (rare, but possible during BLE config
experiments), the SWD interface can always recover it:

```sh
# J-Link
JLinkExe -device nrf52832_xxaa -if swd -speed 4000
> erase
> exit
pio run -e mellon -t upload
```

```sh
# pyOCD
pyocd erase -t nrf52832 --chip
```

There is no factory-reset button on the board — recovery is always via SWD.
This is documented in spec §1.6.

## Power-cycling without re-flashing

Toggle 12 V on J3, or trigger the SWD reset line via the probe (`monitor reset`
from gdb / J-Link `r` command). NRST is wired to J1 pin 5, so the probe can
drive it directly.
