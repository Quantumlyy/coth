/*
 * Persistent configuration storage.
 *
 * Backed by Zephyr NVS in the partition at 0x70000–0x77FFF (32 KB; spec §5).
 * Holds: current mode, BLE passkey override, optional preserved-frame
 * cookies for the M5 KEYSET watcher.
 */

#ifndef MELLON_NVS_CONFIG_H_
#define MELLON_NVS_CONFIG_H_

#include <stdint.h>
#include "../mode.h"

int mellon_nvs_init(void);

int mellon_nvs_put_mode(mellon_mode_t mode);
int mellon_nvs_get_mode(mellon_mode_t *out);

int mellon_nvs_put_passkey(uint32_t passkey);
int mellon_nvs_get_passkey(uint32_t *out);

/*
 * Preserved-frame slots for the M5 KEYSET watcher. The flash log (FCB)
 * may rotate captured frames out, but a KEYSET hit is too important to
 * lose; we keep up to MELLON_PRESERVED_SLOTS copies in NVS, indexed by
 * slot number. Slots are recycled oldest-first when full.
 */
#define MELLON_PRESERVED_SLOTS	8

int mellon_nvs_put_preserved(uint8_t slot, const uint8_t *bytes, size_t len);
int mellon_nvs_get_preserved(uint8_t slot, uint8_t *out, size_t cap, size_t *len_out);
int mellon_nvs_clear_preserved(void);

#endif /* MELLON_NVS_CONFIG_H_ */
