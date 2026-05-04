#include "rs485_fsm.h"

void rs485_fsm_init(rs485_fsm_t *fsm, rs485_pin_set_fn pin_set, void *user)
{
	fsm->pin_set = pin_set;
	fsm->user = user;
	fsm->dir = RS485_DIR_RX;
	fsm->tx_done_count = 0;
	fsm->tx_aborted_count = 0;
	fsm->late_tx_done_count = 0;

	/* Force DE LOW on init regardless of previous pin state. */
	if (fsm->pin_set) {
		fsm->pin_set(false, fsm->user);
	}
}

int rs485_fsm_tx_begin(rs485_fsm_t *fsm)
{
	if (fsm->dir == RS485_DIR_TX) {
		/* Already in TX direction; chained frame. Don't toggle the pin. */
		return 1;
	}
	fsm->dir = RS485_DIR_TX;
	if (fsm->pin_set) {
		fsm->pin_set(true, fsm->user);
	}
	return 0;
}

void rs485_fsm_tx_done(rs485_fsm_t *fsm)
{
	if (fsm->dir != RS485_DIR_TX) {
		/*
		 * Defensive: a TX_DONE event with no preceding tx_begin shouldn't
		 * happen, but if it does we still want to be sure the pin is low.
		 */
		fsm->late_tx_done_count++;
		if (fsm->pin_set) {
			fsm->pin_set(false, fsm->user);
		}
		return;
	}
	fsm->tx_done_count++;
	fsm->dir = RS485_DIR_RX;
	if (fsm->pin_set) {
		fsm->pin_set(false, fsm->user);
	}
}

void rs485_fsm_tx_aborted(rs485_fsm_t *fsm)
{
	fsm->tx_aborted_count++;
	fsm->dir = RS485_DIR_RX;
	if (fsm->pin_set) {
		fsm->pin_set(false, fsm->user);
	}
}

rs485_dir_t rs485_fsm_dir(const rs485_fsm_t *fsm)
{
	return fsm->dir;
}
