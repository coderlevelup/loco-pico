#!/usr/bin/env bash
#
# Copyright 2026 loco-pico contributors
# SPDX-License-Identifier: Apache-2.0
#
# Build loco-pico for a supported board, always with the "cdc-ecm" snippet.
#
# The build runs inside the Zephyr workspace's Python virtual environment,
# which is where esptool (required by the ESP32 targets) lives.
#
# Usage:
#   scripts/build.sh [pico|esp32s3|BOARD] [extra west build args...]
#
# Examples:
#   scripts/build.sh                 # Raspberry Pi Pico (RP2040)
#   scripts/build.sh esp32s3         # ESP32-S3-DevKitC-1
#   scripts/build.sh esp32s3 -- -DEXTRA_CONF_FILE=extra.conf
#
# Environment:
#   ZEPHYR_WS   Workspace root (default: parent of ZEPHYR_BASE, else ~/zephyr_ws)

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

WS="${ZEPHYR_WS:-}"
if [[ -z "$WS" && -n "${ZEPHYR_BASE:-}" ]]; then
	WS="$(cd "$ZEPHYR_BASE/.." && pwd)"
fi
WS="${WS:-$HOME/zephyr_ws}"

if [[ ! -d "$WS/zephyr" ]]; then
	echo "error: Zephyr workspace not found at '$WS' (set ZEPHYR_WS)" >&2
	exit 2
fi

# esptool and the matching Python interpreter live in the workspace venv.
if [[ -f "$WS/venv/bin/activate" ]]; then
	# shellcheck disable=SC1091
	source "$WS/venv/bin/activate"
fi

target="${1:-pico}"
shift || true

case "$target" in
	pico)
		board="rpi_pico"
		out="$HERE/build-pico"
		;;
	esp32s3 | s3)
		board="esp32s3_devkitc/esp32s3/procpu"
		out="$HERE/build-esp32s3"
		;;
	*)
		board="$target"
		out="$HERE/build-$(printf '%s' "$target" | tr '/.' '__')"
		;;
esac

cd "$WS"
echo "Building loco-pico for '$board' -> $out"
west build -b "$board" -S cdc-ecm -d "$out" "$HERE" "$@"
