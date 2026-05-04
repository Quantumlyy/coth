#include "libosdp_glue.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(libosdp_glue, CONFIG_LOG_DEFAULT_LEVEL);

#ifdef HAS_LIBOSDP

#include <osdp.h>	/* from external/libosdp; Zephyr-aware build */

static struct osdp *g_ctx;

int mellon_libosdp_init(void)
{
	osdp_pd_info_t pd_info = {
		.address = 0,
		.baud_rate = 9600,
		.flags = OSDP_FLAG_INSTALL_MODE,
	};
	g_ctx = osdp_pd_setup(&pd_info);
	if (!g_ctx) {
		LOG_ERR("osdp_pd_setup failed");
		return -ENODEV;
	}
	LOG_INF("libosdp wired in (observer mode)");
	return 0;
}

void mellon_libosdp_feed(const osdp_frame_t *frame)
{
	if (!g_ctx || !frame) {
		return;
	}
	(void)osdp_attack_raw_frame(g_ctx, frame->bytes, frame->length);
}

bool mellon_libosdp_present(void)
{
	return g_ctx != NULL;
}

#else /* HAS_LIBOSDP */

int mellon_libosdp_init(void)
{
	LOG_INF("libosdp not compiled in (HAS_LIBOSDP undefined) — using raw parser only");
	return 0;
}

void mellon_libosdp_feed(const osdp_frame_t *frame)
{
	(void)frame;
}

bool mellon_libosdp_present(void)
{
	return false;
}

#endif /* HAS_LIBOSDP */
