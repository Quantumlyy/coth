/*
 * Capture record format tests.
 *
 * The Zephyr-bound capture.c can't run on native_posix, but the byte-layout
 * of a record is the contract between the firmware and tools/parse_capture.py.
 * Drift here would silently corrupt every captured frame.
 */

#include <unity.h>
#include <stdint.h>
#include <string.h>

#include "../../src/storage/capture_rec.c"

void setUp(void) {}
void tearDown(void) {}

/* Round-trip identity: pack(...) then unpack(...) recovers everything. */
void test_capture_rec_round_trip(void)
{
	const uint8_t frame[] = { 0x53, 0x00, 0x08, 0x00, 0x00, 0x60, 0x12, 0x34 };
	capture_rec_hdr_t hdr = {
		.timestamp_us = 0xDEADBEEFu,
		.length = sizeof(frame),
		.direction = CAPTURE_DIR_CP_TO_PD,
		.flags = CAPTURE_FLAG_BAD_CRC | CAPTURE_FLAG_PRESERVE,
	};

	uint8_t buf[CAPTURE_REC_MAX];
	int packed = capture_rec_pack(buf, sizeof(buf), &hdr, frame);
	TEST_ASSERT_EQUAL_INT(CAPTURE_REC_HDR_LEN + (int)sizeof(frame), packed);

	capture_rec_hdr_t out_hdr;
	const uint8_t *out_frame = NULL;
	int consumed = capture_rec_unpack(buf, packed, &out_hdr, &out_frame);
	TEST_ASSERT_EQUAL_INT(packed, consumed);
	TEST_ASSERT_EQUAL_HEX32(hdr.timestamp_us, out_hdr.timestamp_us);
	TEST_ASSERT_EQUAL_UINT16(hdr.length, out_hdr.length);
	TEST_ASSERT_EQUAL_HEX8(hdr.direction, out_hdr.direction);
	TEST_ASSERT_EQUAL_HEX8(hdr.flags, out_hdr.flags);
	TEST_ASSERT_NOT_NULL(out_frame);
	TEST_ASSERT_EQUAL_MEMORY(frame, out_frame, sizeof(frame));
}

/*
 * Endianness: timestamp and length are little-endian. The host-side
 * parser depends on this; if someone "optimises" the firmware to host
 * order, the host parser breaks.
 */
void test_capture_rec_explicit_le_layout(void)
{
	capture_rec_hdr_t hdr = {
		.timestamp_us = 0x11223344u,
		.length = 0x5566,
		.direction = 0x77,
		.flags = 0x88,
	};

	uint8_t buf[CAPTURE_REC_MAX];
	int n = capture_rec_pack(buf, sizeof(buf), &hdr, NULL);
	TEST_ASSERT_EQUAL_INT(CAPTURE_REC_HDR_LEN, n);

	TEST_ASSERT_EQUAL_HEX8(0x44, buf[0]);	/* ts LSB */
	TEST_ASSERT_EQUAL_HEX8(0x33, buf[1]);
	TEST_ASSERT_EQUAL_HEX8(0x22, buf[2]);
	TEST_ASSERT_EQUAL_HEX8(0x11, buf[3]);	/* ts MSB */
	TEST_ASSERT_EQUAL_HEX8(0x66, buf[4]);	/* len LSB */
	TEST_ASSERT_EQUAL_HEX8(0x55, buf[5]);	/* len MSB */
	TEST_ASSERT_EQUAL_HEX8(0x77, buf[6]);
	TEST_ASSERT_EQUAL_HEX8(0x88, buf[7]);
}

/* Buffer-too-small returns negative. */
void test_capture_rec_short_buffer(void)
{
	capture_rec_hdr_t hdr = { .length = 100 };
	uint8_t small[16];
	int n = capture_rec_pack(small, sizeof(small), &hdr, (const uint8_t *)"x");
	TEST_ASSERT_LESS_THAN_INT(0, n);
}

/* Oversize length is rejected. */
void test_capture_rec_oversize(void)
{
	capture_rec_hdr_t hdr = { .length = CAPTURE_FRAME_MAX + 1 };
	uint8_t buf[CAPTURE_REC_MAX];
	int n = capture_rec_pack(buf, sizeof(buf), &hdr, (const uint8_t *)"x");
	TEST_ASSERT_LESS_THAN_INT(0, n);
}

/* Zero-length frame is permitted (synthetic markers, future use). */
void test_capture_rec_zero_length(void)
{
	capture_rec_hdr_t hdr = { .length = 0, .direction = CAPTURE_DIR_UNKNOWN };
	uint8_t buf[CAPTURE_REC_MAX];
	int n = capture_rec_pack(buf, sizeof(buf), &hdr, NULL);
	TEST_ASSERT_EQUAL_INT(CAPTURE_REC_HDR_LEN, n);

	capture_rec_hdr_t out_hdr;
	const uint8_t *out_frame = (const uint8_t *)0x1;	/* poison */
	int consumed = capture_rec_unpack(buf, n, &out_hdr, &out_frame);
	TEST_ASSERT_EQUAL_INT(CAPTURE_REC_HDR_LEN, consumed);
	TEST_ASSERT_NULL(out_frame);
}

/* Concatenated records can be walked end-to-end. The flash layout puts
 * record headers + bodies one after the other; tools/parse_capture.py
 * relies on capture_rec_unpack returning the byte advance for each. */
void test_capture_rec_streaming(void)
{
	uint8_t stream[3 * 16];
	size_t off = 0;
	for (int i = 0; i < 3; i++) {
		capture_rec_hdr_t hdr = {
			.timestamp_us = (uint32_t)i,
			.length = 8,
			.direction = CAPTURE_DIR_PD_TO_CP,
			.flags = 0,
		};
		uint8_t frame[8] = { 0x53, 0x80, 0x08, 0x00, 0x00, 0x40, 0x00, (uint8_t)i };
		int n = capture_rec_pack(stream + off, sizeof(stream) - off, &hdr, frame);
		TEST_ASSERT_GREATER_THAN_INT(0, n);
		off += (size_t)n;
	}
	TEST_ASSERT_EQUAL_size_t(48, off);

	size_t cur = 0;
	int decoded = 0;
	while (cur < off) {
		capture_rec_hdr_t hdr;
		const uint8_t *frame = NULL;
		int n = capture_rec_unpack(stream + cur, off - cur, &hdr, &frame);
		TEST_ASSERT_GREATER_THAN_INT(0, n);
		TEST_ASSERT_EQUAL_UINT32((uint32_t)decoded, hdr.timestamp_us);
		TEST_ASSERT_EQUAL_HEX8((uint8_t)decoded, frame[7]);
		cur += (size_t)n;
		decoded++;
	}
	TEST_ASSERT_EQUAL_INT(3, decoded);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_capture_rec_round_trip);
	RUN_TEST(test_capture_rec_explicit_le_layout);
	RUN_TEST(test_capture_rec_short_buffer);
	RUN_TEST(test_capture_rec_oversize);
	RUN_TEST(test_capture_rec_zero_length);
	RUN_TEST(test_capture_rec_streaming);
	return UNITY_END();
}
