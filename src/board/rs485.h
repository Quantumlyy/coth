/*
 * Mellon RS-485 half-duplex driver.
 *
 * Owns the MAX485 transceiver via UART0 + GPIO P0.04 (DE+RE̅). All "active"
 * attack modes go through this; sniff mode never calls send().
 */

#ifndef MELLON_RS485_H_
#define MELLON_RS485_H_

#include <stddef.h>
#include <stdint.h>

typedef void (*mellon_rs485_rx_fn)(const uint8_t *bytes, size_t len, void *user);

/*
 * Initialise UART0 + DE GPIO. Defaults to 9600 baud (OSDP standard).
 * The board comes up in RX direction.
 */
int mellon_rs485_init(uint32_t baud);

/* Reconfigure baud at runtime. Spec §1.6 supports up to 230400. */
int mellon_rs485_set_baud(uint32_t baud);

/*
 * Synchronously transmit `len` bytes. Raises DE before the first byte,
 * blocks until the UART async layer fires UART_TX_DONE, then drops DE.
 *
 * Returns 0 on success, negative on error. Caller must hold no locks
 * that could deadlock against the UART callback.
 */
int mellon_rs485_send(const uint8_t *data, size_t len);

/*
 * Register a handler for received bytes. Called from the UART async
 * callback context (system workqueue), so the handler should not block.
 *
 * The handler pointer is read lock-free from the UART callback. On Cortex-M
 * an aligned pointer store is atomic, so concurrent set+read does not tear,
 * but no memory barrier is issued — set the handler from a single thread,
 * once, before the bus carries traffic the caller cares about. Bytes that
 * arrive while no handler is registered are dropped silently.
 */
void mellon_rs485_set_rx_handler(mellon_rs485_rx_fn fn, void *user);

#endif /* MELLON_RS485_H_ */
