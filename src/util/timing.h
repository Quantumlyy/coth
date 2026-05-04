/*
 * Bus-timing diagnostics.
 *
 * Exposed via the BLE "status" command. Useful both for operator sanity
 * (poll rate stable? frame rate matching expected?) and for setting the
 * inject window for v1.1+ M6/M7 active modes (we want to wait for a
 * quiet period between polls before transmitting; see spec §6 M6).
 */

#ifndef MELLON_UTIL_TIMING_H_
#define MELLON_UTIL_TIMING_H_

#include <stdint.h>

typedef struct {
	uint32_t poll_count;
	uint32_t last_poll_ms;
	uint32_t avg_interval_ms;
	uint32_t min_interval_ms;
	uint32_t max_interval_ms;
} mellon_timing_stats_t;

void mellon_timing_init(void);
void mellon_timing_record_poll(void);
void mellon_timing_get(mellon_timing_stats_t *out);

#endif /* MELLON_UTIL_TIMING_H_ */
