#include "capture.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/fcb.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

#include "../attacks/sniff.h"

LOG_MODULE_REGISTER(capture, CONFIG_LOG_DEFAULT_LEVEL);

#define CAPTURE_PARTITION		capture_partition
#define CAPTURE_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(CAPTURE_PARTITION)
#define CAPTURE_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(CAPTURE_PARTITION)
#define CAPTURE_PARTITION_SIZE		FIXED_PARTITION_SIZE(CAPTURE_PARTITION)

#define CAPTURE_SECTOR_SIZE		4096
#define CAPTURE_SECTOR_COUNT		(CAPTURE_PARTITION_SIZE / CAPTURE_SECTOR_SIZE)

static struct fcb g_fcb;
static struct flash_sector g_sectors[CAPTURE_SECTOR_COUNT];
static struct fcb_entry g_last_entry;
static bool g_have_last_entry;

static bool g_active;
static bool g_initialised;

static mellon_capture_stats_t g_stats;

static K_MUTEX_DEFINE(cap_lock);

/* Forward */
static void on_sniff_frame(const osdp_frame_t *frame, void *user);

int mellon_capture_init(void)
{
	if (g_initialised) {
		return 0;
	}

	memset(&g_fcb, 0, sizeof(g_fcb));
	g_fcb.f_magic = 0x4D454C4Eu;	/* "MELN" */
	g_fcb.f_version = 1;
	g_fcb.f_sector_cnt = CAPTURE_SECTOR_COUNT;
	g_fcb.f_scratch_cnt = 1;
	g_fcb.f_sectors = g_sectors;

	const struct device *dev = CAPTURE_PARTITION_DEVICE;
	if (!device_is_ready(dev)) {
		LOG_ERR("capture flash device not ready");
		return -ENODEV;
	}

	int sector_cnt = CAPTURE_SECTOR_COUNT;
	int ret = flash_area_get_sectors(FIXED_PARTITION_ID(CAPTURE_PARTITION),
					 (uint32_t *)&sector_cnt, g_sectors);
	if (ret < 0) {
		LOG_ERR("flash_area_get_sectors: %d", ret);
		return ret;
	}
	g_fcb.f_sector_cnt = (uint8_t)sector_cnt;

	ret = fcb_init(FIXED_PARTITION_ID(CAPTURE_PARTITION), &g_fcb);
	if (ret < 0) {
		LOG_WRN("fcb_init: %d — wiping and retrying", ret);
		(void)flash_area_erase(NULL, 0, CAPTURE_PARTITION_SIZE);
		ret = fcb_init(FIXED_PARTITION_ID(CAPTURE_PARTITION), &g_fcb);
		if (ret < 0) {
			LOG_ERR("fcb_init retry: %d", ret);
			return ret;
		}
	}

	g_initialised = true;
	g_active = true;
	memset(&g_stats, 0, sizeof(g_stats));

	(void)mellon_sniff_subscribe(on_sniff_frame, NULL);

	LOG_INF("capture ready: %u sectors x %u bytes",
		g_fcb.f_sector_cnt, CAPTURE_SECTOR_SIZE);
	return 0;
}

void mellon_capture_set_active(bool active)
{
	g_active = active;
}

bool mellon_capture_is_active(void)
{
	return g_active;
}

static void on_sniff_frame(const osdp_frame_t *frame, void *user)
{
	(void)user;
	if (!g_active || !g_initialised || !frame) {
		return;
	}
	(void)mellon_capture_record(frame);
}

