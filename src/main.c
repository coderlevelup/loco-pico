// Copyright 2026 loco-pico contributors
// SPDX-License-Identifier: Apache-2.0
//
// Stage 2: bring up the USB CDC-ECM virtual Ethernet link, then blink the
// onboard LED. The ROS 2 publisher is added in a later stage.

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "usbd_net.h"

LOG_MODULE_REGISTER(loco_pico, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void)
{
	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED device %s is not ready", led.port->name);
		return -ENODEV;
	}

	int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED pin: %d", ret);
		return ret;
	}

	LOG_INF("loco-pico bring-up on %s", CONFIG_BOARD_TARGET);

	if (IS_ENABLED(CONFIG_LOCO_PICO_CDC_ECM)) {
		ret = loco_pico_usbd_net_init();
		if (ret < 0) {
			LOG_ERR("USB CDC-ECM bring-up failed: %d", ret);
		} else {
			char locator[32];

			if (loco_pico_usbd_net_peer_locator(locator, sizeof(locator)) == 0) {
				LOG_INF("USB CDC-ECM up; host Zenoh peer at %s", locator);
			} else {
				LOG_INF("USB CDC-ECM up");
			}
		}
	}

	bool state = false;

	while (true) {
		state = !state;
		gpio_pin_set_dt(&led, state ? 1 : 0);
		LOG_INF("LED %s", state ? "on" : "off");
		k_sleep(K_MSEC(1000));
	}

	return 0;
}
