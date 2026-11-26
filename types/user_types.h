/*
 * Copyright 2026 loco-pico contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * ROS 2 message type table for picoserdes.
 *
 * Only the types this firmware actually uses are listed, to keep the code
 * footprint small. The RIHS type hash below must match the hash used by
 * rmw_zenoh_cpp on the host side for std_msgs/msg/Bool, otherwise the topic
 * key expression will not match and the publisher will be invisible to ROS 2.
 *
 * Regenerate against a specific ROS 2 distribution with Pico-ROS's
 * tools/type-gen (rosidl based) if the hash ever changes.
 */

#ifndef LOCO_PICO_USER_TYPES_H
#define LOCO_PICO_USER_TYPES_H

#define MSG_LIST(BTYPE, CTYPE, TTYPE, FIELD, ARRAY, SEQUENCE) \
    BTYPE(ros_Bool, \
        "std_msgs::msg::dds_::Bool", \
        "feb91e995ff9ebd09c0cb3d2aed18b11077585839fb5db80193b62d74528f6c9", \
        bool \
    )

#define SRV_LIST(SRV, REQUEST, REPLY, FIELD, ARRAY, SEQUENCE)

#endif /* LOCO_PICO_USER_TYPES_H */
