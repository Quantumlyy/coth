/*
 * BLE console (NUS-backed) — operator command channel.
 *
 * Spec §3.4. Commands are line-oriented ASCII over Nordic UART Service.
 * Pairing: passkey-only (no "just works"), 6-digit fixed by default.
 *
 * The console is the v1 transport. v2+ will add a structured GATT service
 * (spec §3.5) layered on top of this state machine.
 */

#ifndef MELLON_BLE_NUS_CONSOLE_H_
#define MELLON_BLE_NUS_CONSOLE_H_

#include <stddef.h>
#include <stdint.h>

#define MELLON_DEFAULT_PASSKEY	123456

int mellon_ble_console_init(uint32_t fixed_passkey);

/* Asynchronously emit a line to whichever NUS peer is connected. Safe to
 * call from any context. */
void mellon_ble_console_print(const char *fmt, ...);

#endif /* MELLON_BLE_NUS_CONSOLE_H_ */
