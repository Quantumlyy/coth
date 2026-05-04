/*
 * osdp_KEYSET watcher (spec §6 M5).
 *
 * Passive: subscribes to the sniff pipeline, flags any matching frame
 * with CAPTURE_FLAG_PRESERVE so it survives FCB eviction, and emits a
 * one-shot BLE notification on the console.
 *
 * Active impersonation (spec M6 install_mode) is the v1.1+ delivery — see
 * docs/deviations.md §2.
 */

#ifndef MELLON_ATTACKS_KEYSET_H_
#define MELLON_ATTACKS_KEYSET_H_

#include <stdbool.h>
#include <stdint.h>

#include "keyset_match.h"

int mellon_keyset_watcher_init(void);

typedef struct {
	uint32_t hits_total;	/* matched KEYSET frames since boot */
	uint32_t last_seen_ms;	/* uptime when last hit */
	uint8_t last_address;
} mellon_keyset_stats_t;

void mellon_keyset_get_stats(mellon_keyset_stats_t *out);

/* Last captured SCBK + address (NULL if none). Pointer is stable until the
 * next match. */
const keyset_match_t *mellon_keyset_get_last(void);

#endif /* MELLON_ATTACKS_KEYSET_H_ */
