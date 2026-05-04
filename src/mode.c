#include "mode.h"

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "attacks/keyset.h"
#include "attacks/sniff.h"
#include "osdp/framing.h"
#include "osdp/libosdp_glue.h"
#include "osdp/secchan.h"
#include "storage/nvs_config.h"
#include "util/timing.h"

LOG_MODULE_REGISTER(mode, CONFIG_LOG_DEFAULT_LEVEL);

#define ARM_MAX_TTL_SEC		300

static const char *const k_mode_names[MODE_COUNT] = {
	[MODE_IDLE]		= "idle",
	[MODE_SNIFF]		= "sniff",
	[MODE_KEYSET_WATCH]	= "keyset_watch",
	[MODE_WEAK_KEYS]	= "weak_keys",
	[MODE_INSTALL_MODE]	= "install_mode",
	[MODE_PDCAP_DOWNGRADE]	= "pdcap_downgrade",
};

static struct {
	mellon_mode_t current;
	struct {
		mellon_mode_t mode;
		uint32_t nonce;
		uint64_t expires_ms;
	} armed;
} g;

static K_MUTEX_DEFINE(mode_lock);

const char *mellon_mode_name(mellon_mode_t m)
{
	if (m < 0 || m >= MODE_COUNT) {
		return "unknown";
	}
	return k_mode_names[m];
}

int mellon_mode_from_name(const char *name, mellon_mode_t *out)
{
	if (!name || !out) {
		return -EINVAL;
	}
	for (int i = 0; i < MODE_COUNT; i++) {
		if (strcmp(name, k_mode_names[i]) == 0) {
			*out = (mellon_mode_t)i;
			return 0;
		}
	}
	return -EINVAL;
}

static void apply_mode(mellon_mode_t mode)
{
	switch (mode) {
	case MODE_IDLE:
		mellon_sniff_set_active(false);
		break;
	case MODE_SNIFF:
	case MODE_KEYSET_WATCH:	/* keyset watcher feeds off the sniff pipeline */
	case MODE_WEAK_KEYS:	/* weak-key decryptor consumes captured frames; sniff stays on */
		mellon_sniff_set_active(true);
		break;
	case MODE_INSTALL_MODE:
	case MODE_PDCAP_DOWNGRADE:
		/* never reached — set() rejects these with -ENOSYS */
		break;
	default:
		break;
	}
}

int mellon_mode_init(void)
{
	memset(&g, 0, sizeof(g));
	g.current = MODE_SNIFF;	/* default per spec §3.2 */

	int ret = mellon_nvs_init();
	if (ret < 0) {
		LOG_WRN("nvs_init failed (%d) — using volatile defaults", ret);
	} else {
		mellon_mode_t persisted;
		if (mellon_nvs_get_mode(&persisted) == 0
		    && persisted < MODE_COUNT) {
			g.current = persisted;
		}
	}

	mellon_timing_init();
	secchan_init();
	mellon_libosdp_init();
	mellon_sniff_init();
	mellon_keyset_watcher_init();

	apply_mode(g.current);
	LOG_INF("mode dispatcher up — current=%s",
		mellon_mode_name(g.current));
	return 0;
}

int mellon_mode_set(mellon_mode_t mode)
{
	if (mode < 0 || mode >= MODE_COUNT) {
		return -EINVAL;
	}
	if (mode == MODE_INSTALL_MODE || mode == MODE_PDCAP_DOWNGRADE) {
		LOG_WRN("mode %s requested but not implemented in this build",
			mellon_mode_name(mode));
		return -ENOSYS;
	}

	k_mutex_lock(&mode_lock, K_FOREVER);
	g.current = mode;
	(void)mellon_nvs_put_mode(mode);
	apply_mode(mode);
	k_mutex_unlock(&mode_lock);

	LOG_INF("mode -> %s", mellon_mode_name(mode));
	return 0;
}

mellon_mode_t mellon_mode_get(void)
{
	return g.current;
}

int mellon_mode_arm(mellon_mode_t mode, uint32_t ttl_sec, uint32_t nonce)
{
	if (mode < 0 || mode >= MODE_COUNT) {
		return -EINVAL;
	}
	if (mode != MODE_INSTALL_MODE && mode != MODE_PDCAP_DOWNGRADE) {
		/* Arming only applies to active modes. */
		return -EINVAL;
	}
	if (ttl_sec == 0 || ttl_sec > ARM_MAX_TTL_SEC) {
		return -EINVAL;
	}

	k_mutex_lock(&mode_lock, K_FOREVER);
	g.armed.mode = mode;
	g.armed.nonce = nonce;
	g.armed.expires_ms = k_uptime_get() + (uint64_t)ttl_sec * 1000U;
	k_mutex_unlock(&mode_lock);
	return 0;
}

int mellon_mode_fire(mellon_mode_t mode, uint32_t nonce)
{
	k_mutex_lock(&mode_lock, K_FOREVER);
	bool match = (g.armed.mode == mode)
		     && (g.armed.nonce == nonce)
		     && (k_uptime_get() < (int64_t)g.armed.expires_ms);
	memset(&g.armed, 0, sizeof(g.armed));
	k_mutex_unlock(&mode_lock);

	if (!match) {
		return -EACCES;
	}
	/* Active firing not implemented in v1. */
	LOG_WRN("fire %s — not implemented in this build", mellon_mode_name(mode));
	return -ENOSYS;
}

bool mellon_mode_is_armed(mellon_mode_t mode)
{
	bool armed;
	k_mutex_lock(&mode_lock, K_FOREVER);
	armed = (g.armed.mode == mode)
	     && (k_uptime_get() < (int64_t)g.armed.expires_ms);
	k_mutex_unlock(&mode_lock);
	return armed;
}
