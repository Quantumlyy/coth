/*
 * RS-485 half-duplex driver — Zephyr binding.
 *
 * Pure FSM logic lives in rs485_fsm.c; this file plugs it into Zephyr's
 * UART async API and the DE GPIO.
 *
 * Critical timing rule (spec §7.1): DE is dropped *only* on UART_TX_DONE,
 * never on byte-written. Off-by-one truncates the last byte.
 */

#include "rs485.h"
#include "rs485_fsm.h"

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(rs485, CONFIG_LOG_DEFAULT_LEVEL);

#define RS485_NODE	DT_PATH(rs485)
#define UART_NODE	DT_ALIAS(mellon_uart)

#if !DT_NODE_HAS_STATUS(RS485_NODE, okay)
#error "rs485 devicetree node missing — check boards/mellon.overlay"
#endif

static const struct device *const uart_dev = DEVICE_DT_GET(UART_NODE);
static const struct gpio_dt_spec de_pin = GPIO_DT_SPEC_GET(RS485_NODE, de_gpios);

#define RX_BUF_SIZE	256
#define RX_TIMEOUT_US	1000	/* idle timeout that flushes a partial RX */

static uint8_t rx_buf[2][RX_BUF_SIZE];
static size_t rx_buf_idx;

static struct k_sem tx_done_sem;
static rs485_fsm_t fsm;
static mellon_rs485_rx_fn g_rx_fn;
static void *g_rx_user;

/* GPIO trampoline used by the FSM. */
static void de_pin_set(bool high, void *user)
{
	ARG_UNUSED(user);
	gpio_pin_set_dt(&de_pin, high ? 1 : 0);
}

static void uart_callback(const struct device *dev, struct uart_event *evt, void *user)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user);

	switch (evt->type) {
	case UART_TX_DONE:
		rs485_fsm_tx_done(&fsm);
		k_sem_give(&tx_done_sem);
		break;

	case UART_TX_ABORTED:
		rs485_fsm_tx_aborted(&fsm);
		k_sem_give(&tx_done_sem);
		LOG_WRN("uart_tx aborted");
		break;

	case UART_RX_RDY: {
		const uint8_t *p = evt->data.rx.buf + evt->data.rx.offset;
		size_t n = evt->data.rx.len;
		if (g_rx_fn && n > 0) {
			g_rx_fn(p, n, g_rx_user);
		}
		break;
	}

	case UART_RX_BUF_REQUEST: {
		/* Hand back the alternate buffer for double-buffered RX. */
		uint8_t *next = rx_buf[(rx_buf_idx + 1) & 1];
		rx_buf_idx = (rx_buf_idx + 1) & 1;
		uart_rx_buf_rsp(uart_dev, next, RX_BUF_SIZE);
		break;
	}

	case UART_RX_DISABLED:
		/* RX was disabled (after STOPPED). Re-enable. */
		(void)uart_rx_enable(uart_dev, rx_buf[0], RX_BUF_SIZE, RX_TIMEOUT_US);
		rx_buf_idx = 0;
		break;

	case UART_RX_BUF_RELEASED:
	case UART_RX_STOPPED:
	default:
		break;
	}
}

static int rs485_apply_baud(uint32_t baud)
{
	struct uart_config cfg = {
		.baudrate = baud,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	};
	return uart_configure(uart_dev, &cfg);
}

int mellon_rs485_init(uint32_t baud)
{
	int ret;

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("uart not ready");
		return -ENODEV;
	}
	if (!gpio_is_ready_dt(&de_pin)) {
		LOG_ERR("DE pin not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&de_pin, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("DE pin configure: %d", ret);
		return ret;
	}

	rs485_fsm_init(&fsm, de_pin_set, NULL);
	k_sem_init(&tx_done_sem, 0, 1);

	ret = uart_callback_set(uart_dev, uart_callback, NULL);
	if (ret < 0) {
		LOG_ERR("uart_callback_set: %d", ret);
		return ret;
	}

	ret = rs485_apply_baud(baud);
	if (ret < 0) {
		LOG_ERR("uart_configure(%u): %d", baud, ret);
		return ret;
	}

	rx_buf_idx = 0;
	ret = uart_rx_enable(uart_dev, rx_buf[0], RX_BUF_SIZE, RX_TIMEOUT_US);
	if (ret < 0) {
		LOG_ERR("uart_rx_enable: %d", ret);
		return ret;
	}

	LOG_INF("rs485 ready @ %u baud (DE=P0.04)", baud);
	return 0;
}

int mellon_rs485_set_baud(uint32_t baud)
{
	int ret = uart_rx_disable(uart_dev);
	if (ret < 0 && ret != -EFAULT) {
		return ret;
	}
	ret = rs485_apply_baud(baud);
	if (ret < 0) {
		return ret;
	}
	rx_buf_idx = 0;
	return uart_rx_enable(uart_dev, rx_buf[0], RX_BUF_SIZE, RX_TIMEOUT_US);
}

int mellon_rs485_send(const uint8_t *data, size_t len)
{
	if (data == NULL || len == 0) {
		return -EINVAL;
	}

	/*
	 * The FSM raises DE *before* the first byte. The Zephyr async UART API
	 * starts shifting bytes only on uart_tx() return, so the order is:
	 *   1. tx_begin() -> DE HIGH
	 *   2. uart_tx() -> bytes start clocking
	 *   3. UART_TX_DONE -> tx_done() -> DE LOW
	 */
	(void)k_sem_take(&tx_done_sem, K_NO_WAIT);	/* drain any stale signal */
	(void)rs485_fsm_tx_begin(&fsm);

	int ret = uart_tx(uart_dev, data, len, SYS_FOREVER_US);
	if (ret < 0) {
		rs485_fsm_tx_aborted(&fsm);
		LOG_ERR("uart_tx: %d", ret);
		return ret;
	}

	if (k_sem_take(&tx_done_sem, K_MSEC(1000)) < 0) {
		LOG_ERR("TX_DONE timeout (%zu bytes)", len);
		(void)uart_tx_abort(uart_dev);
		rs485_fsm_tx_aborted(&fsm);
		return -ETIMEDOUT;
	}
	return 0;
}

void mellon_rs485_set_rx_handler(mellon_rs485_rx_fn fn, void *user)
{
	/* No barrier needed on Cortex-M: aligned pointer/word stores are atomic
	 * and the architecture is sequentially consistent for ordinary memory.
	 * See header for the single-writer expectation. */
	g_rx_user = user;
	g_rx_fn = fn;
}
