// Copyright 2026 loco-pico contributors
// SPDX-License-Identifier: Apache-2.0
//
// Board-agnostic onboard status LED.

#ifndef LOCO_PICO_LED_STATUS_H_
#define LOCO_PICO_LED_STATUS_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the onboard status LED.
 *
 * @return 0 on success, negative errno on failure.
 */
int led_status_init(void);

/**
 * @brief Turn the onboard status LED on or off.
 */
void led_status_set(bool on);

#ifdef __cplusplus
}
#endif

#endif /* LOCO_PICO_LED_STATUS_H_ */
