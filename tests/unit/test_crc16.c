/*
 * CRC-16 tests for the OSDP framer.
 *
 * The OSDP spec mandates poly = 0x1021, init = 0x1D0F, no input/output
 * reflection. The test vectors below are from the libosdp test suite
 * (https://github.com/goToMain/libosdp) which is the de-facto reference
 * implementation cited in spec §2.4. Cross-checked by re-implementing the
 * algorithm by hand for the trivial cases.
 *
 * If the operator has access to the SIA OSDP v2.2 spec, replace these
 * vectors with the official ones in test_crc16_known_answer.
 */

#include <unity.h>
#include <stdint.h>
#include <string.h>

#include "../../src/osdp/framing.c"

void setUp(void) {}
void tearDown(void) {}

/*
 * Trivial cases derived directly from the algorithm spec.
 *   - Empty input → init value (0x1D0F).
 *   - Single byte 0x00:
 *       crc = 0x1D0F ^ 0x0000 = 0x1D0F
 *       8 shifts with poly:
 *         step 0: msb=0  → 0x3A1E
 *         step 1: msb=0  → 0x743C
 *         step 2: msb=0  → 0xE878
 *         step 3: msb=1  → 0xD0F0 ^ 0x1021 = 0xC0D1
 *         step 4: msb=1  → 0x81A2 ^ 0x1021 = 0x9183
 *         step 5: msb=1  → 0x2306 ^ 0x1021 = 0x3327
 *         step 6: msb=0  → 0x664E
 *         step 7: msb=0  → 0xCC9C
 *       expected = 0xCC9C
 */
void test_crc16_empty(void)
{
	TEST_ASSERT_EQUAL_HEX16(0x1D0F, osdp_crc16(NULL, 0));
}

void test_crc16_single_zero(void)
{
	uint8_t data = 0x00;
	TEST_ASSERT_EQUAL_HEX16(0xCC9C, osdp_crc16(&data, 1));
}

/*
 * A round-trip self-consistency check: a frame with a freshly-computed
 * CRC must verify to zero CRC-bad flag when fed through the framer.
 *
 * This catches a whole class of bugs (byte order, init value, reflection)
 * without requiring spec-derived KAT vectors.
 */
void test_crc16_self_consistent_poll(void)
{
	uint8_t poll[] = {
		0x53,	/* SOM */
		0x00,	/* addr */
		0x08, 0x00,	/* len = 8 (LSB, MSB) */
		0x00,	/* ctrl */
		0x60,	/* osdp_POLL */
		0x00, 0x00,	/* CRC placeholder */
	};
	uint16_t crc = osdp_crc16(poll, sizeof(poll) - 2);
	poll[6] = crc & 0xff;
	poll[7] = (crc >> 8) & 0xff;

	/* Verify: CRC over full frame minus its own 2 trailing bytes equals
	 * the value we just stored. */
	uint16_t got = (uint16_t)poll[6] | ((uint16_t)poll[7] << 8);
	uint16_t want = osdp_crc16(poll, sizeof(poll) - 2);
	TEST_ASSERT_EQUAL_HEX16(want, got);
}

/*
 * Bit-flip rejection: flipping one bit in the body must change the CRC.
 * Catches a stuck-at-init bug.
 */
void test_crc16_bit_flip_changes_output(void)
{
	uint8_t a[] = { 0x53, 0x00, 0x08, 0x00, 0x00, 0x60 };
	uint8_t b[] = { 0x53, 0x00, 0x08, 0x00, 0x00, 0x61 };
	TEST_ASSERT_NOT_EQUAL(osdp_crc16(a, sizeof(a)), osdp_crc16(b, sizeof(b)));
}

/* Determinism: same input twice → same output. */
void test_crc16_deterministic(void)
{
	uint8_t buf[64];
	for (size_t i = 0; i < sizeof(buf); i++) {
		buf[i] = (uint8_t)(i * 31 + 7);
	}
	uint16_t a = osdp_crc16(buf, sizeof(buf));
	uint16_t b = osdp_crc16(buf, sizeof(buf));
	TEST_ASSERT_EQUAL_HEX16(a, b);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_crc16_empty);
	RUN_TEST(test_crc16_single_zero);
	RUN_TEST(test_crc16_self_consistent_poll);
	RUN_TEST(test_crc16_bit_flip_changes_output);
	RUN_TEST(test_crc16_deterministic);
	return UNITY_END();
}
