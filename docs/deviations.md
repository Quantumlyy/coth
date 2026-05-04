# Deviations from the build spec

This implementation follows `Mellon Firmware — Build Spec v0.1` (in
`.context/attachments/`) closely, but a handful of decisions intentionally
diverge from it. Each is recorded here so future contributors and any
downstream PR to BishopFox/mellon can reconcile them.

## 1. License: MIT, not GPL-3.0

**Spec §0** mandates GPL-3.0 to match the upstream BishopFox/mellon repo.
This repo is MIT-licensed (see `LICENSE`) — chosen by the original author for
his personal-fork workflow. The repo is **not** a fork of BishopFox/mellon and
is not currently planned to be merged upstream as-is.

If anyone wants to take this upstream:

- The firmware code in `src/`, `tests/`, `tools/` is original work and can be
  re-licensed by the author at that time.
- `external/libosdp/` is vendored under its own license (MIT — see that
  submodule's `LICENSE`) and remains MIT regardless of what we do to the
  surrounding tree.
- A re-license commit is straightforward: rewrite `LICENSE`, update the
  per-file SPDX headers, open a PR against `BishopFox/mellon`.

## 2. Active-injection modes (M6, M7) are not implemented

**Spec §6 milestones M6 and M7** describe install-mode impersonation and PDCAP
downgrade — both of which actively transmit forged OSDP frames onto a live bus.

This build ships **passive and offline modes only**: M0 bring-up, M1 RS-485
loopback, M2 sniffer + BLE console, M3 capture-to-flash, M4 weak-key trial
decryptor, M5 KEYSET watcher.

The mode dispatcher in `src/main.c` reserves enum values for `MODE_INSTALL_MODE`
and `MODE_PDCAP_DOWNGRADE` and accepts them via the BLE console, but they
return `-ENOSYS` and the console replies `"v1.1 / v2 — not implemented in this
build"`. The arming/fire state machine *is* present, so M6/M7 can be added
later without restructuring the dispatcher.

## 3. libosdp integration is soft-gated

**Spec §2.4** lists libosdp as required. Integrating it under PlatformIO +
Zephyr is non-trivial (`external/libosdp/` needs CMake + Kconfig glue that
varies per Zephyr version).

To keep the firmware compilable in environments where libosdp's Zephyr glue
isn't fully wired up, `src/osdp/libosdp_glue.c` is gated behind
`#ifdef HAS_LIBOSDP`. When the macro is undefined, the firmware falls back to
the permissive raw parser in `src/osdp/framing.c` alone, which is sufficient
for sniff / capture / KEYSET-watch / weak-key modes. Secure Channel state
tracking degrades from "decode in real time" to "log encrypted frames as
opaque blobs"; the M4 weak-key decryptor still works on the captured
ciphertext after the fact.

The full HAS_LIBOSDP path is reserved for a follow-up commit once we have
hardware to verify the build against.

## 4. No PR opened against BishopFox/mellon

**Spec §10** lists "Pull request opened against upstream BishopFox/mellon
referencing issue #1" as a definition-of-done item. Skipped here because of
deviation #1 above; an upstream PR would need a re-licensing commit and a
maintainer-side import strategy that isn't this author's call.

## 5. BLE NUS depends on Zephyr's services/nus

`src/ble/nus_console.c` calls `<zephyr/bluetooth/services/nus.h>` (the
Nordic UART Service). This header has shipped in mainline Zephyr since
~3.5, but vendored via Nordic's NCS for years before that.

If your PlatformIO Zephyr version is older than 3.5 or lacks the NUS
subsystem entirely, two paths exist:

- **Upgrade**: bump the `nordicnrf52` PlatformIO platform to a release
  that pulls in mainline Zephyr ≥ 3.5. Verify with `pio system info`.
- **Replace**: implement a minimal NUS GATT service inline in
  `src/ble/nus_console.c` using the universal Zephyr GATT primitives.
  ~80 lines; the wire protocol stays identical (same UUIDs).

The same caveat applies to `BT_LE_ADV_CONN_NAME` (used in
`bt_le_adv_start` setup); Zephyr 3.7+ deprecates it in favour of
`BT_LE_ADV_CONN_FAST_2` plus an explicit `bt_set_name()` call. Adjust
locally if your version warns or errors.

## 6. PlatformIO + Zephyr layout follows spec literally

**Spec §9** places `prj.conf` and `boards/*.overlay` at the repo root. PlatformIO
6.x's Zephyr framework historically expected them under `zephyr/`. Modern
PlatformIO accepts the root layout via `framework = zephyr` plus a `boards_dir`
hint. If your PlatformIO version misbehaves, copy `prj.conf` and `boards/` into
a `zephyr/` subdirectory; no source change is required.

This is documented here rather than fixed in tree because the spec was
explicit about the layout, and forcing the operator's PlatformIO version to a
working set is more useful than masking the version mismatch.
