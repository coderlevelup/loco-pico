# Copyright 2026 loco-pico contributors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(loco_pico)

# ---------------------------------------------------------------------------
# Vendored third-party sources: Pico-ROS, Micro-CDR and the zenoh-pico 1.6.2
# compatibility shim.
# ---------------------------------------------------------------------------
set(PICOROS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Pico-ROS)
set(MICROCDR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Micro-CDR)
set(COMPAT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/compat)

target_sources(app PRIVATE
  src/main.c
  src/led_status.c
  usb/usbd_net.c

  ${PICOROS_DIR}/src/picoros.c
  ${PICOROS_DIR}/src/picoserdes.c

  ${MICROCDR_DIR}/src/c/common.c
  ${MICROCDR_DIR}/src/c/types/array.c
  ${MICROCDR_DIR}/src/c/types/basic.c
  ${MICROCDR_DIR}/src/c/types/sequence.c
  ${MICROCDR_DIR}/src/c/types/string.c

  ${COMPAT_DIR}/picoros_compat.c
  ${COMPAT_DIR}/zephyr_atomics.c
)

target_include_directories(app PRIVATE
  usb
  ${COMPAT_DIR}
  ${PICOROS_DIR}/src
  ${PICOROS_DIR}/examples
  ${MICROCDR_DIR}/include
  ${PICOROS_DIR}/thirdparty/config
)

# Pico-ROS ships the common ROS 2 message and service types generated from the
# ROS IDL definitions (rcl_interfaces, common_interfaces, example_interfaces and
# unique_identifier_msgs) as examples/example_types.h. Compile that table in so
# the app can publish or subscribe any of those types.
target_compile_definitions(app PRIVATE
  USER_TYPE_FILE="example_types.h"
)

# Force the compatibility declarations into Pico-ROS sources so its calls to
# zp_read_task_is_running()/zp_lease_task_is_running() resolve against the
# shim without patching the submodule.
set_source_files_properties(
  ${PICOROS_DIR}/src/picoros.c
  PROPERTIES COMPILE_OPTIONS "-include;${COMPAT_DIR}/picoros_compat.h"
)

# ---------------------------------------------------------------------------
# Pico-ROS is built against a newer zenoh-pico, but the workspace ships 1.6.2.
# Enable the unstable liveliness API it needs and match the rmw_zenohd lease
# timing so the router keeps the session alive.
# ---------------------------------------------------------------------------
zephyr_compile_definitions(
  Z_FEATURE_UNSTABLE_API=1
  Z_TRANSPORT_LEASE=60000
  Z_TRANSPORT_LEASE_EXPIRE_FACTOR=2
)
