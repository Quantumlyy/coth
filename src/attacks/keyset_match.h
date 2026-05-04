/*
 * KEYSET frame matcher — pure C, host-testable.
 *
 * Detects an osdp_KEYSET command (CMD = 0x75) on the wire and pulls the
 * SCBK out of the payload. Spec §0 attack #5: install-mode panels send the
 * base key in the clear during commissioning, and a passive sniffer can
 * pick it up.
 *
 * Frame layout (per OSDP spec):
 *   SOM ADDR LSB MSB CTRL CMD=0x75 KEY_TYPE KEY_LEN KEY[16] CRC_LSB CRC_MSB
 * KEY_TYPE = 0x01 means SCBK; KEY_LEN = 0x10 (16) for AES-128.
 */

#ifndef MELLON_ATTACKS_KEYSET_MATCH_H_
#define MELLON_ATTACKS_KEYSET_MATCH_H_

#include <stdbool.h>
#include <stdint.h>

#include "../osdp/framing.h"

#define OSDP_CMD_KEYSET		0x75
#define OSDP_KEYSET_TYPE_SCBK	0x01
#define OSDP_KEYSET_KEY_LEN	16

typedef struct {
	uint8_t address;
	uint8_t scbk[OSDP_KEYSET_KEY_LEN];
} keyset_match_t;

/* True iff the frame is a CP→PD osdp_KEYSET with a 16-byte SCBK payload.
 * If true and `out` non-NULL, fills with address + key. */
bool keyset_match(const osdp_frame_t *frame, keyset_match_t *out);

#endif /* MELLON_ATTACKS_KEYSET_MATCH_H_ */
