/*
 * Permissive OSDP framer tests.
 *
 * Spec §3.3 says the framer must accept frames libosdp would reject (bad CRC,
 * bad MAC, out-of-sequence) because attackers care about malformed frames.
 * That property is non-trivial to test for: passing the test means we *do*
 * emit malformed frames with the right flag set, not that we drop them.
 */

#include <unity.h>
#include <stdint.h>
#include <string.h>

#include "../../../src/osdp/framing.c"

#define MAX_CAPTURED 16

static struct {
	uint8_t bytes[OSDP_FRAME_MAX_LEN];
	uint16_t length;
	uint8_t flags;
	osdp_dir_t direction;
	uint8_t address;
	uint8_t cmd;
} captured[MAX_CAPTURED];
static size_t captured_count;

static void capture_cb(const osdp_frame_t *frame, void *user)
{
	(void)user;
	if (captured_count >= MAX_CAPTURED) {
		return;
	}
	memcpy(captured[captured_count].bytes, frame->bytes, frame->length);
	captured[captured_count].length = frame->length;
	captured[captured_count].flags = frame->flags;
	captured[captured_count].direction = frame->direction;
	captured[captured_count].address = frame->address;
	captured[captured_count].cmd = frame->cmd_or_reply;
	captured_count++;
}

static void with_crc(uint8_t *frame, size_t total)
{
	uint16_t crc = osdp_crc16(frame, total - 2);
	frame[total - 2] = crc & 0xff;
	frame[total - 1] = (crc >> 8) & 0xff;
}

void setUp(void)
{
	memset(captured, 0, sizeof(captured));
	captured_count = 0;
}
void tearDown(void) {}

/* A clean poll → exactly one frame, valid CRC, direction CP→PD. */
void test_framing_clean_poll(void)
{
	osdp_framer_t f;
	osdp_framer_init(&f, capture_cb, NULL);

	uint8_t frame[8] = { 0x53, 0x00, 0x08, 0x00, 0x00, 0x60, 0x00, 0x00 };
	with_crc(frame, sizeof(frame));
	osdp_framer_feed(&f, frame, sizeof(frame));

	TEST_ASSERT_EQUAL_size_t(1, captured_count);
	TEST_ASSERT_EQUAL(0, captured[0].flags & OSDP_FRAME_FLAG_BAD_CRC);
	TEST_ASSERT_EQUAL(OSDP_DIR_CP_TO_PD, captured[0].direction);
	TEST_ASSERT_EQUAL_HEX8(0x60, captured[0].cmd);
	TEST_ASSERT_EQUAL_UINT32(1, f.frames_total);
}

/* A reply (address bit 7 set) → direction PD→CP. */
void test_framing_reply_direction(void)
{
	osdp_framer_t f;
	osdp_framer_init(&f, capture_cb, NULL);

	uint8_t frame[8] = { 0x53, 0x80, 0x08, 0x00, 0x00, 0x40, 0x00, 0x00 };
	with_crc(frame, sizeof(frame));
	osdp_framer_feed(&f, frame, sizeof(frame));

	TEST_ASSERT_EQUAL_size_t(1, captured_count);
	TEST_ASSERT_EQUAL(OSDP_DIR_PD_TO_CP, captured[0].direction);
	TEST_ASSERT_EQUAL_HEX8(0x80, captured[0].address);
}

/*
 * Permissive contract: a bad-CRC frame must STILL be emitted, with the
 * BAD_CRC flag set. Dropping it would defeat the whole point of the
 * permissive parser (spec §3.3).
 */
void test_framing_bad_crc_is_emitted(void)
{
	osdp_framer_t f;
	osdp_framer_init(&f, capture_cb, NULL);

	uint8_t frame[8] = { 0x53, 0x00, 0x08, 0x00, 0x00, 0x60, 0xDE, 0xAD };
	osdp_framer_feed(&f, frame, sizeof(frame));

	TEST_ASSERT_EQUAL_size_t(1, captured_count);
	TEST_ASSERT_TRUE(captured[0].flags & OSDP_FRAME_FLAG_BAD_CRC);
	TEST_ASSERT_EQUAL_UINT32(1, f.frames_bad_crc);
}

/* SCB-control-bit set → flag the frame as encrypted. */
void test_framing_scb_flag(void)
{
	osdp_framer_t f;
	osdp_framer_init(&f, capture_cb, NULL);

	uint8_t frame[8] = { 0x53, 0x00, 0x08, 0x00, OSDP_CTRL_SCB, 0x60, 0x00, 0x00 };
	with_crc(frame, sizeof(frame));
	osdp_framer_feed(&f, frame, sizeof(frame));

	TEST_ASSERT_EQUAL_size_t(1, captured_count);
	TEST_ASSERT_TRUE(captured[0].flags & OSDP_FRAME_FLAG_ENCRYPTED);
}

/*
 * Frame split across two calls — common in async UART RX where bytes arrive
 * in chunks. Must reassemble correctly.
 */
