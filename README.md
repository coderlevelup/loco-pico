# loco-pico

A minimal Zephyr firmware that blinks an onboard LED and reports the LED state
to ROS 2 as a `std_msgs/msg/Bool` topic, using
[Pico-ROS](https://github.com/Pico-ROS/Pico-ROS-software) over Zenoh
(the `rmw_zenoh` wire protocol). No micro-ROS agent and no DDS bridge are
required: the firmware speaks the `rmw_zenoh` format directly and connects to a
Zenoh router on the host.

The device is reachable as a Zenoh client over a USB CDC-ECM virtual Ethernet
link, so a single USB cable carries both power and the ROS 2 transport.

## Targets

| Board | `west build -b` | Onboard LED |
| --- | --- | --- |
| Raspberry Pi Pico (RP2040) | `rpi_pico` | GPIO25 (`led0`) |
| ESP32-S3-DevKitC | `esp32s3_devkitc/esp32s3/procpu` | WS2812 RGB over SPI3 (GPIO48) |

## Behaviour

- Blinks the onboard LED at ~1 Hz.
- Publishes `std_msgs/msg/Bool` on topic `blink` (`data` = LED state).
- Appears as ROS 2 node `loco_pico`.
- Connects to the host Zenoh router at `tcp/192.0.2.1:7447` over USB CDC-ECM.

## Repository layout

```
CMakeLists.txt                     # app + Pico-ROS/Micro-CDR build integration
Kconfig                            # application Kconfig root (sources Zephyr)
prj.conf                           # base configuration
types/user_types.h                 # picoserdes type table (std_msgs/Bool + RIHS)
src/main.c                         # blink + publish loop
usb/usbd_net.[ch]                  # USB CDC-ECM bring-up (vendored, attributed)
snippets/cdc-ecm/                  # USB CDC-ECM snippet (conf + DT overlay)
thirdparty/Pico-ROS/               # pinned submodule
thirdparty/Micro-CDR/              # pinned submodule
thirdparty/compat/                 # zenoh-pico 1.6.2 compatibility shim
scripts/check_bool_hash.sh         # validate the RIHS type hash on the host
```

## Prerequisites

- A Zephyr workspace with Zephyr 4.x and the `zenoh-pico` Zephyr module.
  This project is developed against `~/zephyr_ws` (Zephyr v4.4.2,
  zenoh-pico 1.6.2).
- The Zephyr SDK (`ZEPHYR_SDK_INSTALL_DIR` set).
- Host: ROS 2 with `rmw_zenoh_cpp` and a Zenoh router (`zenohd` or
  `rmw_zenohd`).

Fetch the submodules after cloning:

```sh
git submodule update --init thirdparty/Pico-ROS thirdparty/Micro-CDR
```

Pico-ROS's own nested submodules are intentionally not fetched: the build uses
the workspace `zenoh-pico` module and the separately pinned Micro-CDR.

## Build

```sh
# Raspberry Pi Pico
west build -b rpi_pico -S cdc-ecm -d build-pico .

# ESP32-S3-DevKitC
west build -b esp32s3_devkitc/esp32s3/procpu -S cdc-ecm -d build-esp32s3 .
```

Flash the resulting `build-*/zephyr/zephyr.uf2` (Pico, BOOTSEL mass storage) or
`west flash` for the ESP32-S3.

## Validate the message type hash

`rmw_zenoh` keys topics by type hash, so the firmware and host must agree on the
`std_msgs/msg/Bool` RIHS hash:

```sh
scripts/check_bool_hash.sh
```

It compares `types/user_types.h` against
`/opt/ros/<distro>/share/std_msgs/msg/Bool.json`.

## Host side

Start a Zenoh router and point ROS 2 at `rmw_zenoh_cpp`, then plug in the board:

```sh
rmw_zenohd                      # or: zenohd --listen tcp/0.0.0.0:7447
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
ros2 node list                  # should show /loco_pico
ros2 topic echo /blink          # streams true/false at ~1 Hz
```

The device addresses the host at `192.0.2.1` and takes `192.0.2.2` itself; the
firmware runs a DHCPv4 server so the host interface is configured automatically.

See `docs/` and `scripts/` as the host bring-up tooling fills in.