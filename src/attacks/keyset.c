#include "keyset.h"

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "../ble/nus_console.h"
#include "../board/board.h"
#include "../osdp/framing.h"
#include "../storage/capture.h"
#include "sniff.h"

LOG_MODULE_REGISTER(keyset, CONFIG_LOG_DEFAULT_LEVEL);

static struct {
	mellon_keyset_stats_t stats;
	keyset_match_t last;
	bool have_last;
} g;

static void on_frame(const osdp_frame_t *frame, void *user)
{
	(void)user;

	keyset_match_t m;
	if (!keyset_match(frame, &m)) {
		return;
	}

	g.last = m;
	g.have_last = true;
	g.stats.hits_total++;
	g.stats.last_seen_ms = (uint32_t)k_uptime_get_32();
	g.stats.last_address = m.address;

	/* Mark the just-recorded capture frame as preserve-on-evict. */
	(void)mellon_capture_preserve_last();

	/* Visual: solid D4 status while a fresh KEYSET sits in g.last. */
	mellon_led_status(true);

	LOG_WRN("KEYSET seen — addr=0x%02x, SCBK captured", m.address);

	mellon_ble_console_print(
		"* KEYSET seen at t=%ums addr=0x%02X scbk=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		g.stats.last_seen_ms, m.address,
		m.scbk[0], m.scbk[1], m.scbk[2], m.scbk[3],
		m.scbk[4], m.scbk[5], m.scbk[6], m.scbk[7],
		m.scbk[8], m.scbk[9], m.scbk[10], m.scbk[11],
		m.scbk[12], m.scbk[13], m.scbk[14], m.scbk[15]);
}

int mellon_keyset_watcher_init(void)
{
	memset(&g, 0, sizeof(g));
	int ret = mellon_sniff_subscribe(on_frame, NULL);
	if (ret < 0) {
		LOG_ERR("sniff_subscribe: %d", ret);
		return ret;
	}
	LOG_INF("KEYSET watcher armed");
	return 0;
}

void mellon_keyset_get_stats(mellon_keyset_stats_t *out)
{
	*out = g.stats;
}

const keyset_match_t *mellon_keyset_get_last(void)
{
	return g.have_last ? &g.last : NULL;
}
