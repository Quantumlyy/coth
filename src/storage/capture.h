/*
 * Capture-to-flash log.
 *
 * Subscribes to the sniff pipeline; persists every frame seen (when
 * active=true) to the FCB-backed `capture` flash partition. Survives
 * power-cycle. Spec §5.
 *
 * Storage region (set by boards/mellon.overlay):
 *   capture_partition: 0x78000–0x7FFFF, 32 KB, 8 sectors of 4 KB each.
 *
 * Eviction policy: oldest sector is erased when the head reaches it.
 * Records flagged CAPTURE_FLAG_PRESERVE are guarded against eviction
 * via the M5 KEYSET-watcher hook (commit 8).
 */

#ifndef MELLON_CAPTURE_H_
#define MELLON_CAPTURE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../osdp/framing.h"
#include "capture_rec.h"

int mellon_capture_init(void);

void mellon_capture_set_active(bool active);
bool mellon_capture_is_active(void);

int mellon_capture_record(const osdp_frame_t *frame);

/*
 * Mark the most-recently-recorded frame with CAPTURE_FLAG_PRESERVE so it
 * survives eviction. Called by the KEYSET watcher in M5.
 */
int mellon_capture_preserve_last(void);

/*
 * Walk records oldest→newest, calling `cb` for each up to `max_records`.
 * `cb` returns non-zero to stop iteration early. Pointer into the
 * read buffer is valid only for the duration of the callback.
 */
typedef int (*mellon_capture_walk_cb)(const capture_rec_hdr_t *hdr,
				      const uint8_t *frame, void *user);

int mellon_capture_walk(mellon_capture_walk_cb cb, void *user, size_t max_records);

/* Erase all records. Used by the BLE `wipe` command. */
int mellon_capture_wipe(void);

/* Diagnostics for the BLE `status` command. */
typedef struct {
	uint32_t records_total;		/* recorded since boot */
	uint32_t records_evicted;
	uint32_t records_preserved;
	uint32_t bytes_used_estimate;
} mellon_capture_stats_t;

void mellon_capture_get_stats(mellon_capture_stats_t *out);

#endif /* MELLON_CAPTURE_H_ */
