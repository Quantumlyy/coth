/*
 * KEYSET match tests.
 *
 * Three classes of guard here:
 *   1. Positive matches (a real KEYSET frame is recognised, key is extracted)
 *   2. Direction filter (PD→CP CMD=0x75 is something else and must NOT match)
 *   3. Length / payload-shape filters (truncated KEYSETs must not match)
 *
 * The third category is non-obvious: a real attacker won't send malformed
 * KEYSETs, but a noisy bus might. We don't want a phantom match latching
 * the watcher on garbage.
 */

#include <unity.h>
#include <string.h>

#include "../../src/attacks/keyset_match.c"
#include "../../src/osdp/framing.c"

void setUp(void) {}
void tearDown(void) {}

static osdp_frame_t make_frame(const uint8_t *bytes, uint16_t len,
			       osdp_dir_t dir, uint8_t cmd)
{
	osdp_frame_t f = {
		.bytes = bytes,
		.length = len,
		.direction = dir,
		.address = bytes[OSDP_OFF_ADDR],
		.control = bytes[OSDP_OFF_CTRL],
		.cmd_or_reply = cmd,
		.flags = 0,
	};
	return f;
}

void test_keyset_matches_real_payload(void)
{
	/* CP→PD KEYSET addressed to PD 0x0A, key type SCBK, key len 16,
	 * key = 04 04 04 04 ... (the spec's example weak-key candidate). */
	uint8_t buf[] = {
		0x53, 0x0A,			/* SOM, addr=0x0A */
		(uint8_t)(5 + 1 + 2 + 16 + 2), 0x00,	/* len LSB, MSB */
		0x00,				/* control */
		0x75,				/* CMD = osdp_KEYSET */
		0x01, 0x10,			/* type=SCBK, len=16 */
		0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
		0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
		0x00, 0x00,			/* CRC placeholder */
	};
	osdp_frame_t f = make_frame(buf, sizeof(buf), OSDP_DIR_CP_TO_PD, 0x75);

	keyset_match_t m;
	TEST_ASSERT_TRUE(keyset_match(&f, &m));
	TEST_ASSERT_EQUAL_HEX8(0x0A, m.address);
	for (int i = 0; i < 16; i++) {
		TEST_ASSERT_EQUAL_HEX8(0x04, m.scbk[i]);
	}
}

/* PD→CP CMD=0x75 must NOT match — that's a reply byte, not a command,
 * and the layout of the payload is different. */
void test_keyset_rejects_pd_to_cp(void)
{
	uint8_t buf[26] = { 0x53, 0x80, 0x1A, 0x00, 0x00, 0x75 };
	osdp_frame_t f = make_frame(buf, sizeof(buf), OSDP_DIR_PD_TO_CP, 0x75);
	TEST_ASSERT_FALSE(keyset_match(&f, NULL));
}

/* Different command code → not a KEYSET. */
void test_keyset_rejects_other_cmds(void)
{
	uint8_t buf[26] = { 0x53, 0x00, 0x1A, 0x00, 0x00, 0x60 };	/* POLL */
	osdp_frame_t f = make_frame(buf, sizeof(buf), OSDP_DIR_CP_TO_PD, 0x60);
	TEST_ASSERT_FALSE(keyset_match(&f, NULL));
}

/* Truncated KEYSET (length too short for the 16-byte key) must NOT match. */
void test_keyset_rejects_truncated(void)
{
	uint8_t buf[10] = {
		0x53, 0x00, 0x0A, 0x00, 0x00, 0x75, 0x01, 0x10, 0x00, 0x00,
	};
	osdp_frame_t f = make_frame(buf, sizeof(buf), OSDP_DIR_CP_TO_PD, 0x75);
	TEST_ASSERT_FALSE(keyset_match(&f, NULL));
}

/* Wrong key type byte (e.g. 0x02 = some other key) must NOT match. */
void test_keyset_rejects_wrong_key_type(void)
{
	uint8_t buf[26] = {
		0x53, 0x00, 0x1A, 0x00, 0x00, 0x75,
		0x02, 0x10,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0x00, 0x00,
	};
	osdp_frame_t f = make_frame(buf, sizeof(buf), OSDP_DIR_CP_TO_PD, 0x75);
	TEST_ASSERT_FALSE(keyset_match(&f, NULL));
}

/* Wrong key length (e.g. 0x08 instead of 0x10) must NOT match. */
void test_keyset_rejects_wrong_key_len(void)
{
	uint8_t buf[26] = {
		0x53, 0x00, 0x1A, 0x00, 0x00, 0x75,
		0x01, 0x08,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0x00, 0x00,
	};
	osdp_frame_t f = make_frame(buf, sizeof(buf), OSDP_DIR_CP_TO_PD, 0x75);
	TEST_ASSERT_FALSE(keyset_match(&f, NULL));
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_keyset_matches_real_payload);
	RUN_TEST(test_keyset_rejects_pd_to_cp);
	RUN_TEST(test_keyset_rejects_other_cmds);
	RUN_TEST(test_keyset_rejects_truncated);
	RUN_TEST(test_keyset_rejects_wrong_key_type);
	RUN_TEST(test_keyset_rejects_wrong_key_len);
	return UNITY_END();
}
