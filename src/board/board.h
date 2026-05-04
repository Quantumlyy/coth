/*
 * Mellon board support — GPIO + LED helpers.
 *
 * The Mellon v0.1 has exactly two firmware-controlled LEDs (D3 = activity,
 * D4 = status; spec §1.2) and one direction line for the RS-485 transceiver
 * (handled in board/rs485.c). This file owns the LEDs.
 */

#ifndef MELLON_BOARD_H_
#define MELLON_BOARD_H_

#include <stdbool.h>
#include <stdint.h>

/* Bring up GPIO peripherals and put both LEDs in a known-off state. */
int mellon_board_init(void);

/* D4 (status LED). Held by main loop / mode dispatcher. */
void mellon_led_status(bool on);

/* D3 (activity LED). Used by sniffer / KEYSET watcher to indicate frames. */
void mellon_led_act(bool on);

/*
 * Brief blink of D3 from a short non-blocking work item, suitable for
 * "frame seen" indicators that fire at OSDP poll rates without holding the
 * LED on long enough to merge visually into solid-on.
 */
void mellon_led_act_pulse(uint32_t ms);

#endif /* MELLON_BOARD_H_ */
