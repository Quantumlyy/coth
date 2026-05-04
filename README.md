# Mellon firmware

On-board firmware for the [Bishop Fox Mellon v0.1](https://github.com/BishopFox/mellon)
OSDP attack tool. Closes the upstream [issue #1](https://github.com/BishopFox/mellon/issues/1)
("no firmware exists").

> **Authorized use only.** See [`WARNING.md`](./WARNING.md) before powering this on.
> OSDP buses carry traffic on shared infrastructure and most jurisdictions
> criminalise eavesdropping on systems you do not own or have written
> authorization to test.

## What's in this build (v1)

| Mode | Behavior | Spec milestone |
|------|----------|----------------|
| `SNIFF` | Passive frame capture, BLE console hex dump | M2 |
| `KEYSET_WATCH` | Sniff + flag any `osdp_KEYSET` (CMD 0x75) and persist forever | M5 |
| `WEAK_KEYS` | Offline AES-CBC trial decrypt against captured Secure Channel ciphertext | M4 |
| `IDLE` | Quiet; BLE only | — |

Active-injection modes (`INSTALL_MODE`, `PDCAP_DOWNGRADE` — spec milestones M6
and M7) are intentionally **not** built into this image. See
[`docs/deviations.md`](./docs/deviations.md) for why.

## Hardware

- **Board:** Bishop Fox Mellon v0.1 (PCB sources in upstream repo)
- **MCU module:** Raytac MDBT42Q-512KV2 (Nordic nRF52832, Cortex-M4F @ 64 MHz, 512 KB flash, 64 KB RAM)
- **RS-485:** MAX485CSA+ half-duplex, DE+RE̅ tied to a single GPIO
- **Programming:** SWD via J-Link (J-Link EDU Mini works) or CMSIS-DAP

Pin assignments are extracted directly from the v0.1 KiCad schematic; see
spec §1.2 (in `.context/attachments/`) and `boards/mellon.overlay`.

## Toolchain

- [PlatformIO](https://platformio.org/) ≥ 6.x with the `nordicnrf52` platform
- Zephyr framework (auto-installed by PlatformIO on first build)
- A J-Link programmer (any model). CMSIS-DAP via pyOCD also works.

```sh
pip install -U platformio
pio run -e mellon                     # build firmware
pio run -e mellon -t upload           # flash via J-Link
pio test -e native_posix              # host-side unit tests
```

See [`docs/quickstart.md`](./docs/quickstart.md) for a full board-in-hand walkthrough.

## Repository layout

```
src/         firmware sources (board, OSDP, attacks, BLE, storage, util)
boards/      PlatformIO board JSON + Zephyr devicetree overlay
external/    vendored deps (libosdp; see docs/deviations.md §3)
tests/unit/  native_posix host tests
tests/hil/   manual hardware-in-the-loop walkthroughs
tools/       host-side helpers (BLE console, capture parser)
docs/        operator-facing documentation
prj.conf     Zephyr project configuration
platformio.ini  PlatformIO project + environments
```

## License

MIT, see [`LICENSE`](./LICENSE). The upstream BishopFox/mellon repo is GPL-3.0;
this is **not** a fork of it. See [`docs/deviations.md`](./docs/deviations.md) §1
for the full rationale and a re-licensing path.
