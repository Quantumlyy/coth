/*
 * OSDP Secure Channel observer.
 *
 * Tracks the limited SC state we can derive from the raw parser alone:
 *   - Has anyone on the bus issued an osdp_KEYSET (CMD 0x75)?
 *   - What was the SCBK in that frame? (The KEYSET payload contains the
 *     base key in the clear — that's the whole point of attack #5 in
 *     spec §0.)
 *   - For a given address, is the bus running plaintext or SC?
 *
 * Real session-key derivation (RNDA/RNDB, S_ENC, S_MAC) lives in libosdp;
 * we only mirror what we can see.
 */

#ifndef MELLON_OSDP_SECCHAN_H_
#define MELLON_OSDP_SECCHAN_H_

#include <stdbool.h>
#include <stdint.h>
#include "framing.h"

#define SECCHAN_KEY_LEN			16
#define SECCHAN_TRACK_ADDRS		8	/* most buses run a handful of PDs */

typedef struct {
	uint8_t address;
	bool sc_active;
	bool scbk_known;
	uint8_t scbk[SECCHAN_KEY_LEN];
} secchan_pd_state_t;

void secchan_init(void);
void secchan_observe(const osdp_frame_t *frame);

/* Return the SCBK we last saw for `address`, or NULL if none. The pointer
 * is valid until the next secchan_observe() that overwrites it. */
const uint8_t *secchan_get_scbk(uint8_t address);

/* Snapshot for the BLE console "status" command. */
size_t secchan_dump(secchan_pd_state_t *out, size_t cap);

#endif /* MELLON_OSDP_SECCHAN_H_ */
