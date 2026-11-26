// Copyright 2026 loco-pico contributors
// SPDX-License-Identifier: Apache-2.0
//
// Board-agnostic onboard status LED.
//
// Two backends are available, selected through Kconfig:
//   LOCO_PICO_LED_GPIO  - a plain GPIO wired to the LED
//                         (Raspberry Pi Pico, devicetree alias led0)
//   LOCO_PICO_LED_STRIP - an addressable WS2812 over SPI
//                         (ESP32-S3-DevKitC, devicetree alias led-strip)

#include "led_status.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(loco_pico, LOG_LEVEL_INF);

#if defined(CONFIG_LOCO_PICO_LED_GPIO)

#include <zephyr/drivers/gpio.h>

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int led_status_init(void)
{
	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED device %s is not ready", led.port->name);
		return -ENODEV;
	}

	return gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
}

void led_status_set(bool on)
{
	(void)gpio_pin_set_dt(&led, on ? 1 : 0);
}

#elif defined(CONFIG_LOCO_PICO_LED_STRIP)

#include <zephyr/drivers/led_strip.h>

/* Non-zero green is clearly visible on the DevKitC's WS2812. */
#define LED_STRIP_ON_LEVEL 32

static const struct device *const strip = DEVICE_DT_GET(DT_ALIAS(led_strip));

int led_status_init(void)
{
	if (!device_is_ready(strip)) {
		LOG_ERR("LED strip device %s is not ready", strip->name);
		return -ENODEV;
	}

	led_status_set(false);

	return 0;
}

void led_status_set(bool on)
{
	struct led_rgb pixel = {
		.r = 0,
		.g = on ? LED_STRIP_ON_LEVEL : 0,
		.b = 0,
	};
	int ret = led_strip_update_rgb(strip, &pixel, 1);

	if (ret < 0) {
		LOG_WRN("Failed to update the LED strip: %d", ret);
	}
}

#else
#error "Select an onboard LED backend (CONFIG_LOCO_PICO_LED_GPIO or _STRIP)"
#endif
