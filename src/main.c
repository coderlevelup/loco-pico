// Copyright 2026 loco-pico contributors
// SPDX-License-Identifier: Apache-2.0
//
// Blink the onboard LED and publish its state to ROS 2 as std_msgs/msg/Bool
// over a Pico-ROS (rmw_zenoh) session carried by USB CDC-ECM.

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "led_status.h"
#include "picoros.h"
#include "picoserdes.h"

#include "usbd_net.h"

LOG_MODULE_REGISTER(loco_pico, LOG_LEVEL_INF);

#define BLINK_PERIOD_MS 1000
#define RECONNECT_DELAY_MS 1000

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

static char zenoh_locator[40];
static bool ros_connected;

static int build_locator(void)
{
	char peer[32];
	int ret;

	ret = loco_pico_usbd_net_peer_locator(peer, sizeof(peer));
	if (ret < 0) {
		return ret;
	}

	ret = snprintf(zenoh_locator, sizeof(zenoh_locator), "tcp/%s", peer);
	if (ret < 0 || (size_t)ret >= sizeof(zenoh_locator)) {
		return -ENOSPC;
	}

	return 0;
}

static int ros_connect(void)
{
	picoros_interface_t ifx = {
		.mode = "client",
		.locator = zenoh_locator,
	};
	picoros_res_t res;

	res = picoros_interface_init(&ifx);
	if (res != PICOROS_OK) {
		LOG_WRN("Zenoh session not ready (%d), retrying", res);
		return -EAGAIN;
	}

	res = picoros_node_init(&ros_node);
	if (res != PICOROS_OK) {
		LOG_ERR("Failed to declare node '%s' (%d)", ros_node.name, res);
		picoros_interface_close();
		return -EIO;
	}

	res = picoros_publisher_declare(&ros_node, &blink_pub);
	if (res != PICOROS_OK) {
		LOG_ERR("Failed to declare publisher '%s' (%d)", blink_pub.topic.name, res);
		picoros_node_drop(&ros_node);
		picoros_interface_close();
		return -EIO;
	}

	ros_connected = true;
	LOG_INF("ROS 2 node '/%s' publishing '%s' (%s) to %s", ros_node.name,
		blink_pub.topic.name, blink_pub.topic.type, zenoh_locator);

	return 0;
}

static void ros_disconnect(void)
{
	picoros_publisher_drop(&blink_pub);
	picoros_node_drop(&ros_node);
	picoros_interface_close();
	ros_connected = false;
}

static void ros_publish(bool state)
{
	size_t len;

	led_state = state;
	len = ps_serialize(tx_buf, &led_state, sizeof(tx_buf));
	if (len == 0) {
		LOG_WRN("Failed to serialize std_msgs/Bool");
		return;
	}

	picoros_publish(&blink_pub, tx_buf, len);
}

int main(void)
{
	int ret = led_status_init();

	if (ret < 0) {
		LOG_ERR("Failed to initialise the status LED: %d", ret);
		return ret;
	}

	LOG_INF("loco-pico on %s", CONFIG_BOARD_TARGET);

	if (IS_ENABLED(CONFIG_LOCO_PICO_CDC_ECM)) {
		ret = loco_pico_usbd_net_init();
		if (ret < 0) {
			LOG_ERR("USB CDC-ECM bring-up failed: %d", ret);
		}
	}

	bool state = false;

	while (true) {
		state = !state;
		led_status_set(state);

		if (!ros_connected) {
			if (loco_pico_usbd_net_ready() && build_locator() == 0) {
				ros_connect();
			}
		} else if (!picoros_interface_is_up()) {
			LOG_WRN("ROS 2 session lost, reconnecting");
			ros_disconnect();
		}

		if (ros_connected) {
			ros_publish(state);
		}

		k_sleep(K_MSEC(ros_connected ? BLINK_PERIOD_MS : RECONNECT_DELAY_MS));
	}

	return 0;
}
