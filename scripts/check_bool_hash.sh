#!/usr/bin/env bash
#
# Copyright 2026 loco-pico contributors
# SPDX-License-Identifier: Apache-2.0
#
# Verify that the std_msgs/msg/Bool RIHS type hash compiled into the firmware
# matches the hash used by the ROS 2 installation on this host.
#
# rmw_zenoh builds topic key expressions as
#   0/<topic>/std_msgs::msg::dds_::Bool_/RIHS01_<hash>
# so a mismatch makes the firmware publisher invisible to ROS 2 tools.
#
# Usage:
#   scripts/check_bool_hash.sh [ROS_PREFIX]
#
# ROS_PREFIX defaults to /opt/ros/$ROS_DISTRO (or /opt/ros/lyrical).

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ $# -ge 1 ]]; then
	ROS_PREFIX="$1"
elif [[ -n "${ROS_PREFIX:-}" ]]; then
	ROS_PREFIX="$ROS_PREFIX"
else
	ROS_PREFIX="/opt/ros/${ROS_DISTRO:-lyrical}"
fi

HOST_JSON="$ROS_PREFIX/share/std_msgs/msg/Bool.json"
USER_TYPES="$HERE/types/user_types.h"

if [[ ! -f "$HOST_JSON" ]]; then
	echo "error: $HOST_JSON not found (is ROS 2 ${ROS_DISTRO:-lyrical} installed?)" >&2
	exit 2
fi

if [[ ! -f "$USER_TYPES" ]]; then
	echo "error: $USER_TYPES not found" >&2
	exit 2
fi

host_hash="$(grep -o '"hash_string"[[:space:]]*:[[:space:]]*"[^"]*"' "$HOST_JSON" \
	| head -n1 | sed -E 's/.*RIHS01_([0-9a-f]+)".*/\1/')"

fw_hash="$(awk '/std_msgs::msg::dds_::Bool/{getline; if (match($0, /"[0-9a-f]+"/)) print substr($0, RSTART+1, RLENGTH-2)}' \
	"$USER_TYPES" | head -n1)"

if [[ -z "$host_hash" || -z "$fw_hash" ]]; then
	echo "error: could not extract a type hash (host='$host_hash' firmware='$fw_hash')" >&2
	exit 2
fi

echo "host     ($HOST_JSON): RIHS01_$host_hash"
echo "firmware ($USER_TYPES): $fw_hash"

if [[ "$host_hash" == "$fw_hash" ]]; then
	echo "OK: std_msgs/msg/Bool type hashes match"
	exit 0
fi

echo "MISMATCH: std_msgs/msg/Bool type hashes differ" >&2
exit 1