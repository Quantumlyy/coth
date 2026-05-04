/*
 * Mellon firmware — entry point.
 *
 * Boot sequence:
 *   1. board init (LEDs, RTT log)
 *   2. mode dispatcher init: NVS → sniff pipeline → libosdp glue → secchan
 *   3. RS-485 driver up at OSDP default 9600 baud
 *   4. BLE console up; advertising under name "Mellon"
 *   5. main loop: blink the heartbeat and feed the watchdog
 */

#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble/nus_console.h"
#include "board/board.h"
#include "board/rs485.h"
#include "mode.h"
#include "storage/nvs_config.h"
#include "util/log.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#ifndef MELLON_FIRMWARE_VERSION
#define MELLON_FIRMWARE_VERSION "0.0.0-pre"
#endif
#ifndef MELLON_BUILD_HASH
#define MELLON_BUILD_HASH "unknown"
#endif

#define HEARTBEAT_PERIOD_MS	1000
#define HEARTBEAT_ON_MS		50
#define WDT_TIMEOUT_MS		5000

static int wdt_chan = -1;
static const struct device *wdt_dev;

static int mellon_wdt_init(void)
{
	wdt_dev = DEVICE_DT_GET_ONE(nordic_nrf_wdt);
	if (!device_is_ready(wdt_dev)) {
		LOG_WRN("watchdog not ready — skipping");
		return -ENODEV;
	}
	struct wdt_timeout_cfg cfg = {
		.window.min = 0,
		.window.max = WDT_TIMEOUT_MS,
		.callback = NULL,
		.flags = WDT_FLAG_RESET_SOC,
	};
	wdt_chan = wdt_install_timeout(wdt_dev, &cfg);
	if (wdt_chan < 0) {
		LOG_WRN("wdt_install_timeout: %d", wdt_chan);
		return wdt_chan;
	}
	int ret = wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
	if (ret < 0) {
		LOG_WRN("wdt_setup: %d", ret);
		return ret;
	}
	return 0;
}

static void wdt_kick(void)
{
	if (wdt_chan >= 0 && wdt_dev) {
		(void)wdt_feed(wdt_dev, wdt_chan);
	}
}

int main(void)
{
	LOG_INF("");
	LOG_INF("Mellon firmware %s (%s)",
		MELLON_FIRMWARE_VERSION, MELLON_BUILD_HASH);

	mellon_log_init();

	int ret = mellon_board_init();
	if (ret < 0) {
		LOG_ERR("board_init: %d — halting", ret);
		return ret;
	}
	mellon_led_status(true);

	ret = mellon_rs485_init(9600);
	if (ret < 0) {
		LOG_ERR("rs485_init: %d — halting", ret);
		return ret;
	}

	ret = mellon_mode_init();
	if (ret < 0) {
		LOG_WRN("mode_init: %d — running degraded", ret);
	}

	uint32_t passkey = MELLON_DEFAULT_PASSKEY;
	(void)mellon_nvs_get_passkey(&passkey);
	ret = mellon_ble_console_init(passkey);
	if (ret < 0) {
		LOG_WRN("ble init: %d — running headless", ret);
	}

	(void)mellon_wdt_init();

	LOG_INF("boot complete — current mode=%s",
		mellon_mode_name(mellon_mode_get()));

	while (1) {
		mellon_led_act(true);
		k_msleep(HEARTBEAT_ON_MS);
		mellon_led_act(false);
		wdt_kick();
		k_msleep(HEARTBEAT_PERIOD_MS - HEARTBEAT_ON_MS);
	}
}
