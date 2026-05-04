# external/libosdp — vendoring instructions

The Mellon firmware can run two OSDP parsers in parallel: a permissive raw
parser (`src/osdp/framing.c`, always present) and the compliant
[libosdp](https://github.com/goToMain/libosdp) state machine. The compliant
parser is what gives us automatic Secure-Channel state tracking, sequence
validation, and clean PD/CP role detection. Spec §3.3.

## Soft gate

This directory is intentionally a placeholder. `src/osdp/libosdp_glue.c` is
gated behind the `HAS_LIBOSDP` compile-time macro and falls back to the raw
parser when the macro is undefined. The firmware compiles, flashes, sniffs,
captures, KEYSET-watches, and runs the weak-key trial decryptor without
libosdp.

Reasons it's not a real submodule yet:

1. Integrating libosdp under PlatformIO + Zephyr requires CMake glue that
   varies per Zephyr / libosdp version pair. Doing it once on a known-good
   pair is straightforward; doing it speculatively without hardware to
   verify is wasted churn.
2. `external/libosdp/` would push libosdp's MIT-licensed source into a
   tree the rest of which is MIT — fine — but the upstream BishopFox/mellon
   repo is GPL-3.0 and Mellon-firmware-as-a-fork would inherit that. See
   `docs/deviations.md` §1.

## How to vendor it (when the time comes)

```sh
git submodule add -b master https://github.com/goToMain/libosdp.git external/libosdp
git -C external/libosdp checkout -b mellon-hooks
# apply patches:
#   - osdp_attack_pre_pdcap_send hook
#   - osdp_attack_keyset_received hook
#   - osdp_attack_raw_frame hook
git -C external/libosdp commit -am "mellon: attack hooks"
git submodule update --init --recursive
```

Then enable in `platformio.ini`:

```ini
build_flags = ... -DHAS_LIBOSDP=1
```

`src/osdp/libosdp_glue.c` does the rest.

## Hooks

The three attack hooks the firmware expects to add to libosdp (see
spec §3.3):

| Hook | Fires | Used by |
|------|-------|---------|
| `osdp_attack_raw_frame` | every successfully-parsed frame | `attacks/sniff.c` |
| `osdp_attack_keyset_received` | on `osdp_KEYSET` (CMD 0x75) | `attacks/keyset.c` |
| `osdp_attack_pre_pdcap_send` | before CP sends a `osdp_PDCAP` reply | `attacks/downgrade.c` (M7, not in v1) |

The first two are the ones M5 needs; the third is reserved for v1.1+.
