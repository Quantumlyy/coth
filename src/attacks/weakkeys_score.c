#include "weakkeys_score.h"

/*
 * Known OSDP commands (CP→PD) and replies (PD→CP). This is a coarse
 * filter — the real spec has ~50 command codes; we use it to reject
 * plaintexts where the supposed first byte makes no sense.
 *
 * Bit `i` set means byte value `i` is plausible. Sparse enough to fit
 * in 32 bytes (256 bits).
 */
static const uint8_t k_plausible_first_bytes[32] = {
	/* 0x00-0x1F */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x20-0x3F */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* 0x40-0x5F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x60-0x7F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x80-0xFF */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* …          */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* …          */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* …          */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static bool first_byte_plausible(uint8_t b)
{
	return (k_plausible_first_bytes[b / 8] & (1u << (b % 8))) != 0;
}

uint8_t mellon_weak_score(const uint8_t *bytes, size_t len, uint8_t expected_cmd)
{
	if (!bytes || len < 1) {
		return 0;
	}

	int score = 0;

	/* Component 1: first byte is in the OSDP command/reply alphabet (45 pts). */
	if (first_byte_plausible(bytes[0])) {
		score += 45;
	}

	/* Component 2: first byte equals the expected_cmd we already saw on the
	 * wire (15 pts). For PD replies the stored command is the reply code. */
	if (expected_cmd != 0 && bytes[0] == expected_cmd) {
		score += 15;
	}

	/*
	 * Component 3: low entropy within the plaintext, measured as byte
	 * variety. Random ciphertext-XOR-junk has near-uniform byte distribution
	 * (~256 unique bytes for a 16-byte block is unusual but possible). Real
	 * plaintexts of access-control commands have lots of zeros / ASCII and
	 * a small unique-byte count. We reward unique_bytes ≤ 8 in a 16-byte
	 * decrypted block.
	 */
	bool seen[256] = { false };
	int unique = 0;
	const size_t scan_len = (len > 16) ? 16 : len;
	for (size_t i = 0; i < scan_len; i++) {
		if (!seen[bytes[i]]) {
			seen[bytes[i]] = true;
			unique++;
		}
	}
	if (unique <= 8) {
		score += 30;
	} else if (unique <= 11) {
		score += 15;
	}

	/* Component 4: a trailing PKCS#7-style pad would be a bonus, but OSDP
	 * uses a fixed-block-aligned scheme so this is rarely useful. Skip for
	 * v1; reserved for a refinement pass. */

	if (score > 100) {
		score = 100;
	}
	return (uint8_t)score;
}
