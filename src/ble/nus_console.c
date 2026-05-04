#include "nus_console.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "../attacks/sniff.h"
#include "../mode.h"
#include "../osdp/framing.h"
#include "../osdp/libosdp_glue.h"
#include "../osdp/secchan.h"
#include "../storage/nvs_config.h"
#include "../util/timing.h"

LOG_MODULE_REGISTER(ble_console, CONFIG_LOG_DEFAULT_LEVEL);

#ifndef MELLON_FIRMWARE_VERSION
#define MELLON_FIRMWARE_VERSION "0.0.0-pre"
#endif
#ifndef MELLON_BUILD_HASH
#define MELLON_BUILD_HASH "unknown"
#endif

#define LINE_BUF_LEN	160
#define OUT_BUF_LEN	256

static struct bt_conn *g_conn;
static struct k_mutex tx_lock;
static char rx_line[LINE_BUF_LEN];
static size_t rx_fill;

/* Forward decls */
static void cmd_dispatch(const char *line);

/* ---------- BLE plumbing ---------- */

static void on_connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("ble connect err %u", err);
		return;
	}
	g_conn = bt_conn_ref(conn);
	mellon_ble_console_print("Mellon %s (%s) — type 'help'",
				 MELLON_FIRMWARE_VERSION, MELLON_BUILD_HASH);
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("ble disconnected reason=%u", reason);
	if (g_conn) {
		bt_conn_unref(g_conn);
		g_conn = NULL;
	}
	rx_fill = 0;
}

static struct bt_conn_cb conn_cb = {
	.connected = on_connected,
	.disconnected = on_disconnected,
};

static void on_nus_rx(struct bt_conn *conn, const void *data, uint16_t len)
{
	(void)conn;
	const uint8_t *p = data;

	for (uint16_t i = 0; i < len; i++) {
		uint8_t c = p[i];
		if (c == '\r') {
			continue;
		}
		if (c == '\n') {
			rx_line[rx_fill] = '\0';
			cmd_dispatch(rx_line);
			rx_fill = 0;
			continue;
		}
		if (rx_fill < sizeof(rx_line) - 1) {
			rx_line[rx_fill++] = (char)c;
		} else {
			/* line too long — drop and ask the operator to retry */
			rx_fill = 0;
			mellon_ble_console_print("err: line >%d chars", LINE_BUF_LEN - 1);
		}
	}
}

static void on_nus_send_enabled(enum bt_nus_send_status status)
{
	(void)status;
}

static struct bt_nus_cb nus_cb = {
	.received = on_nus_rx,
	.send_enabled = on_nus_send_enabled,
};

static const struct bt_data adv[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, "Mellon", 6),
};

void mellon_ble_console_print(const char *fmt, ...)
{
	if (!g_conn) {
		return;
	}
	char buf[OUT_BUF_LEN];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
	va_end(ap);
	if (n < 0) {
		return;
	}
	if (n > (int)sizeof(buf) - 2) {
		n = sizeof(buf) - 2;
	}
	buf[n++] = '\n';
	buf[n] = '\0';

	k_mutex_lock(&tx_lock, K_FOREVER);
	(void)bt_nus_send(g_conn, buf, n);
	k_mutex_unlock(&tx_lock);
}

/* ---------- Command dispatch ---------- */

static char *next_arg(char **cursor)
{
	if (!cursor || !*cursor) {
		return NULL;
	}
	char *p = *cursor;
	while (*p && isspace((unsigned char)*p)) {
		p++;
	}
	if (*p == '\0') {
		*cursor = p;
		return NULL;
	}
	char *start = p;
	while (*p && !isspace((unsigned char)*p)) {
		p++;
	}
	if (*p) {
		*p++ = '\0';
	}
	*cursor = p;
	return start;
}

static void cmd_help(void)
{
	mellon_ble_console_print("commands:");
	mellon_ble_console_print("  help                 this");
	mellon_ble_console_print("  status               build, mode, framer + bus stats");
	mellon_ble_console_print("  mode <name>          idle|sniff|keyset_watch|weak_keys|...");
	mellon_ble_console_print("  dump <n>             last n captured frames (M3)");
	mellon_ble_console_print("  capture on|off       toggle flash capture (M3)");
	mellon_ble_console_print("  wipe                 erase capture region (M3)");
	mellon_ble_console_print("  keys                 list captured SCBKs");
	mellon_ble_console_print("  attack weak_keys     run trial decrypt (M4)");
	mellon_ble_console_print("  arm <mode> <ttl>     arm an active mode (M6/M7 — disabled)");
	mellon_ble_console_print("  fire <mode> <nonce>  execute the armed mode");
}

