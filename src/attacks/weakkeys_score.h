/*
 * Plausibility scoring for trial-decrypted OSDP plaintext (spec §6 M4).
 *
 * Pure C, host-testable. Given a buffer of supposed plaintext bytes from
 * a trial AES-CBC decryption, returns a heuristic score 0..100 of how
 * plausibly it's an actual OSDP frame body. The trial-decryptor uses a
 * threshold of 60 to declare a match.
 *
 * Why heuristic: AES-CBC decrypts to *some* output for *any* key; we need
 * to distinguish "real plaintext" from "random-looking garbage". Real
 * OSDP frame bodies have a very small command-byte alphabet and lengths
 * that match the encrypted-frame size; that's what the score keys on.
 */

#ifndef MELLON_WEAKKEYS_SCORE_H_
#define MELLON_WEAKKEYS_SCORE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MELLON_WEAK_SCORE_MIN_MATCH	60

/*
 * Score a candidate plaintext. `bytes`/`len` is the decrypted block(s);
 * `expected_cmd` is the command byte from the original (encrypted)
 * frame's header — present in the clear because OSDP only encrypts the
 * payload, not the framing header.
 *
 * Returns 0..100. ≥ MELLON_WEAK_SCORE_MIN_MATCH counts as a match.
 */
uint8_t mellon_weak_score(const uint8_t *bytes, size_t len, uint8_t expected_cmd);

#endif /* MELLON_WEAKKEYS_SCORE_H_ */
