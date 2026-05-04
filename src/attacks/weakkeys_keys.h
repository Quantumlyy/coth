/*
 * Weak-key candidate list for M4 (spec §6 M4).
 *
 * Three families:
 *   1. Single-byte fills 0x00..0xFF (256 entries). Catches the canonical
 *      "deployed without setting an SCBK" misconfiguration.
 *   2. Monotonic ascending (0x00..0x0F) and descending (0xFF..0xF0).
 *   3. Documented vendor defaults — small list, expand as deployments
 *      reveal more.
 *
 * The list lives in flash (PROGMEM-equivalent on Zephyr — `const`).
 *
 * IMPORTANT: this module does not store keys it has *recovered*. Those
 * stay in src/osdp/secchan.c and are reachable via the BLE `keys`
 * command. This list is just the dictionary the trial-decryptor walks.
 */

#ifndef MELLON_WEAKKEYS_KEYS_H_
#define MELLON_WEAKKEYS_KEYS_H_

#include <stddef.h>
#include <stdint.h>

#define MELLON_WEAK_KEY_LEN	16

typedef struct {
	const char *name;
	uint8_t key[MELLON_WEAK_KEY_LEN];
} mellon_weak_key_t;

/*
 * Build a candidate list into `out`. Returns the number of entries
 * written. `out` must be at least MELLON_WEAK_KEY_CANDIDATE_MAX entries.
 *
 * Why a builder rather than a static table: the 256 single-byte fills
 * are repetitive enough to compute on demand and save 256 * 16 = 4 KiB
 * of flash, which is significant on a 512 KiB part.
 */
#define MELLON_WEAK_KEY_VENDOR_COUNT	14
#define MELLON_WEAK_KEY_CANDIDATE_MAX	(256 + 2 + MELLON_WEAK_KEY_VENDOR_COUNT)

size_t mellon_weak_keys_build(mellon_weak_key_t *out, size_t cap);

#endif /* MELLON_WEAKKEYS_KEYS_H_ */
