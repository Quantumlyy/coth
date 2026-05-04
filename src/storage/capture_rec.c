#include "capture_rec.h"

#include <string.h>

int capture_rec_pack(uint8_t *out, size_t cap,
		     const capture_rec_hdr_t *hdr,
		     const uint8_t *frame)
{
	if (!out || !hdr) {
		return -1;
	}
	if (hdr->length > CAPTURE_FRAME_MAX) {
		return -2;
	}
	const size_t total = (size_t)CAPTURE_REC_HDR_LEN + hdr->length;
	if (cap < total) {
		return -3;
	}

	out[0] = (uint8_t)(hdr->timestamp_us & 0xff);
	out[1] = (uint8_t)((hdr->timestamp_us >> 8) & 0xff);
	out[2] = (uint8_t)((hdr->timestamp_us >> 16) & 0xff);
	out[3] = (uint8_t)((hdr->timestamp_us >> 24) & 0xff);
	out[4] = (uint8_t)(hdr->length & 0xff);
	out[5] = (uint8_t)((hdr->length >> 8) & 0xff);
	out[6] = hdr->direction;
	out[7] = hdr->flags;

	if (hdr->length > 0) {
		if (!frame) {
			return -4;
		}
		memcpy(out + CAPTURE_REC_HDR_LEN, frame, hdr->length);
	}
	return (int)total;
}

int capture_rec_unpack(const uint8_t *in, size_t len,
		       capture_rec_hdr_t *hdr,
		       const uint8_t **frame_out)
{
	if (!in || !hdr || len < CAPTURE_REC_HDR_LEN) {
		return -1;
	}
	hdr->timestamp_us = ((uint32_t)in[0])
			  | ((uint32_t)in[1] << 8)
			  | ((uint32_t)in[2] << 16)
			  | ((uint32_t)in[3] << 24);
	hdr->length = (uint16_t)in[4] | ((uint16_t)in[5] << 8);
	hdr->direction = in[6];
	hdr->flags = in[7];

	if (hdr->length > CAPTURE_FRAME_MAX) {
		return -2;
	}
	const size_t total = (size_t)CAPTURE_REC_HDR_LEN + hdr->length;
	if (len < total) {
		return -3;
	}

	if (frame_out) {
		*frame_out = (hdr->length > 0) ? in + CAPTURE_REC_HDR_LEN : NULL;
	}
	return (int)total;
}
