/*
 * Mellon logging — a thin wrapper around Zephyr's LOG subsystem plus a
 * forwarder that the BLE NUS console (added in M2) can subscribe to. The
 * forwarder is a small ring buffer so logs emitted while no BLE peer is
 * connected aren't lost, only bounded.
 */

#ifndef MELLON_UTIL_LOG_H_
#define MELLON_UTIL_LOG_H_

#include <stddef.h>
#include <stdint.h>

#define MELLON_LOG_RING_SIZE	1024	/* bytes; bounded forwarder for BLE */

typedef void (*mellon_log_sink_fn)(const char *line, size_t len, void *user);

/* Initialise the BLE-bridgeable forwarder. Called once at boot, before
 * Bluetooth is brought up. */
int mellon_log_init(void);

/* Register / clear a sink. The BLE console uses this to stream the most
 * recent entries when a peer connects. Only one sink is supported in v1. */
void mellon_log_set_sink(mellon_log_sink_fn fn, void *user);

/* Push a formatted line into the ring; will also emit via Zephyr LOG. */
void mellon_log_emit(const char *line, size_t len);

#endif /* MELLON_UTIL_LOG_H_ */
