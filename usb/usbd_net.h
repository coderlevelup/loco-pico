// Copyright 2026 loco-pico contributors
// SPDX-License-Identifier: Apache-2.0
//
// USB CDC-ECM network bring-up helpers.
//
// Adapted from the Zenbedded project's zenbedded_common USB support
// (Copyright 2026 Open Source Robotics Foundation, Inc., Apache-2.0) and from
// the Zephyr "sample_usbd" helper.

#ifndef LOCO_PICO_USBD_NET_H_
#define LOCO_PICO_USBD_NET_H_

#include <stdbool.h>
#include <stddef.h>

#include <zephyr/usb/usbd.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the USB device context with all enabled classes registered.
 *
 * @return Pointer to the device context, or NULL on failure.
 */
struct usbd_context *loco_pico_usbd_init(void);

/**
 * @brief Bring up the USB CDC-ECM interface.
 *
 * Initialises and enables the USB device, configures the deterministic static
 * IPv4 address on the CDC-ECM interface, optionally starts a DHCPv4 server for
 * the host and waits until the interface has a preferred IPv4 address.
 *
 * @return 0 on success, negative errno on failure.
 */
int loco_pico_usbd_net_init(void);

/**
 * @brief Whether the USB CDC-ECM interface is up with a preferred IPv4 address.
 */
bool loco_pico_usbd_net_ready(void);

/**
 * @brief Build the Zenoh peer locator that points at the host over USB CDC-ECM.
 *
 * For example "192.0.2.1:7447".
 *
 * @param out    Destination buffer.
 * @param out_len Capacity of @p out.
 * @return 0 on success, -EINVAL / -ENOSPC on error.
 */
int loco_pico_usbd_net_peer_locator(char *out, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif /* LOCO_PICO_USBD_NET_H_ */
