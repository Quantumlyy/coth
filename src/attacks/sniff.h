/*
 * Passive sniff mode (spec §6 M2).
 *
 * Wires UART RX bytes through the permissive framer, the libosdp glue, the
 * Secure-Channel observer, and out to two consumers:
 *
 *   1. The flash capture (M3) — if enabled.
 *   2. The BLE console (M2 commit 6) — for live hex-dump streaming.
 *
 * Sniff is the default mode after first boot per spec §3.2.
 */

#ifndef MELLON_ATTACKS_SNIFF_H_
#define MELLON_ATTACKS_SNIFF_H_

#include <stdbool.h>
#include "../osdp/framing.h"

typedef void (*mellon_sniff_frame_fn)(const osdp_frame_t *frame, void *user);

int mellon_sniff_init(void);

/*
 * Subscribe a downstream consumer (capture / BLE console / KEYSET watcher).
 * Multiple subscribers are supported up to MELLON_SNIFF_MAX_SUBS.
 */
int mellon_sniff_subscribe(mellon_sniff_frame_fn fn, void *user);

/* Toggle sniffing on/off without unsubscribing handlers. Used by mode
 * dispatcher when switching modes mid-run. */
void mellon_sniff_set_active(bool active);
bool mellon_sniff_is_active(void);

/* Diagnostics for the BLE "status" command. */
typedef struct {
	uint32_t frames_total;
	uint32_t frames_bad_crc;
	uint32_t resync_count;
	uint32_t oversize_count;
} mellon_sniff_stats_t;

void mellon_sniff_get_stats(mellon_sniff_stats_t *out);

#endif /* MELLON_ATTACKS_SNIFF_H_ */
