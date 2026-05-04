# Attacks

This document describes the attack modules that ship in v1 of the Mellon
firmware. Each module corresponds to one of the five attacks described in
Bishop Fox's Black Hat 2023 OSDP research and listed in spec §0.

The two **active**-injection attacks (M6 install-mode impersonation, M7
PDCAP downgrade) are not implemented in this build — see
`docs/deviations.md` §2.

## SNIFF (M2)

The default mode after first boot. Listens on the OSDP wire, parses every
frame the bus carries (including ones that fail CRC), and surfaces them
to the BLE console. This is the foundational mode all the others rely on.

Acceptance: spec §6 M2.

## KEYSET_WATCH (M5)

Spec §0 attack #5. Watches for `osdp_KEYSET` (CMD = 0x75) frames sent
CP→PD during install mode. The KEYSET payload contains the SCBK in the
clear, so a passive sniffer that happens to be on the bus during
commissioning gets the install-time key for free.

The watcher:

1. Subscribes to the sniff pipeline.
2. Matches CP→PD frames where `cmd_or_reply == 0x75` and the payload is
   shaped as `[0x01, 0x10, key[16]]` (key type SCBK, length 16).
3. Marks the most-recently-recorded capture entry with
   `CAPTURE_FLAG_PRESERVE` and copies it into one of 8 NVS slots so it
   survives FCB rotation **and** power-cycle.
4. Emits `* KEYSET seen at t=… addr=0xXX scbk=…` over the BLE console.

Reachable from the operator side via the `mode keyset_watch` and
`preserved` BLE commands.

## WEAK_KEYS (M4)

Spec §0 attack #4. Offline trial decryption against captured Secure
Channel ciphertext, intended for cases where the operator missed the
install-mode KEYSET window but suspects the SCBK is one of:

- A single-byte fill (`0x00 16`, `0x04 16`, `0xFF 16`, …)
- A monotonic sequence (`0x00 0x01 … 0x0F` or its reverse)
- A documented vendor / install default (see `weakkeys_keys.c`)

**Algorithm:**

1. Walk the FCB log; pull all entries whose `flags & CAPTURE_FLAG_ENCRYPTED`.
   Up to MAX_PROBE_FRAMES (256) are sampled.
2. Build the candidate list (256 + 2 + vendor defaults).
3. For each candidate, AES-CBC-decrypt the first 16-byte block past the
   OSDP header of each sampled frame using a zero IV (heuristic — see
   "Caveats" below).
4. Score each plaintext via `weakkeys_score.c`:
   - +45 if first byte is in the plausible OSDP command/reply alphabet (0x40–0x7F)
   - +15 if first byte equals the cleartext-header CMD byte
   - +30 if the 16-byte block has ≤ 8 unique byte values, +15 if ≤ 11
5. ≥ 60 = match. Stop on first match. Report key + frame index over BLE.

**Operator path:**

```
mellon> mode weak_keys
ok mode=weak_keys
mellon> attack weak_keys
attack weak_keys: 24 encrypted frames sampled
trying 272 candidates...
* MATCH after 5 candidates: score=90 name=- key=04040404040404040404040404040404 (frame idx 11)
ok
```

**Caveats:**

- The IV used in step 3 is zeros, not the session-derived IV that real
  OSDP SC requires. This works because the *first* AES block of a
  CBC-mode ciphertext, decrypted with the wrong IV, still produces
  recoverable plaintext after the IV-XOR step would normally be applied;
  for a heuristic plausibility check that only looks at the first byte
  and entropy, this approximation is sufficient.

- Real session-aware decryption requires the RNDA/RNDB nonces exchanged
  during SC establishment. With `HAS_LIBOSDP`, the libosdp glue tracks
  these and the decryptor would use them; without it, we fall back to the
  zero-IV heuristic. See `docs/deviations.md` §3.

- Plausibility scoring is heuristic. False positives are possible at low
  match thresholds. The threshold of 60 was chosen so that the scorer
  rejects every candidate against truly random data while accepting
  real OSDP plaintexts in synthetic test fixtures.

## Mode interlocks

Modes are mutually exclusive. The dispatcher in `src/mode.c` enforces:

| From mode | To mode | Behavior |
|-----------|---------|----------|
| Any | `idle` | Sniff disabled; BLE stays up |
| Any | `sniff` / `keyset_watch` / `weak_keys` | Sniff enabled; framer reset |
| Any | `install_mode` / `pdcap_downgrade` | Returns `-ENOSYS` |

`weak_keys` mode keeps sniffing active in the background — the trial
decryptor itself runs as a Zephyr work item on the system workqueue and
yields every 32 candidates so BLE / sniff aren't starved.