static void cmd_status(void)
{
	int64_t up_ms = k_uptime_get();
	uint32_t up_s = (uint32_t)(up_ms / 1000);

	mellon_sniff_stats_t s;
	mellon_sniff_get_stats(&s);
	mellon_timing_stats_t t;
	mellon_timing_get(&t);

	mellon_ble_console_print("build:    %s (%s)",
				 MELLON_FIRMWARE_VERSION, MELLON_BUILD_HASH);
	mellon_ble_console_print("uptime:   %02u:%02u:%02u",
				 up_s / 3600, (up_s / 60) % 60, up_s % 60);
	mellon_ble_console_print("mode:     %s",
				 mellon_mode_name(mellon_mode_get()));
	mellon_ble_console_print("frames:   total=%u bad_crc=%u resync=%u oversize=%u",
				 s.frames_total, s.frames_bad_crc,
				 s.resync_count, s.oversize_count);
	mellon_ble_console_print("polls:    n=%u avg=%ums min=%ums max=%ums",
				 t.poll_count, t.avg_interval_ms,
				 t.min_interval_ms == UINT32_MAX ? 0 : t.min_interval_ms,
				 t.max_interval_ms);
	mellon_ble_console_print("libosdp:  %s",
				 mellon_libosdp_present() ? "present" : "absent");
}

static void cmd_mode(char *args)
{
	char *name = next_arg(&args);
	if (!name) {
		mellon_ble_console_print("usage: mode <name>");
		return;
	}
	mellon_mode_t m;
	if (mellon_mode_from_name(name, &m) < 0) {
		mellon_ble_console_print("err: unknown mode '%s'", name);
		return;
	}
	int ret = mellon_mode_set(m);
	if (ret == -ENOSYS) {
		mellon_ble_console_print("err: %s — v1.1/v2 — not implemented in this build", name);
		return;
	}
	if (ret < 0) {
		mellon_ble_console_print("err: %d", ret);
		return;
	}
	mellon_ble_console_print("ok mode=%s", mellon_mode_name(m));
}

static void cmd_keys(void)
{
	secchan_pd_state_t pds[SECCHAN_TRACK_ADDRS];
	size_t n = secchan_dump(pds, SECCHAN_TRACK_ADDRS);
	if (n == 0) {
		mellon_ble_console_print("no PD state observed yet");
		return;
	}
	for (size_t i = 0; i < n; i++) {
		mellon_ble_console_print("addr=0x%02x sc=%c scbk=%s",
			pds[i].address,
			pds[i].sc_active ? 'y' : 'n',
			pds[i].scbk_known ? "captured" : "-");
	}
}

static void cmd_dispatch(const char *line)
{
	char buf[LINE_BUF_LEN];
	strncpy(buf, line, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	char *cursor = buf;
	char *cmd = next_arg(&cursor);
	if (!cmd || cmd[0] == '\0') {
		return;
	}

	if (strcmp(cmd, "help") == 0) {
		cmd_help();
	} else if (strcmp(cmd, "status") == 0) {
		cmd_status();
	} else if (strcmp(cmd, "mode") == 0) {
		cmd_mode(cursor);
	} else if (strcmp(cmd, "keys") == 0) {
		cmd_keys();
	} else if (strcmp(cmd, "dump") == 0
		   || strcmp(cmd, "capture") == 0
		   || strcmp(cmd, "wipe") == 0
		   || strcmp(cmd, "attack") == 0
		   || strcmp(cmd, "arm") == 0
		   || strcmp(cmd, "fire") == 0) {
		/* Hooked up in commits 7 (capture), 8 (KEYSET), 9 (weak_keys). */
		mellon_ble_console_print("err: '%s' not yet wired in this build", cmd);
	} else {
		mellon_ble_console_print("err: unknown '%s' (try 'help')", cmd);
	}
}

/* ---------- BLE bring-up ---------- */

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	(void)conn;
	LOG_INF("BLE passkey: %06u", passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
	(void)conn;
	LOG_INF("BLE pairing cancelled");
}

static struct bt_conn_auth_cb auth_cb = {
	.passkey_display = auth_passkey_display,
	.cancel = auth_cancel,
};

int mellon_ble_console_init(uint32_t fixed_passkey)
{
	int ret;

	k_mutex_init(&tx_lock);

	bt_conn_cb_register(&conn_cb);
	bt_conn_auth_cb_register(&auth_cb);

	ret = bt_enable(NULL);
	if (ret < 0) {
		LOG_ERR("bt_enable: %d", ret);
		return ret;
	}

	ret = bt_passkey_set(fixed_passkey);
	if (ret < 0) {
		LOG_ERR("bt_passkey_set: %d", ret);
		return ret;
	}

	ret = bt_nus_cb_register(&nus_cb, NULL);
	if (ret < 0) {
		LOG_ERR("bt_nus_cb_register: %d", ret);
		return ret;
	}

	ret = bt_le_adv_start(BT_LE_ADV_CONN_NAME, adv, ARRAY_SIZE(adv),
			      sd, ARRAY_SIZE(sd));
	if (ret < 0) {
		LOG_ERR("bt_le_adv_start: %d", ret);
		return ret;
	}

	LOG_INF("ble advertising as 'Mellon', passkey-only pairing");
	return 0;
}
