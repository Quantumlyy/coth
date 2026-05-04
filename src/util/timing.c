#include "timing.h"

#include <string.h>
#include <zephyr/kernel.h>

static mellon_timing_stats_t g_stats;

void mellon_timing_init(void)
{
	memset(&g_stats, 0, sizeof(g_stats));
	g_stats.min_interval_ms = UINT32_MAX;
}

void mellon_timing_record_poll(void)
{
	uint32_t now = k_uptime_get_32();

	if (g_stats.poll_count > 0) {
		uint32_t delta = now - g_stats.last_poll_ms;
		if (delta < g_stats.min_interval_ms) {
			g_stats.min_interval_ms = delta;
		}
		if (delta > g_stats.max_interval_ms) {
			g_stats.max_interval_ms = delta;
		}
		/*
		 * Streaming average without storing history: equivalent to
		 *   avg <- avg + (delta - avg) / N
		 * for the new N=poll_count.
		 */
		g_stats.avg_interval_ms +=
			(delta - g_stats.avg_interval_ms) / g_stats.poll_count;
	}
	g_stats.last_poll_ms = now;
	g_stats.poll_count++;
}

void mellon_timing_get(mellon_timing_stats_t *out)
{
	*out = g_stats;
}
