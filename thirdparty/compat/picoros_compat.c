/*
 * Copyright 2026 loco-pico contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Compatibility shims for building Pico-ROS against zenoh-pico 1.6.2.
 *
 * The newer zenoh-pico helpers zp_read_task_is_running() and
 * zp_lease_task_is_running() do not exist in 1.6.2. They are reimplemented
 * here by reading the read/lease task flags of the session's active transport,
 * which is what the newer helpers do internally.
 */

#include "picoros_compat.h"

#include "zenoh-pico/transport/transport.h"

static _z_transport_common_t *session_transport(const z_loaned_session_t *zs)
{
	_z_session_t *zn;

	if (zs == NULL) {
		return NULL;
	}

	zn = _Z_RC_IN_VAL((z_loaned_session_t *)zs);
	if (zn == NULL) {
		return NULL;
	}

	return _z_transport_get_common(&zn->_tp);
}

bool zp_read_task_is_running(const z_loaned_session_t *zs)
{
	_z_transport_common_t *ztc = session_transport(zs);

	return ztc != NULL && ztc->_read_task_running;
}

bool zp_lease_task_is_running(const z_loaned_session_t *zs)
{
	_z_transport_common_t *ztc = session_transport(zs);

	return ztc != NULL && ztc->_lease_task_running;
}
