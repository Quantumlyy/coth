/*
 * Mellon firmware — entry point.
 *
 * M0 bring-up: initialises the board, prints a build-hash banner, blinks D3
 * at ~1 Hz as a heartbeat. The mode dispatcher and BLE console arrive in
 * later commits; this file evolves with each milestone.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "board/board.h"
#include "util/log.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#ifndef MELLON_FIRMWARE_VERSION
#define MELLON_FIRMWARE_VERSION "0.0.0-pre"
#endif

#ifndef MELLON_BUILD_HASH
#define MELLON_BUILD_HASH "unknown"
#endif

#define HEARTBEAT_ON_MS		50
#define HEARTBEAT_PERIOD_MS	1000

int main(void)
{
	LOG_INF("");
	LOG_INF("Mellon firmware %s (%s)", MELLON_FIRMWARE_VERSION, MELLON_BUILD_HASH);
	LOG_INF("M0 bring-up — heartbeat only. See docs/quickstart.md.");

	mellon_log_init();

	int ret = mellon_board_init();
	if (ret < 0) {
		LOG_ERR("board_init failed: %d — halting", ret);
		return ret;
	}

	/* Status LED solid: alive, idle, no mode active yet. */
	mellon_led_status(true);

	while (1) {
		mellon_led_act(true);
		k_msleep(HEARTBEAT_ON_MS);
		mellon_led_act(false);
		k_msleep(HEARTBEAT_PERIOD_MS - HEARTBEAT_ON_MS);
	}
}
