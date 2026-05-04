/*
 * Mellon firmware — entry point (skeleton).
 *
 * This commit only proves the toolchain works end-to-end. M0 bring-up wires
 * up GPIO/LEDs/RTT in the next commit; the mode dispatcher lands in M2.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#ifndef MELLON_FIRMWARE_VERSION
#define MELLON_FIRMWARE_VERSION "0.0.0-pre"
#endif

#ifndef MELLON_BUILD_HASH
#define MELLON_BUILD_HASH "unknown"
#endif

int main(void)
{
	printk("\n");
	printk("Mellon firmware %s (%s)\n", MELLON_FIRMWARE_VERSION, MELLON_BUILD_HASH);
	printk("Skeleton boot — no modes wired yet. See docs/quickstart.md.\n");
	return 0;
}
