#include "board.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(board, CONFIG_LOG_DEFAULT_LEVEL);

#define LED_ACT_NODE	DT_ALIAS(mellon_led_act)
#define LED_STATUS_NODE	DT_ALIAS(mellon_led_status)

#if !DT_NODE_HAS_STATUS(LED_ACT_NODE, okay) || !DT_NODE_HAS_STATUS(LED_STATUS_NODE, okay)
#error "mellon-led-act and mellon-led-status aliases must be defined in the overlay"
#endif

static const struct gpio_dt_spec led_act = GPIO_DT_SPEC_GET(LED_ACT_NODE, gpios);
static const struct gpio_dt_spec led_status = GPIO_DT_SPEC_GET(LED_STATUS_NODE, gpios);

static void led_pulse_off(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(led_act_off_work, led_pulse_off);

static void led_pulse_off(struct k_work *work)
{
	ARG_UNUSED(work);
	gpio_pin_set_dt(&led_act, 0);
}

int mellon_board_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led_act) || !gpio_is_ready_dt(&led_status)) {
		LOG_ERR("LED GPIOs not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&led_act, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("configure D3 (act) failed: %d", ret);
		return ret;
	}
	ret = gpio_pin_configure_dt(&led_status, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("configure D4 (status) failed: %d", ret);
		return ret;
	}

	LOG_INF("board init ok (D3=act, D4=status)");
	return 0;
}

void mellon_led_status(bool on)
{
	gpio_pin_set_dt(&led_status, on ? 1 : 0);
}

void mellon_led_act(bool on)
{
	gpio_pin_set_dt(&led_act, on ? 1 : 0);
}

void mellon_led_act_pulse(uint32_t ms)
{
	gpio_pin_set_dt(&led_act, 1);
	k_work_reschedule(&led_act_off_work, K_MSEC(ms));
}