void test_framing_split_buffers(void)
{
	osdp_framer_t f;
	osdp_framer_init(&f, capture_cb, NULL);

	uint8_t frame[8] = { 0x53, 0x00, 0x08, 0x00, 0x00, 0x60, 0x00, 0x00 };
	with_crc(frame, sizeof(frame));

	osdp_framer_feed(&f, frame, 3);
	TEST_ASSERT_EQUAL_size_t(0, captured_count);
	osdp_framer_feed(&f, frame + 3, 5);
	TEST_ASSERT_EQUAL_size_t(1, captured_count);
}

/*
 * Garbage before SOM is silently discarded. The framer should not
 * emit a frame for a stretch of 0xFF bytes followed by a real frame.
 */
void test_framing_garbage_then_frame(void)
{
	osdp_framer_t f;
	osdp_framer_init(&f, capture_cb, NULL);

	uint8_t garbage[] = { 0xff, 0xff, 0xaa, 0x55, 0x00, 0xff };
	osdp_framer_feed(&f, garbage, sizeof(garbage));
	TEST_ASSERT_EQUAL_size_t(0, captured_count);

	uint8_t frame[8] = { 0x53, 0x00, 0x08, 0x00, 0x00, 0x60, 0x00, 0x00 };
	with_crc(frame, sizeof(frame));
	osdp_framer_feed(&f, frame, sizeof(frame));
	TEST_ASSERT_EQUAL_size_t(1, captured_count);
}

/*
 * An implausibly short length field (< OSDP_MIN_FRAME_LEN) triggers a resync,
 * NOT a partial frame emission.
 */
void test_framing_short_length_resyncs(void)
{
	osdp_framer_t f;
	osdp_framer_init(&f, capture_cb, NULL);

	uint8_t bogus[] = { 0x53, 0x00, 0x02, 0x00, 0x00, 0x60, 0x00, 0x00 };
	osdp_framer_feed(&f, bogus, sizeof(bogus));
	TEST_ASSERT_EQUAL_size_t(0, captured_count);
	TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1, f.resync_count);
}

/*
 * Oversize length triggers resync and bumps oversize_count. This is the
 * defense against an attacker injecting a length=0xFFFF garbage frame to
 * stall the parser.
 */
void test_framing_oversize_length_resyncs(void)
{
	osdp_framer_t f;
	osdp_framer_init(&f, capture_cb, NULL);

	uint8_t huge[] = { 0x53, 0x00, 0xff, 0xff, 0x00, 0x60 };
	osdp_framer_feed(&f, huge, sizeof(huge));

	TEST_ASSERT_EQUAL_size_t(0, captured_count);
	TEST_ASSERT_EQUAL_UINT32(1, f.oversize_count);
}

/*
 * Two frames back-to-back in a single feed() call must both emit.
 */
void test_framing_back_to_back(void)
{
	osdp_framer_t f;
	osdp_framer_init(&f, capture_cb, NULL);

	uint8_t buf[16];
	uint8_t one[8] = { 0x53, 0x00, 0x08, 0x00, 0x00, 0x60, 0x00, 0x00 };
	uint8_t two[8] = { 0x53, 0x80, 0x08, 0x00, 0x00, 0x40, 0x00, 0x00 };
	with_crc(one, sizeof(one));
	with_crc(two, sizeof(two));
	memcpy(buf, one, 8);
	memcpy(buf + 8, two, 8);

	osdp_framer_feed(&f, buf, sizeof(buf));

	TEST_ASSERT_EQUAL_size_t(2, captured_count);
	TEST_ASSERT_EQUAL(OSDP_DIR_CP_TO_PD, captured[0].direction);
	TEST_ASSERT_EQUAL(OSDP_DIR_PD_TO_CP, captured[1].direction);
}

/*
 * A SOM byte appearing inside what looks like the body of an oversize
 * frame must be picked up by the resync logic, so we don't drop the
 * subsequent valid frame.
 */
void test_framing_resync_finds_inner_som(void)
{
	osdp_framer_t f;
	osdp_framer_init(&f, capture_cb, NULL);

	/* Oversize header followed by a real poll frame. */
	uint8_t bogus[] = { 0x53, 0x00, 0xff, 0xff, 0x00 };
	uint8_t frame[8] = { 0x53, 0x00, 0x08, 0x00, 0x00, 0x60, 0x00, 0x00 };
	with_crc(frame, sizeof(frame));

	osdp_framer_feed(&f, bogus, sizeof(bogus));
	osdp_framer_feed(&f, frame, sizeof(frame));

	TEST_ASSERT_EQUAL_size_t(1, captured_count);
	TEST_ASSERT_EQUAL(0, captured[0].flags & OSDP_FRAME_FLAG_BAD_CRC);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_framing_clean_poll);
	RUN_TEST(test_framing_reply_direction);
	RUN_TEST(test_framing_bad_crc_is_emitted);
	RUN_TEST(test_framing_scb_flag);
	RUN_TEST(test_framing_split_buffers);
	RUN_TEST(test_framing_garbage_then_frame);
	RUN_TEST(test_framing_short_length_resyncs);
	RUN_TEST(test_framing_oversize_length_resyncs);
	RUN_TEST(test_framing_back_to_back);
	RUN_TEST(test_framing_resync_finds_inner_som);
	return UNITY_END();
}
