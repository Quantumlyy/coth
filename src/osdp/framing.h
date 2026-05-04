/*
 * Permissive OSDP frame parser.
 *
 * libosdp gives us a compliant view of the wire (good for sniff-and-decode);
 * this parser gives us the *attacker's* view: it accepts anything starting
 * with SOM = 0x53, parses the length, and emits the raw bytes regardless of
 * CRC/MAC/sequence validity. Spec §3.3 + §7.3.
 *
 * No Zephyr / kernel deps — pure C. Same translation unit is built into the
 * native_posix unit-test binary.
 */

#ifndef MELLON_OSDP_FRAMING_H_
#define MELLON_OSDP_FRAMING_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* OSDP Start-Of-Message marker. */
#define OSDP_SOM			0x53

/*
 * Hard cap on a single frame, spec §7.3. Anything longer is treated as
 * sync loss and we resync to the next SOM. The figure is generous: real
 * OSDP frames are typically <128 bytes.
 */
#define OSDP_FRAME_MAX_LEN		512

/* Byte offsets inside a buffered frame, post-SOM. */
#define OSDP_OFF_SOM			0
#define OSDP_OFF_ADDR			1
#define OSDP_OFF_LEN_LSB		2
#define OSDP_OFF_LEN_MSB		3
#define OSDP_OFF_CTRL			4
#define OSDP_OFF_CMD			5
#define OSDP_HEADER_LEN			5	/* SOM + addr + len(2) + ctrl */
#define OSDP_MIN_FRAME_LEN		8	/* hdr + cmd + crc(2) */

/* Address-byte bit 7: 0 = CP→PD command, 1 = PD→CP reply (per OSDP spec). */
#define OSDP_ADDR_REPLY_BIT		0x80

/* Control-byte fields. CTRL_SCB = bit 3 — Secure Control Block present. */
#define OSDP_CTRL_SQN_MASK		0x03
#define OSDP_CTRL_CRC_OR_MAC		0x04
#define OSDP_CTRL_SCB			0x08

typedef enum {
	OSDP_DIR_UNKNOWN	= 0,
	OSDP_DIR_CP_TO_PD	= 1,
	OSDP_DIR_PD_TO_CP	= 2,
} osdp_dir_t;

#define OSDP_FRAME_FLAG_ENCRYPTED	(1u << 0)	/* SCB bit set */
#define OSDP_FRAME_FLAG_BAD_CRC		(1u << 1)
#define OSDP_FRAME_FLAG_BAD_MAC		(1u << 2)
#define OSDP_FRAME_FLAG_TRUNCATED	(1u << 3)
#define OSDP_FRAME_FLAG_OVERSIZE	(1u << 4)	/* > OSDP_FRAME_MAX_LEN */

typedef struct {
	const uint8_t *bytes;	/* points into the framer's buffer; valid for the
				 * duration of the callback only */
	uint16_t length;
	osdp_dir_t direction;
	uint8_t address;
	uint8_t control;
	uint8_t cmd_or_reply;
	uint8_t flags;
} osdp_frame_t;

typedef void (*osdp_frame_cb_fn)(const osdp_frame_t *frame, void *user);

typedef enum {
	FRAME_STATE_HUNT_SOM = 0,	/* scanning for 0x53 */
	FRAME_STATE_NEED_HEADER,	/* have SOM, need addr + len */
	FRAME_STATE_NEED_BODY,		/* have header, reading body */
} osdp_framer_state_t;

typedef struct {
	uint8_t buf[OSDP_FRAME_MAX_LEN];
	size_t fill;			/* bytes currently in buf */
	size_t target;			/* total expected frame length */
	osdp_framer_state_t state;

	osdp_frame_cb_fn cb;
	void *user;

	/* Diagnostics — read by the BLE console "status" command. */
	uint32_t frames_total;
	uint32_t frames_bad_crc;
	uint32_t resync_count;
	uint32_t oversize_count;
} osdp_framer_t;

void osdp_framer_init(osdp_framer_t *f, osdp_frame_cb_fn cb, void *user);

/* Feed bytes from the UART RX path. Frames are emitted via the callback
 * synchronously as they complete. */
void osdp_framer_feed(osdp_framer_t *f, const uint8_t *bytes, size_t len);

/* Reset framer state, e.g. on mode switch. */
void osdp_framer_reset(osdp_framer_t *f);

/* CRC-16 per OSDP spec: polynomial 0x1021, init 0x1D0F, no reflection. */
uint16_t osdp_crc16(const uint8_t *data, size_t len);

#endif /* MELLON_OSDP_FRAMING_H_ */
