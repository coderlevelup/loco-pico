/*
 * Copyright 2026 loco-pico contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Compatibility shims for building Pico-ROS against zenoh-pico 1.6.2.
 *
 * Pico-ROS targets a newer zenoh-pico (mid 2026) that exposes
 * zp_read_task_is_running() and zp_lease_task_is_running(). Those two helpers
 * only exist to implement picoros_interface_is_up(); this header declares them
 * so they can be force-included into Pico-ROS sources, and the matching .c
 * file implements them on top of zenoh-pico 1.6.2 internals.
 */

#ifndef LOCO_PICO_PICOROS_COMPAT_H_
#define LOCO_PICO_PICOROS_COMPAT_H_

#include <stdbool.h>

#include "zenoh-pico.h"

#ifdef __cplusplus
extern "C" {
#endif

bool zp_read_task_is_running(const z_loaned_session_t *zs);
bool zp_lease_task_is_running(const z_loaned_session_t *zs);

#ifdef __cplusplus
}
#endif

#endif /* LOCO_PICO_PICOROS_COMPAT_H_ */
