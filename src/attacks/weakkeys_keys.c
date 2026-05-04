#include "weakkeys_keys.h"

#include <string.h>

/*
 * Documented vendor / common defaults. Sourced from public security advisories
 * and the BF research that motivated this firmware (spec §0). Add to this
 * list as new defaults are discovered; do NOT remove entries — the list is
 * append-only so old captures remain decryptable across firmware upgrades.
 */
static const uint8_t k_vendor_defaults[MELLON_WEAK_KEY_VENDOR_COUNT][MELLON_WEAK_KEY_LEN] = {
	/* 1. SIA OSDP "default install key" mentioned in BF Black Hat 2023. */
	{ 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	  0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F },
	/* 2. ASCII "0123456789ABCDEF" — common documentation example. */
	{ '0', '1', '2', '3', '4', '5', '6', '7',
	  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' },
	/* 3. ASCII "OSDP-DEFAULT-KEY" (16 chars). */
	{ 'O', 'S', 'D', 'P', '-', 'D', 'E', 'F',
	  'A', 'U', 'L', 'T', '-', 'K', 'E', 'Y' },
	/* 4. ASCII "AAAAAAAAAAAAAAAA". */
	{ 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A',
	  'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A' },
	/* 5. ASCII "0000000000000000". */
	{ '0', '0', '0', '0', '0', '0', '0', '0',
	  '0', '0', '0', '0', '0', '0', '0', '0' },
	/* 6. ASCII "1234567812345678". */
	{ '1', '2', '3', '4', '5', '6', '7', '8',
	  '1', '2', '3', '4', '5', '6', '7', '8' },
	/* 7. ASCII "PASSWORDPASSWORD". */
	{ 'P', 'A', 'S', 'S', 'W', 'O', 'R', 'D',
	  'P', 'A', 'S', 'S', 'W', 'O', 'R', 'D' },
	/* 8. ASCII "OSDPSECRETOSDPSE". */
	{ 'O', 'S', 'D', 'P', 'S', 'E', 'C', 'R',
	  'E', 'T', 'O', 'S', 'D', 'P', 'S', 'E' },
	/* 9. NIST AES test vector key (FIPS 197 example). */
	{ 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
	  0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C },
	/* 10. ASCII "OSDPOSDPOSDPOSDP". */
	{ 'O', 'S', 'D', 'P', 'O', 'S', 'D', 'P',
	  'O', 'S', 'D', 'P', 'O', 'S', 'D', 'P' },
	/* 11. Sequential bytes starting at 0x10. */
	{ 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	  0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F },
	/* 12. ASCII "TESTKEYTESTKEY12". */
	{ 'T', 'E', 'S', 'T', 'K', 'E', 'Y', 'T',
	  'E', 'S', 'T', 'K', 'E', 'Y', '1', '2' },
	/* 13. Repeating 0xDE 0xAD 0xBE 0xEF (debug/dev). */
	{ 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
	  0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF },
	/* 14. Repeating 0xCA 0xFE 0xBA 0xBE. */
	{ 0xCA, 0xFE, 0xBA, 0xBE, 0xCA, 0xFE, 0xBA, 0xBE,
	  0xCA, 0xFE, 0xBA, 0xBE, 0xCA, 0xFE, 0xBA, 0xBE },
};

static const char *const k_vendor_names[MELLON_WEAK_KEY_VENDOR_COUNT] = {
	"sia-default",
	"hex-ascii",
	"osdp-default-key",
	"all-A",
	"all-zero-ascii",
	"1234567812345678",
	"password-x2",
	"osdpsecret",
	"nist-fips197",
	"osdp-x4",
	"seq-0x10",
	"testkey-12",
	"deadbeef-x4",
	"cafebabe-x4",
};

size_t mellon_weak_keys_build(mellon_weak_key_t *out, size_t cap)
{
	if (!out) {
		return 0;
	}
	size_t i = 0;

	/* 1. Single-byte fills 0x00 .. 0xFF. */
	for (int b = 0; b < 256 && i < cap; b++, i++) {
		out[i].name = NULL;	/* synthesised at print-time */
		memset(out[i].key, (uint8_t)b, MELLON_WEAK_KEY_LEN);
	}

	/* 2a. Monotonic ascending 0x00..0x0F. */
	if (i < cap) {
		out[i].name = "asc-00-0F";
		for (int j = 0; j < MELLON_WEAK_KEY_LEN; j++) {
			out[i].key[j] = (uint8_t)j;
		}
		i++;
	}

	/* 2b. Monotonic descending 0xFF..0xF0. */
	if (i < cap) {
		out[i].name = "desc-FF-F0";
		for (int j = 0; j < MELLON_WEAK_KEY_LEN; j++) {
			out[i].key[j] = (uint8_t)(0xFF - j);
		}
		i++;
	}

	/* 3. Documented vendor defaults. */
	for (size_t v = 0; v < MELLON_WEAK_KEY_VENDOR_COUNT && i < cap; v++, i++) {
		out[i].name = k_vendor_names[v];
		memcpy(out[i].key, k_vendor_defaults[v], MELLON_WEAK_KEY_LEN);
	}

	return i;
}
