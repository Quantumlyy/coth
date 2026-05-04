/*
 * Capture record serialisation — pure C, host-testable.
 *
 * One record represents one OSDP frame the firmware decided to persist.
 * The on-flash layout is:
 *
 *   uint32_t timestamp_us;   // little-endian; since boot
 *   uint16_t length;         // little-endian; frame bytes that follow
 *   uint8_t  direction;      // CAPTURE_DIR_* below
 *   uint8_t  flags;          // CAPTURE_FLAG_* below
 *   uint8_t  frame[length];  // raw OSDP bytes off the wire
 *
 * Total: 8 + length bytes.
 *
 * Rationale: byte-exact and endian-explicit so a host running
 * tools/parse_capture.py can decode a flash dump without help from the
 * firmware. Spec §5.
 */

#ifndef MELLON_CAPTURE_REC_H_
#define MELLON_CAPTURE_REC_H_

#include <stddef.h>
#include <stdint.h>

#define CAPTURE_REC_HDR_LEN	8
#define CAPTURE_FRAME_MAX	512	/* matches OSDP_FRAME_MAX_LEN */
#define CAPTURE_REC_MAX		(CAPTURE_REC_HDR_LEN + CAPTURE_FRAME_MAX)

/* Direction encoding (matches osdp_dir_t numerically). */
#define CAPTURE_DIR_UNKNOWN	0
#define CAPTURE_DIR_CP_TO_PD	1
#define CAPTURE_DIR_PD_TO_CP	2

/* Flag bits — first 5 mirror OSDP_FRAME_FLAG_*; bit 5 is M5-specific. */
#define CAPTURE_FLAG_ENCRYPTED	(1u << 0)
#define CAPTURE_FLAG_BAD_CRC	(1u << 1)
#define CAPTURE_FLAG_BAD_MAC	(1u << 2)
#define CAPTURE_FLAG_TRUNCATED	(1u << 3)
#define CAPTURE_FLAG_OVERSIZE	(1u << 4)
#define CAPTURE_FLAG_PRESERVE	(1u << 5)	/* set by KEYSET watcher (M5) */

typedef struct {
	uint32_t timestamp_us;
	uint16_t length;
	uint8_t direction;
	uint8_t flags;
} capture_rec_hdr_t;

/*
 * Pack a header + frame body into a flat buffer. Returns total bytes
 * written, or negative on error (cap too small, oversize length).
 */
int capture_rec_pack(uint8_t *out, size_t cap,
		     const capture_rec_hdr_t *hdr,
		     const uint8_t *frame);

/*
 * Inverse of pack(). Reads up to `len` bytes from `in`, fills `hdr`, and
 * returns a pointer to the frame body inside `in`. Returns total bytes
 * consumed (header + body) on success, negative on error.
 */
int capture_rec_unpack(const uint8_t *in, size_t len,
		       capture_rec_hdr_t *hdr,
		       const uint8_t **frame_out);

#endif /* MELLON_CAPTURE_REC_H_ */
