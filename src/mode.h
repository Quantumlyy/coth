/*
 * Mode dispatcher.
 *
 * Modes are mutually exclusive. Switching flushes pending TX in the
 * RS-485 layer, resets the framer, and persists the new mode to NVS so
 * it survives a power-cycle (spec §3.2).
 *
 * Active modes (M6 INSTALL_MODE, M7 PDCAP_DOWNGRADE) are not implemented
 * in this build; mellon_mode_set() returns -ENOSYS for them. The
 * arming state machine is still present, so the BLE protocol's
 * arm/fire commands behave consistently across builds.
 */

#ifndef MELLON_MODE_H_
#define MELLON_MODE_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	MODE_IDLE		= 0,
	MODE_SNIFF		= 1,
	MODE_KEYSET_WATCH	= 2,
	MODE_WEAK_KEYS		= 3,
	MODE_INSTALL_MODE	= 4,	/* not implemented in v1 */
	MODE_PDCAP_DOWNGRADE	= 5,	/* not implemented in v1 */
	MODE_COUNT
} mellon_mode_t;

const char *mellon_mode_name(mellon_mode_t m);
int mellon_mode_from_name(const char *name, mellon_mode_t *out);

/* Top-level init: NVS, sniff, BLE, etc. Reads the persisted mode and
 * activates it. */
int mellon_mode_init(void);

/* Switch to a new mode. Returns 0 on success, -EINVAL for unknown mode,
 * -ENOSYS for unimplemented modes (INSTALL_MODE, PDCAP_DOWNGRADE). */
int mellon_mode_set(mellon_mode_t mode);

mellon_mode_t mellon_mode_get(void);

/*
 * Active-mode arming (spec §3.2 — 60 second TTL).
 * Currently only relevant for M6/M7 which are out of scope; the state
 * machine is here so the BLE protocol behaves consistently and the
 * v1.1+ branch slot is in place.
 */
int mellon_mode_arm(mellon_mode_t mode, uint32_t ttl_sec, uint32_t nonce);
int mellon_mode_fire(mellon_mode_t mode, uint32_t nonce);
bool mellon_mode_is_armed(mellon_mode_t mode);

#endif /* MELLON_MODE_H_ */