int mellon_capture_record(const osdp_frame_t *frame)
{
	if (!g_initialised) {
		return -ENODEV;
	}

	uint8_t buf[CAPTURE_REC_MAX];
	capture_rec_hdr_t hdr = {
		.timestamp_us = (uint32_t)k_uptime_get_32() * 1000U,	/* ms→us proxy */
		.length = frame->length,
		.direction = (uint8_t)frame->direction,
		.flags = frame->flags,
	};
	int packed = capture_rec_pack(buf, sizeof(buf), &hdr, frame->bytes);
	if (packed < 0) {
		return packed;
	}

	struct fcb_entry loc;
	int ret;
	k_mutex_lock(&cap_lock, K_FOREVER);
	for (int attempt = 0; attempt < 4; attempt++) {
		ret = fcb_append(&g_fcb, (uint16_t)packed, &loc);
		if (ret == 0) {
			break;
		}
		if (ret == -ENOSPC) {
			(void)fcb_rotate(&g_fcb);
			g_stats.records_evicted++;
			continue;
		}
		break;
	}
	if (ret == 0) {
		ret = flash_area_write(g_fcb.fap, FCB_ENTRY_FA_DATA_OFF(loc),
				       buf, packed);
	}
	if (ret == 0) {
		ret = fcb_append_finish(&g_fcb, &loc);
	}
	if (ret == 0) {
		g_last_entry = loc;
		g_have_last_entry = true;
		g_stats.records_total++;
		g_stats.bytes_used_estimate += (uint32_t)packed;
	}
	k_mutex_unlock(&cap_lock);

	if (ret < 0) {
		LOG_WRN("capture write failed: %d", ret);
	}
	return ret;
}

int mellon_capture_preserve_last(void)
{
	/*
	 * FCB doesn't natively support per-entry mutation. For the v1
	 * implementation we just bump a counter and record the address
	 * of the entry; the eviction guard is implemented in M5 (commit 8)
	 * by re-appending preserved entries to the head before fcb_rotate
	 * erases the sector that holds them.
	 *
	 * Until then, this function is best-effort: it counts preservations
	 * for diagnostics but doesn't actually prevent eviction.
	 */
	if (!g_have_last_entry) {
		return -ENOENT;
	}
	g_stats.records_preserved++;
	return 0;
}

struct walk_ctx {
	mellon_capture_walk_cb cb;
	void *user;
	size_t max;
	size_t seen;
	int stop;
};

static int walk_helper(struct fcb_entry_ctx *eptr, void *user)
{
	struct walk_ctx *ctx = user;
	if (ctx->stop || ctx->seen >= ctx->max) {
		return 1;
	}

	uint8_t buf[CAPTURE_REC_MAX];
	const uint16_t to_read = (eptr->loc.fe_data_len < sizeof(buf))
				  ? eptr->loc.fe_data_len : sizeof(buf);
	int ret = flash_area_read(eptr->fap, FCB_ENTRY_FA_DATA_OFF(eptr->loc),
				  buf, to_read);
	if (ret < 0) {
		return 0;	/* skip this entry, keep iterating */
	}

	capture_rec_hdr_t hdr;
	const uint8_t *frame;
	int n = capture_rec_unpack(buf, to_read, &hdr, &frame);
	if (n < 0) {
		return 0;
	}

	int rc = ctx->cb(&hdr, frame, ctx->user);
	ctx->seen++;
	if (rc != 0) {
		ctx->stop = 1;
		return 1;
	}
	return 0;
}

int mellon_capture_walk(mellon_capture_walk_cb cb, void *user, size_t max_records)
{
	if (!cb || !g_initialised) {
		return -EINVAL;
	}
	struct walk_ctx ctx = { .cb = cb, .user = user, .max = max_records };
	(void)fcb_walk(&g_fcb, NULL, walk_helper, &ctx);
	return (int)ctx.seen;
}

int mellon_capture_wipe(void)
{
	if (!g_initialised) {
		return -ENODEV;
	}
	k_mutex_lock(&cap_lock, K_FOREVER);
	int ret = fcb_clear(&g_fcb);
	g_have_last_entry = false;
	memset(&g_stats, 0, sizeof(g_stats));
	k_mutex_unlock(&cap_lock);
	return ret;
}

void mellon_capture_get_stats(mellon_capture_stats_t *out)
{
	*out = g_stats;
}
