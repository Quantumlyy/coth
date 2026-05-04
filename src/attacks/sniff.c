#include "sniff.h"

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "../board/board.h"
#include "../board/rs485.h"
#include "../osdp/framing.h"
#include "../osdp/libosdp_glue.h"
#include "../osdp/secchan.h"
#include "../util/timing.h"

LOG_MODULE_REGISTER(sniff, CONFIG_LOG_DEFAULT_LEVEL);

#define MELLON_SNIFF_MAX_SUBS	4

#define OSDP_CMD_POLL		0x60

static struct {
	mellon_sniff_frame_fn fn;
	void *user;
} g_subs[MELLON_SNIFF_MAX_SUBS];
static size_t g_sub_count;

static osdp_framer_t g_framer;
static bool g_active;

/* From rs485.c via the rx handler. The handler runs in callback context
 * (system workqueue). It must not block; the framer is non-blocking. */
static void on_uart_rx(const uint8_t *bytes, size_t len, void *user)
{
	(void)user;
	if (!g_active) {
		return;
	}
	osdp_framer_feed(&g_framer, bytes, len);
}

static void on_frame(const osdp_frame_t *frame, void *user)
{
	(void)user;

	/* Visual: blip D3 for every frame seen. The 30-ms pulse merges into
	 * a flicker at high poll rates rather than hiding it. */
	mellon_led_act_pulse(30);

	/* Bus timing — only count CP-side polls so the average isn't pulled
	 * by PD replies that follow each poll. */
	if (frame->direction == OSDP_DIR_CP_TO_PD
	    && frame->cmd_or_reply == OSDP_CMD_POLL) {
		mellon_timing_record_poll();
	}

	secchan_observe(frame);
	mellon_libosdp_feed(frame);

	for (size_t i = 0; i < g_sub_count; i++) {
		if (g_subs[i].fn) {
			g_subs[i].fn(frame, g_subs[i].user);
		}
	}
}

int mellon_sniff_init(void)
{
	memset(g_subs, 0, sizeof(g_subs));
	g_sub_count = 0;

	osdp_framer_init(&g_framer, on_frame, NULL);
	mellon_rs485_set_rx_handler(on_uart_rx, NULL);

	g_active = true;
	LOG_INF("sniff active");
	return 0;
}

int mellon_sniff_subscribe(mellon_sniff_frame_fn fn, void *user)
{
	if (g_sub_count >= MELLON_SNIFF_MAX_SUBS) {
		return -ENOMEM;
	}
	g_subs[g_sub_count].fn = fn;
	g_subs[g_sub_count].user = user;
	g_sub_count++;
	return 0;
}

void mellon_sniff_set_active(bool active)
{
	g_active = active;
	if (!active) {
		osdp_framer_reset(&g_framer);
	}
}

bool mellon_sniff_is_active(void)
{
	return g_active;
}

void mellon_sniff_get_stats(mellon_sniff_stats_t *out)
{
	out->frames_total = g_framer.frames_total;
	out->frames_bad_crc = g_framer.frames_bad_crc;
	out->resync_count = g_framer.resync_count;
	out->oversize_count = g_framer.oversize_count;
}
