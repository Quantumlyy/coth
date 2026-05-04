/*
 * Offline weak-key trial decryptor (spec §6 M4).
 *
 * Walks the captured-frame log and tries each entry in
 * weakkeys_keys.h's candidate list against any encrypted body it finds.
 * Match criterion: plausibility score ≥ MELLON_WEAK_SCORE_MIN_MATCH
 * (see weakkeys_score.h).
 *
 * Runs as a background work item so it doesn't block sniffing or BLE.
 * Stops on first match; reports key + matching frame index over BLE.
 */

#ifndef MELLON_WEAKKEYS_H_
#define MELLON_WEAKKEYS_H_

#include <stdbool.h>
#include <stdint.h>

#include "weakkeys_keys.h"

int mellon_weakkeys_init(void);

/*
 * Kick off a trial decryption pass. Non-blocking; results stream over
 * the BLE console. Returns 0 if the pass is now scheduled, -EBUSY if
 * one is already in flight, or other negative on error.
 */
int mellon_weakkeys_start(void);

/* Cancel an in-flight pass. Returns 0 if cancelled, -ENOENT if idle. */
int mellon_weakkeys_cancel(void);

typedef struct {
	bool running;
	uint32_t candidates_tried;
	uint32_t frames_attempted;
	bool match_found;
	uint8_t match_key[MELLON_WEAK_KEY_LEN];
	uint8_t match_score;
	const char *match_name;
} mellon_weakkeys_status_t;

void mellon_weakkeys_get_status(mellon_weakkeys_status_t *out);

#endif /* MELLON_WEAKKEYS_H_ */
