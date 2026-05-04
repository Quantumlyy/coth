/*
 * Weak-key candidate generator + plausibility scorer tests.
 *
 * The full mbedTLS-driven trial-decrypt loop in weakkeys.c is hardware-only
 * (mbedTLS is wired through Zephyr); the candidate list and scoring
 * function are pure C and exercised here.
 */

#include <unity.h>
#include <stdint.h>
#include <string.h>

#include "../../src/attacks/weakkeys_keys.c"
#include "../../src/attacks/weakkeys_score.c"

void setUp(void) {}
void tearDown(void) {}

/* The candidate generator must always produce 256+2+vendor entries. */
void test_weakkeys_candidate_count(void)
{
	mellon_weak_key_t list[MELLON_WEAK_KEY_CANDIDATE_MAX];
	size_t n = mellon_weak_keys_build(list, MELLON_WEAK_KEY_CANDIDATE_MAX);
	TEST_ASSERT_EQUAL_size_t(256 + 2 + MELLON_WEAK_KEY_VENDOR_COUNT, n);
}

/* The first 256 entries must be single-byte fills 0x00..0xFF in order. */
void test_weakkeys_single_byte_fills(void)
{
	mellon_weak_key_t list[MELLON_WEAK_KEY_CANDIDATE_MAX];
	(void)mellon_weak_keys_build(list, MELLON_WEAK_KEY_CANDIDATE_MAX);

	for (int b = 0; b < 256; b++) {
		for (int i = 0; i < MELLON_WEAK_KEY_LEN; i++) {
			TEST_ASSERT_EQUAL_HEX8(b, list[b].key[i]);
		}
	}
}

/* Spec example weak key: 0x04 repeated 16 times must be at index 4. */
void test_weakkeys_spec_example_present(void)
{
	mellon_weak_key_t list[MELLON_WEAK_KEY_CANDIDATE_MAX];
	(void)mellon_weak_keys_build(list, MELLON_WEAK_KEY_CANDIDATE_MAX);

	for (int i = 0; i < MELLON_WEAK_KEY_LEN; i++) {
		TEST_ASSERT_EQUAL_HEX8(0x04, list[4].key[i]);
	}
}

/* Monotonic ascending key 0x00..0x0F must be present. */
void test_weakkeys_monotonic_asc_present(void)
{
	mellon_weak_key_t list[MELLON_WEAK_KEY_CANDIDATE_MAX];
	size_t n = mellon_weak_keys_build(list, MELLON_WEAK_KEY_CANDIDATE_MAX);

	bool found = false;
	for (size_t k = 0; k < n; k++) {
		bool match = true;
		for (int j = 0; j < MELLON_WEAK_KEY_LEN; j++) {
			if (list[k].key[j] != (uint8_t)j) {
				match = false;
				break;
			}
		}
		if (match) {
			found = true;
			break;
		}
	}
	TEST_ASSERT_TRUE(found);
}

/* Buffer cap is honoured. */
void test_weakkeys_respects_cap(void)
{
	mellon_weak_key_t list[10];
	size_t n = mellon_weak_keys_build(list, 10);
	TEST_ASSERT_EQUAL_size_t(10, n);
}

/* Plausibility scorer: a buffer that decrypts to "starts with osdp_POLL"
 * (0x60) and has low entropy should score above the match threshold. */
void test_weakkeys_score_real_plaintext(void)
{
	uint8_t plain[16] = {
		0x60,	/* osdp_POLL */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	uint8_t s = mellon_weak_score(plain, sizeof(plain), 0x60);
	TEST_ASSERT_GREATER_OR_EQUAL_UINT8(MELLON_WEAK_SCORE_MIN_MATCH, s);
}

/* High-entropy random-looking buffer should score below the threshold. */
void test_weakkeys_score_random_garbage(void)
{
	uint8_t garbage[16] = {
		0x9F, 0xA3, 0x12, 0x6E, 0xC4, 0x80, 0x55, 0x11,
		0x73, 0xEE, 0x2A, 0x4D, 0xBA, 0x07, 0xCD, 0xF2,
	};
	uint8_t s = mellon_weak_score(garbage, sizeof(garbage), 0x60);
	TEST_ASSERT_LESS_THAN_UINT8(MELLON_WEAK_SCORE_MIN_MATCH, s);
}

/*
 * First-byte-plausibility: a buffer starting with 0xAA (out of OSDP
 * command range) should score below the threshold even if low entropy.
 */
void test_weakkeys_score_implausible_first_byte(void)
{
	uint8_t plain[16] = {
		0xAA,	/* not an OSDP cmd/reply */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	uint8_t s = mellon_weak_score(plain, sizeof(plain), 0x60);
	TEST_ASSERT_LESS_THAN_UINT8(MELLON_WEAK_SCORE_MIN_MATCH, s);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_weakkeys_candidate_count);
	RUN_TEST(test_weakkeys_single_byte_fills);
	RUN_TEST(test_weakkeys_spec_example_present);
	RUN_TEST(test_weakkeys_monotonic_asc_present);
	RUN_TEST(test_weakkeys_respects_cap);
	RUN_TEST(test_weakkeys_score_real_plaintext);
	RUN_TEST(test_weakkeys_score_random_garbage);
	RUN_TEST(test_weakkeys_score_implausible_first_byte);
	return UNITY_END();
}
