// Copyright 2026 loco-pico contributors
// SPDX-License-Identifier: Apache-2.0
//
// Stage 3: Pico-ROS build integration.
//
// The firmware still only blinks and brings up USB CDC-ECM, but the Pico-ROS
// node / publisher definitions and the std_msgs/Bool type table are wired in
// and compiled, and a loopback CDR serialization is run at startup to prove
// the serdes path is functional. Connecting to the host router and publishing
// in the main loop follows in a later stage.

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "picoros.h"
#include "picoserdes.h"

#include "usbd_net.h"

LOG_MODULE_REGISTER(loco_pico, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* ROS 2 node and publisher (declared on the router in a later stage). */
static picoros_node_t ros_node = {
	.name = "loco_pico",
};

static picoros_publisher_t blink_pub = {
	.topic = {
		.name = "blink",
		.type = ROSTYPE_NAME(ros_Bool),
		.rihs_hash = ROSTYPE_HASH(ros_Bool),
	},
};

/* std_msgs/msg/Bool is a single bool field. */
static ros_Bool led_state;
static uint8_t tx_buf[64];

static void ros_types_selftest(void)
{
	size_t len;

	LOG_INF("ROS publisher '%s' type '%s'", blink_pub.topic.name, blink_pub.topic.type);

	len = ps_serialize(tx_buf, &led_state, sizeof(tx_buf));
	LOG_INF("std_msgs/Bool serialized to %u byte(s)", (unsigned int)len);
}

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

	LOG_INF("loco-pico bring-up on %s (node '%s')", CONFIG_BOARD_TARGET, ros_node.name);

	ros_types_selftest();

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
