#include "framing.h"

#include <string.h>

uint16_t osdp_crc16(const uint8_t *data, size_t len)
{
	uint16_t crc = 0x1D0F;
	for (size_t i = 0; i < len; i++) {
		crc ^= (uint16_t)data[i] << 8;
		for (int b = 0; b < 8; b++) {
			crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U)
					      : (uint16_t)(crc << 1);
		}
	}
	return crc;
}

static void emit_frame(osdp_framer_t *f, uint8_t extra_flags)
{
	osdp_frame_t fr = {
		.bytes = f->buf,
		.length = (uint16_t)f->fill,
		.address = f->buf[OSDP_OFF_ADDR],
		.control = f->buf[OSDP_OFF_CTRL],
		.cmd_or_reply = f->buf[OSDP_OFF_CMD],
		.flags = extra_flags,
	};
	fr.direction = (fr.address & OSDP_ADDR_REPLY_BIT) ? OSDP_DIR_PD_TO_CP
							  : OSDP_DIR_CP_TO_PD;
	if (fr.control & OSDP_CTRL_SCB) {
		fr.flags |= OSDP_FRAME_FLAG_ENCRYPTED;
	}

	if (f->fill >= 2) {
		uint16_t got = (uint16_t)f->buf[f->fill - 2]
			     | ((uint16_t)f->buf[f->fill - 1] << 8);
		uint16_t want = osdp_crc16(f->buf, f->fill - 2);
		if (got != want) {
			fr.flags |= OSDP_FRAME_FLAG_BAD_CRC;
			f->frames_bad_crc++;
		}
	} else {
		fr.flags |= OSDP_FRAME_FLAG_TRUNCATED;
	}

	f->frames_total++;
	if (f->cb) {
		f->cb(&fr, f->user);
	}
}

static void resync(osdp_framer_t *f)
{
	/*
	 * On sync loss, slide the buffer past the leading SOM (if any) and
	 * scan for the next SOM in what we already have. This recovers from
	 * a corrupted length field without dropping the frame that follows.
	 */
	f->resync_count++;
	if (f->fill == 0) {
		f->state = FRAME_STATE_HUNT_SOM;
		return;
	}

	for (size_t i = 1; i < f->fill; i++) {
		if (f->buf[i] == OSDP_SOM) {
			memmove(f->buf, f->buf + i, f->fill - i);
			f->fill -= i;
			f->target = 0;
			f->state = FRAME_STATE_NEED_HEADER;

			/*
			 * If the buffered remainder already contains a parseable header,
			 * evaluate length immediately rather than waiting for another
			 * byte to trigger the NEED_HEADER branch — otherwise the next
			 * byte would falsely satisfy fill >= target=0 in NEED_BODY.
			 */
			if (f->fill >= OSDP_HEADER_LEN) {
				uint16_t total = (uint16_t)f->buf[OSDP_OFF_LEN_LSB]
					       | ((uint16_t)f->buf[OSDP_OFF_LEN_MSB] << 8);
				if (total >= OSDP_MIN_FRAME_LEN
				    && total <= OSDP_FRAME_MAX_LEN) {
					f->target = total;
					f->state = FRAME_STATE_NEED_BODY;
					if (f->fill >= f->target) {
						emit_frame(f, 0);
						osdp_framer_reset(f);
					}
				} else {
					/* Inner header also bogus — drop and HUNT. */
					f->fill = 0;
					f->state = FRAME_STATE_HUNT_SOM;
				}
			}
			return;
		}
	}
	f->fill = 0;
	f->state = FRAME_STATE_HUNT_SOM;
}

void osdp_framer_init(osdp_framer_t *f, osdp_frame_cb_fn cb, void *user)
{
	memset(f, 0, sizeof(*f));
	f->cb = cb;
	f->user = user;
	f->state = FRAME_STATE_HUNT_SOM;
}

void osdp_framer_reset(osdp_framer_t *f)
{
	f->fill = 0;
	f->target = 0;
	f->state = FRAME_STATE_HUNT_SOM;
	/* Diagnostic counters survive reset — they're cumulative-since-boot. */
}

void osdp_framer_feed(osdp_framer_t *f, const uint8_t *bytes, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		uint8_t b = bytes[i];

		switch (f->state) {
		case FRAME_STATE_HUNT_SOM:
			if (b == OSDP_SOM) {
				f->buf[0] = b;
				f->fill = 1;
				f->target = 0;
				f->state = FRAME_STATE_NEED_HEADER;
			}
			break;

		case FRAME_STATE_NEED_HEADER:
			f->buf[f->fill++] = b;
			if (f->fill >= OSDP_HEADER_LEN) {
				uint16_t total = (uint16_t)f->buf[OSDP_OFF_LEN_LSB]
					       | ((uint16_t)f->buf[OSDP_OFF_LEN_MSB] << 8);

				if (total < OSDP_MIN_FRAME_LEN) {
					/* Implausibly short — likely garbage; resync. */
					resync(f);
					break;
				}
				if (total > OSDP_FRAME_MAX_LEN) {
					/* Oversize — flag and resync. */
					f->oversize_count++;
					resync(f);
					break;
				}
				f->target = total;
				f->state = FRAME_STATE_NEED_BODY;
				if (f->fill >= f->target) {
					emit_frame(f, 0);
					osdp_framer_reset(f);
				}
			}
			break;

		case FRAME_STATE_NEED_BODY:
			f->buf[f->fill++] = b;
			if (f->fill >= f->target) {
				emit_frame(f, 0);
				osdp_framer_reset(f);
			} else if (f->fill >= OSDP_FRAME_MAX_LEN) {
				/*
				 * Defensive: target said we'd already have stopped,
				 * but in case of state corruption never overflow buf.
				 */
				f->oversize_count++;
				emit_frame(f, OSDP_FRAME_FLAG_OVERSIZE
					      | OSDP_FRAME_FLAG_TRUNCATED);
				osdp_framer_reset(f);
			}
			break;
		}
	}
}
