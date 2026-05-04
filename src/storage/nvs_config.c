#include "nvs_config.h"

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(nvs_cfg, CONFIG_LOG_DEFAULT_LEVEL);

#define NVS_PARTITION		nvs_partition
#define NVS_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(NVS_PARTITION)

#define NVS_KEY_MODE		1
#define NVS_KEY_PASSKEY		2

/* Preserved-frame slots live at NVS keys 0x100..0x100+SLOTS-1. */
#define NVS_KEY_PRESERVED_BASE	0x100

static struct nvs_fs g_fs;
static bool g_initialised;

int mellon_nvs_init(void)
{
	if (g_initialised) {
		return 0;
	}

	struct flash_pages_info info;
	g_fs.flash_device = NVS_PARTITION_DEVICE;
	if (!device_is_ready(g_fs.flash_device)) {
		LOG_ERR("nvs flash device not ready");
		return -ENODEV;
	}
	g_fs.offset = NVS_PARTITION_OFFSET;

	int ret = flash_get_page_info_by_offs(g_fs.flash_device, g_fs.offset, &info);
	if (ret < 0) {
		LOG_ERR("flash_get_page_info_by_offs: %d", ret);
		return ret;
	}
	g_fs.sector_size = info.size;
	g_fs.sector_count = 8;	/* matches the 32 KB nvs_partition / 4 KB pages */

	ret = nvs_mount(&g_fs);
	if (ret < 0) {
		LOG_ERR("nvs_mount: %d", ret);
		return ret;
	}

	g_initialised = true;
	LOG_INF("nvs ready (offset=0x%lx, sector=%u, count=%u)",
		(unsigned long)g_fs.offset, g_fs.sector_size, g_fs.sector_count);
	return 0;
}

int mellon_nvs_put_mode(mellon_mode_t mode)
{
	if (!g_initialised) {
		return -EBADF;
	}
	uint8_t v = (uint8_t)mode;
	ssize_t n = nvs_write(&g_fs, NVS_KEY_MODE, &v, sizeof(v));
	return (n < 0) ? (int)n : 0;
}

int mellon_nvs_get_mode(mellon_mode_t *out)
{
	if (!g_initialised || !out) {
		return -EBADF;
	}
	uint8_t v;
	ssize_t n = nvs_read(&g_fs, NVS_KEY_MODE, &v, sizeof(v));
	if (n != sizeof(v)) {
		return -ENOENT;
	}
	*out = (mellon_mode_t)v;
	return 0;
}

int mellon_nvs_put_passkey(uint32_t passkey)
{
	if (!g_initialised) {
		return -EBADF;
	}
	ssize_t n = nvs_write(&g_fs, NVS_KEY_PASSKEY, &passkey, sizeof(passkey));
	return (n < 0) ? (int)n : 0;
}

int mellon_nvs_get_passkey(uint32_t *out)
{
	if (!g_initialised || !out) {
		return -EBADF;
	}
	ssize_t n = nvs_read(&g_fs, NVS_KEY_PASSKEY, out, sizeof(*out));
	if (n != sizeof(*out)) {
		return -ENOENT;
	}
	return 0;
}

int mellon_nvs_put_preserved(uint8_t slot, const uint8_t *bytes, size_t len)
{
	if (!g_initialised || slot >= MELLON_PRESERVED_SLOTS || !bytes) {
		return -EINVAL;
	}
	ssize_t n = nvs_write(&g_fs, NVS_KEY_PRESERVED_BASE + slot, bytes, len);
	return (n < 0) ? (int)n : 0;
}

int mellon_nvs_get_preserved(uint8_t slot, uint8_t *out, size_t cap, size_t *len_out)
{
	if (!g_initialised || slot >= MELLON_PRESERVED_SLOTS || !out) {
		return -EINVAL;
	}
	ssize_t n = nvs_read(&g_fs, NVS_KEY_PRESERVED_BASE + slot, out, cap);
	if (n < 0) {
		return -ENOENT;
	}
	if (len_out) {
		*len_out = (size_t)n;
	}
	return 0;
}

int mellon_nvs_clear_preserved(void)
{
	if (!g_initialised) {
		return -EBADF;
	}
	for (uint8_t i = 0; i < MELLON_PRESERVED_SLOTS; i++) {
		(void)nvs_delete(&g_fs, NVS_KEY_PRESERVED_BASE + i);
	}
	return 0;
}
