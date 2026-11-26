# Copyright 2026 loco-pico contributors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Make the in-tree snippets (e.g. cdc-ecm) discoverable via -S <name>.
list(APPEND SNIPPET_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(loco_pico)

# ---------------------------------------------------------------------------
# Vendored third-party sources: Pico-ROS, Micro-CDR and the zenoh-pico 1.6.2
# compatibility shim.
# ---------------------------------------------------------------------------
set(PICOROS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Pico-ROS)
set(MICROCDR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Micro-CDR)
set(COMPAT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/compat)
set(TYPES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/types)

target_sources(app PRIVATE
  src/main.c
  usb/usbd_net.c

  ${PICOROS_DIR}/src/picoros.c
  ${PICOROS_DIR}/src/picoserdes.c

  ${MICROCDR_DIR}/src/c/common.c
  ${MICROCDR_DIR}/src/c/types/array.c
  ${MICROCDR_DIR}/src/c/types/basic.c
  ${MICROCDR_DIR}/src/c/types/sequence.c
  ${MICROCDR_DIR}/src/c/types/string.c

  ${COMPAT_DIR}/picoros_compat.c
)

target_include_directories(app PRIVATE
  usb
  ${TYPES_DIR}
  ${COMPAT_DIR}
  ${PICOROS_DIR}/src
  ${MICROCDR_DIR}/include
  ${PICOROS_DIR}/thirdparty/config
)

# The picoserdes type table to compile in.
target_compile_definitions(app PRIVATE
  USER_TYPE_FILE="user_types.h"
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
