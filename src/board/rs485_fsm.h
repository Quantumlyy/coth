/*
 * RS-485 DE/RE̅ direction-control FSM (pure logic, host-testable).
 *
 * The MAX485 on Mellon v0.1 has DE and RE̅ tied to a single GPIO (P0.04).
 * HIGH = transmit, LOW = receive. The #1 bug in half-duplex RS-485 is
 * deasserting DE too early (truncating the last TX byte) or too late
 * (colliding with an incoming frame).
 *
 * This FSM tracks the intended DE state and exposes the transitions that
 * the Zephyr-bound driver in rs485.c invokes from the UART async callback.
 * Keeping it free of Zephyr deps lets us unit-test the timing rules on
 * native_posix without a real UART. See spec §7.1.
 */

#ifndef MELLON_RS485_FSM_H_
#define MELLON_RS485_FSM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
	RS485_DIR_RX = 0,	/* idle / receiving — DE LOW */
	RS485_DIR_TX,		/* TX in flight — DE HIGH */
} rs485_dir_t;

typedef void (*rs485_pin_set_fn)(bool high, void *user);

typedef struct {
	rs485_pin_set_fn pin_set;
	void *user;
	rs485_dir_t dir;
	uint32_t tx_done_count;
	uint32_t tx_aborted_count;
	uint32_t late_tx_done_count;	/* tx_done arrived without a tx_begin */
} rs485_fsm_t;

/* Initialise. After this DE is forced LOW (RX direction). */
void rs485_fsm_init(rs485_fsm_t *fsm, rs485_pin_set_fn pin_set, void *user);

/*
 * Caller is about to invoke uart_tx(). Raise DE before any byte goes out.
 * Returns 0 on the first-of-a-batch transition, 1 if already TX-direction
 * (the caller may chain multiple frames without a round-trip through RX),
 * negative on error.
 */
int rs485_fsm_tx_begin(rs485_fsm_t *fsm);

/*
 * Called from the UART async callback on UART_TX_DONE / EVENTS_TXSTOPPED
 * (NOT on byte-written). Drops DE.
 */
void rs485_fsm_tx_done(rs485_fsm_t *fsm);

/*
 * Called from the UART async callback on UART_TX_ABORTED. Drops DE so a
 * partial transmit doesn't pin the bus.
 */
void rs485_fsm_tx_aborted(rs485_fsm_t *fsm);

/* Current direction (introspection / tests). */
rs485_dir_t rs485_fsm_dir(const rs485_fsm_t *fsm);

#endif /* MELLON_RS485_FSM_H_ */
