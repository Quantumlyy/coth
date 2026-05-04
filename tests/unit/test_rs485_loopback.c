/*
 * Host-side unit test for the RS-485 DE/RE̅ FSM.
 *
 * The full driver in src/board/rs485.c can't be exercised on native_posix
 * because it depends on Zephyr's UART async API. The state machine that
 * decides *when* DE is high vs. low is pure logic, so we test it directly.
 *
 * The catastrophic bug this guards against (spec §7.1): deasserting DE too
 * early truncates the last byte of every emitted frame. Concretely, that
 * means:
 *   - DE must be HIGH from the moment we call tx_begin() ...
 *   - ... until tx_done() fires from the UART layer (NOT from a count of
 *     bytes-written or any timer).
 *   - DE must drop on tx_done() and on tx_aborted().
 */

#include <unity.h>
#include <stdbool.h>
#include <stdint.h>

#include "../../src/board/rs485_fsm.c"

static bool last_pin_state;
static uint32_t pin_transitions;
static uint32_t pin_high_count;

static void mock_pin_set(bool high, void *user)
{
	(void)user;
	if (high != last_pin_state) {
		pin_transitions++;
	}
	last_pin_state = high;
	if (high) {
		pin_high_count++;
	}
}

void setUp(void)
{
	last_pin_state = false;
	pin_transitions = 0;
	pin_high_count = 0;
}

void tearDown(void) {}

/* After init, DE must be LOW. */
void test_init_forces_de_low(void)
{
	rs485_fsm_t fsm;
	last_pin_state = true;	/* poison the mock */

	rs485_fsm_init(&fsm, mock_pin_set, NULL);

	TEST_ASSERT_FALSE(last_pin_state);
	TEST_ASSERT_EQUAL(RS485_DIR_RX, rs485_fsm_dir(&fsm));
}

/* tx_begin() raises DE; tx_done() lowers it. */
void test_tx_cycle_toggles_de(void)
{
	rs485_fsm_t fsm;
	rs485_fsm_init(&fsm, mock_pin_set, NULL);

	int ret = rs485_fsm_tx_begin(&fsm);
	TEST_ASSERT_EQUAL_INT(0, ret);
	TEST_ASSERT_TRUE(last_pin_state);
	TEST_ASSERT_EQUAL(RS485_DIR_TX, rs485_fsm_dir(&fsm));

	rs485_fsm_tx_done(&fsm);
	TEST_ASSERT_FALSE(last_pin_state);
	TEST_ASSERT_EQUAL(RS485_DIR_RX, rs485_fsm_dir(&fsm));
	TEST_ASSERT_EQUAL_UINT32(1, fsm.tx_done_count);
}

/* Multiple back-to-back tx_begin() calls don't bounce the pin. This matters
 * because some attack modes may chain frames; bouncing DE between them would
 * be a bug we never want to ship. */
void test_chained_tx_keeps_de_high(void)
{
	rs485_fsm_t fsm;
	rs485_fsm_init(&fsm, mock_pin_set, NULL);

	(void)rs485_fsm_tx_begin(&fsm);
	int ret = rs485_fsm_tx_begin(&fsm);
	TEST_ASSERT_EQUAL_INT(1, ret);	/* "already TX" */
	TEST_ASSERT_TRUE(last_pin_state);
	TEST_ASSERT_EQUAL_UINT32(1, pin_high_count);
}

/* tx_aborted is treated as if TX completed: DE drops. */
void test_tx_aborted_drops_de(void)
{
	rs485_fsm_t fsm;
	rs485_fsm_init(&fsm, mock_pin_set, NULL);

	(void)rs485_fsm_tx_begin(&fsm);
	rs485_fsm_tx_aborted(&fsm);

	TEST_ASSERT_FALSE(last_pin_state);
	TEST_ASSERT_EQUAL(RS485_DIR_RX, rs485_fsm_dir(&fsm));
	TEST_ASSERT_EQUAL_UINT32(1, fsm.tx_aborted_count);
}

/*
 * A stray tx_done() without a tx_begin() must be defensively handled — it
 * should leave DE LOW and bump a counter so we can detect this in soak
 * tests. Skipping this would let a UART glitch leave DE stuck HIGH and
 * pin the bus.
 */
void test_late_tx_done_is_defensive(void)
{
	rs485_fsm_t fsm;
	rs485_fsm_init(&fsm, mock_pin_set, NULL);

	rs485_fsm_tx_done(&fsm);

	TEST_ASSERT_FALSE(last_pin_state);
	TEST_ASSERT_EQUAL_UINT32(1, fsm.late_tx_done_count);
}

/*
 * Idle (RX) direction must never see DE go high spontaneously. Strict
 * invariant: pin_high_count after init+RX-only operations is 0.
 */
void test_rx_only_never_raises_de(void)
{
	rs485_fsm_t fsm;
	rs485_fsm_init(&fsm, mock_pin_set, NULL);

	/* Simulate a long stretch of received bytes — none of these should
	 * touch the FSM. The driver's UART_RX_RDY callback path doesn't
	 * call into the FSM at all. */
	for (int i = 0; i < 1000; i++) {
		/* nothing */
	}

	TEST_ASSERT_EQUAL_UINT32(0, pin_high_count);
	TEST_ASSERT_FALSE(last_pin_state);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_init_forces_de_low);
	RUN_TEST(test_tx_cycle_toggles_de);
	RUN_TEST(test_chained_tx_keeps_de_high);
	RUN_TEST(test_tx_aborted_drops_de);
	RUN_TEST(test_late_tx_done_is_defensive);
	RUN_TEST(test_rx_only_never_raises_de);
	return UNITY_END();
}
