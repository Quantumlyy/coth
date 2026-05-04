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

#endif /* MELLON_NVS_CONFIG_H_ */
