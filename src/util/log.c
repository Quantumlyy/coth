#include "log.h"

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

LOG_MODULE_REGISTER(mellon_log, CONFIG_LOG_DEFAULT_LEVEL);

RING_BUF_DECLARE(log_ring, MELLON_LOG_RING_SIZE);

static struct {
	mellon_log_sink_fn fn;
	void *user;
} g_sink;

static K_MUTEX_DEFINE(log_lock);

int mellon_log_init(void)
{
	g_sink.fn = NULL;
	g_sink.user = NULL;
	ring_buf_reset(&log_ring);
	return 0;
}

void mellon_log_set_sink(mellon_log_sink_fn fn, void *user)
{
	k_mutex_lock(&log_lock, K_FOREVER);
	g_sink.fn = fn;
	g_sink.user = user;
	k_mutex_unlock(&log_lock);
}

void mellon_log_emit(const char *line, size_t len)
{
	if (len == 0 || line == NULL) {
		return;
	}
	/* Forward to RTT/printk for native console viewing. */
	LOG_INF("%.*s", (int)len, line);

	k_mutex_lock(&log_lock, K_FOREVER);
	mellon_log_sink_fn sink = g_sink.fn;
	void *user = g_sink.user;

	/* Bounded ring: drop from the front to make room rather than spinning. */
	while (ring_buf_space_get(&log_ring) < len + 1) {
		uint8_t discard;
		if (ring_buf_get(&log_ring, &discard, 1) == 0) {
			break;
		}
	}
	ring_buf_put(&log_ring, (const uint8_t *)line, len);
	uint8_t nl = '\n';
	ring_buf_put(&log_ring, &nl, 1);
	k_mutex_unlock(&log_lock);

	if (sink != NULL) {
		sink(line, len, user);
	}
}
