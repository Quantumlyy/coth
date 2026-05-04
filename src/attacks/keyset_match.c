#include "keyset_match.h"

#include <string.h>

bool keyset_match(const osdp_frame_t *frame, keyset_match_t *out)
{
	if (!frame || !frame->bytes) {
		return false;
	}
	if (frame->direction != OSDP_DIR_CP_TO_PD) {
		return false;
	}
	if (frame->cmd_or_reply != OSDP_CMD_KEYSET) {
		return false;
	}
	/*
	 * Need at least: header (5) + cmd (1) + key_type (1) + key_len (1)
	 * + 16 bytes of key + 2 bytes of CRC.
	 */
	const uint16_t need = OSDP_HEADER_LEN + 1 + 2 + OSDP_KEYSET_KEY_LEN + 2;
	if (frame->length < need) {
		return false;
	}

	const uint8_t *payload = frame->bytes + OSDP_OFF_CMD + 1;
	if (payload[0] != OSDP_KEYSET_TYPE_SCBK) {
		return false;
	}
	if (payload[1] != OSDP_KEYSET_KEY_LEN) {
		return false;
	}

	if (out) {
		out->address = frame->address & ~OSDP_ADDR_REPLY_BIT;
		memcpy(out->scbk, payload + 2, OSDP_KEYSET_KEY_LEN);
	}
	return true;
}
