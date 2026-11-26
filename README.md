# loco-pico

A minimal Zephyr firmware that blinks an onboard LED and reports the LED state
to ROS 2 as a `std_msgs/msg/Bool` topic, using
[Pico-ROS](https://github.com/Pico-ROS/Pico-ROS-software) over Zenoh
(the `rmw_zenoh` wire protocol). No micro-ROS agent and no DDS bridge are
required: the firmware speaks the `rmw_zenoh` format directly and connects to a
Zenoh router on the host.

The device is reachable as a Zenoh client over a USB CDC-ECM virtual Ethernet
link, so a single USB cable carries both power and the ROS 2 transport. The same
application builds for the RP2040 and the ESP32-S3.

## Targets

| Board | `scripts/build.sh` | Build dir | Onboard LED |
| --- | --- | --- | --- |
| Raspberry Pi Pico (RP2040) | `pico` | `build-pico` | GPIO25 (`led0`) |
| ESP32-S3-DevKitC-1 | `esp32s3` | `build-esp32s3` | WS2812 RGB over SPI3 (GPIO48) |

## Behaviour

- Blinks the onboard LED at ~1 Hz.
- Publishes `std_msgs/msg/Bool` on topic `blink` (`data` = LED state).
- Appears as the ROS 2 node `loco_pico`.
- Connects to the host Zenoh router at `tcp/192.0.2.1:7447` over USB CDC-ECM,
  reconnecting automatically if the session drops.

## Repository layout

```
CMakeLists.txt                     app + Pico-ROS/Micro-CDR build integration
Kconfig                            application Kconfig root (sources Zephyr)
prj.conf                           base configuration
types/user_types.h                 picoserdes type table (std_msgs/Bool + RIHS)
src/main.c                         blink + Zenoh publish loop
src/led_status.[ch]                board-agnostic status LED (GPIO or LED strip)
usb/usbd_net.[ch]                  USB CDC-ECM bring-up (vendored, attributed)
snippets/cdc-ecm/                  USB CDC-ECM snippet (conf + DT overlay)
boards/esp32s3_devkitc_*           ESP32-S3 overlay (WS2812 on SPI3) + conf
thirdparty/Pico-ROS/               pinned submodule
thirdparty/Micro-CDR/              pinned submodule
thirdparty/compat/                 zenoh-pico 1.6.2 compatibility shims
scripts/build.sh                   build for a target inside the workspace venv
scripts/check_bool_hash.sh         validate the RIHS type hash on the host
```

## Prerequisites

- A Zephyr workspace with Zephyr 4.x and the `zenoh-pico` Zephyr module.
  This project is developed against `~/zephyr_ws` (Zephyr v4.4.2,
  zenoh-pico 1.6.2) and its `venv` (which provides `esptool` for the ESP32
  targets).
- The Zephyr SDK (`ZEPHYR_SDK_INSTALL_DIR` set).
- Host: ROS 2 with `rmw_zenoh_cpp` and a Zenoh router (`rmw_zenohd` or
  `zenohd`).

Fetch the submodules after cloning:

```sh
git submodule update --init thirdparty/Pico-ROS thirdparty/Micro-CDR
```

Pico-ROS's own nested submodules are intentionally not fetched: the build uses
the workspace `zenoh-pico` module and the separately pinned Micro-CDR.

## Build

`scripts/build.sh` enters the workspace `venv` first, which puts `esptool` on
`PATH` (the ESP32 SoC CMake requires it) and uses the workspace's Python:

```sh
scripts/build.sh pico       # Raspberry Pi Pico
scripts/build.sh esp32s3     # ESP32-S3-DevKitC-1
```

Extra arguments are forwarded to `west build`, for example
`scripts/build.sh esp32s3 -- -DEXTRA_CONF_FILE=extra.conf`. Set `ZEPHYR_WS` if
the workspace is not at `~/zephyr_ws`.

Equivalent manual invocation for the Pico:

```sh
cd ~/zephyr_ws
west build -b rpi_pico -S cdc-ecm -d build-pico ~/Desktop/loco-pico
```

## Flash

- Pico: hold BOOTSEL, plug in, then copy `build-pico/zephyr/zephyr.uf2` to the
  `RPI-RP2` mass-storage device.
- ESP32-S3: `west flash` (or `west flash -d build-esp32s3`), or flash
  `build-esp32s3/zephyr/zephyr.bin`.

## Validate the message type hash

`rmw_zenoh` keys topics by type hash, so the firmware and host must agree on the
`std_msgs/msg/Bool` RIHS hash:

```sh
scripts/check_bool_hash.sh
```

It compares `types/user_types.h` against
`/opt/ros/<distro>/share/std_msgs/msg/Bool.json`.

## Host side

Start a Zenoh router that listens for the device, pointing ROS 2 at
`rmw_zenoh_cpp`:

```sh
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
export ZENOH_CONFIG_OVERRIDE='mode="peer";listen/endpoints=["tcp/0.0.0.0:7447"]'
ros2 run rmw_zenoh_cpp rmw_zenohd   # or: zenohd --listen tcp/0.0.0.0:7447
```

Plug in the board. It addresses the host at `192.0.2.1` and takes `192.0.2.2`
itself, and runs a DHCPv4 server so a DHCP client on the host interface is
configured automatically. If your host does not run one, configure the
interface manually (it appears as a new USB Ethernet device, host MAC
`00:00:5e:00:53:01`):

```sh
sudo ip addr add 192.0.2.1/24 dev <iface>
sudo ip link set <iface> up
```

Then, in a shell that has `RMW_IMPLEMENTATION=rmw_zenoh_cpp` exported:

```sh
ros2 node list      # shows /loco_pico
ros2 topic echo /blink
```

## zenoh-pico 1.6.2 compatibility

The pinned Pico-ROS targets a newer zenoh-pico. Two small shims under
`thirdparty/compat/` bridge the gap without patching either upstream:

- `picoros_compat.[ch]` reimplements `zp_read_task_is_running()` and
  `zp_lease_task_is_running()` (used only by `picoros_interface_is_up()`).
- `zephyr_atomics.c` supplies the `__atomic_fetch_add_4` and friends that
  zenoh-pico's C11 refcounting needs on Cortex-M0+, where the Zephyr SDK has no
  libatomic. It compiles to nothing on targets with native atomics.

## Status

The firmware builds for both targets. End-to-end runtime verification
(`ros2 node list`, `ros2 topic echo /blink`) requires the board to be attached
to a host with the router running.
