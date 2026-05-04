/*
 * libosdp glue — soft-gated wiring for the compliant OSDP state machine.
 *
 * When HAS_LIBOSDP is defined, this file translates between the Mellon
 * permissive parser's frame stream and libosdp's compliant frame intake,
 * giving us SC-state tracking and sequence validation in addition to the
 * raw-frame view the attacker cares about.
 *
 * When HAS_LIBOSDP is *not* defined, every function here is a no-op stub
 * and the firmware operates on the raw parser only. See spec §3.3 and
 * docs/deviations.md §3.
 */

#ifndef MELLON_LIBOSDP_GLUE_H_
#define MELLON_LIBOSDP_GLUE_H_

#include <stdbool.h>
#include "framing.h"

/* Initialise the libosdp instance, if compiled in. Safe to call always. */
int mellon_libosdp_init(void);

/* Feed a raw frame to the compliant parser, in addition to whatever the
 * sniffer is doing with it. The frame reference is valid only for the
 * duration of the call. */
void mellon_libosdp_feed(const osdp_frame_t *frame);

/* Returns true if libosdp is wired in and tracking SC state. The BLE
 * console reports this in `status` so the operator knows whether
 * decryption-on-the-fly is available. */
bool mellon_libosdp_present(void);

#endif /* MELLON_LIBOSDP_GLUE_H_ */
