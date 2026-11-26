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

This repository is a self-contained **west "forest" application**: it is its own
manifest repository (`west.yml`), so it can be cloned into a workspace and
built without a separate application project.

## Targets

| Board | `west build -b` | Suggested build dir | Onboard LED |
| --- | --- | --- | --- |
| Raspberry Pi Pico (RP2040) | `rpi_pico` | `build-pico` | GPIO25 (`led0`) |
| ESP32-S3-DevKitC-1 | `esp32s3_devkitc/esp32s3/procpu` | `build-esp32s3` | WS2812 RGB over SPI3 (GPIO48) |

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
prj.conf                           base configuration (USB CDC-ECM is always on)
west.yml                           west manifest (makes this a forest app)
src/main.c                         blink + Zenoh publish loop
src/led_status.[ch]                board-agnostic status LED (GPIO or LED strip)
usb/usbd_net.[ch]                  USB CDC-ECM bring-up (vendored, attributed)
boards/rpi_pico.overlay            RP2040 CDC-ECM devicetree node
boards/esp32s3_devkitc_*           ESP32-S3 WS2812 overlay + conf
thirdparty/Pico-ROS/               pinned submodule (sources + common types)
thirdparty/Micro-CDR/              pinned submodule
thirdparty/compat/                 zenoh-pico 1.6.2 compatibility shims
```

The message type table is Pico-ROS's generated common type set
(`thirdparty/Pico-ROS/examples/example_types.h`). It is produced from the ROS
IDL definitions of `rcl_interfaces`, `common_interfaces`,
`example_interfaces` and `unique_identifier_msgs` by Pico-ROS's
`tools/type-gen`, so the app can publish or subscribe any of those common
types out of the box.

## Prerequisites

- `west` and a Zephyr 4.x toolchain (this app is developed against Zephyr
  v4.4.2).
- The Zephyr SDK (`ZEPHYR_SDK_INSTALL_DIR` set).
- `esptool >= 5.0.2` on `PATH` for the ESP32-S3 build. This is easiest from the
  Zephyr workspace virtualenv (`source <workspace>/venv/bin/activate`).
- Host: ROS 2 with `rmw_zenoh_cpp` and a Zenoh router (`rmw_zenohd` or
  `zenohd`).

Pico-ROS's own nested submodules are intentionally not fetched: the build uses
the workspace `zenoh-pico` project from `west.yml` and the separately pinned
Micro-CDR submodule.

## Get the workspace

Because the app is its own manifest repository, create a workspace and clone
the app into it. The manifest declares `self: path: app`, so the directory the
app is cloned into should be named `app` (or you must point west at it, see
below).

```sh
mkdir loco-ws && cd loco-ws
git clone --recurse-submodules <repository-url> app
west init -l app
west update
```

If you already have a west workspace and just cloned the app into it, set the
manifest path instead of running `west init`:

```sh
git clone --recurse-submodules <repository-url> app
west config manifest.path app
west update
```

`west update` fetches Zephyr v4.4.2, its imported modules, and zenoh-pico 1.6.2
into `modules/lib/zenoh-pico`. If you cloned without `--recurse-submodules`,
fetch the vendored sources before building:

```sh
git -C app submodule update --init thirdparty/Pico-ROS thirdparty/Micro-CDR
```

## Build

From the workspace root:

```sh
source venv/bin/activate          # puts esptool on PATH for the ESP32-S3

west build -b rpi_pico -d build-pico app
west build -b esp32s3_devkitc/esp32s3/procpu -d build-esp32s3 app
```

Pass `--pristine` to force a clean build, and set
`ZEPHYR_SDK_INSTALL_DIR` if the SDK is not found automatically.

## Flash

Raspberry Pi Pico:

1. Hold `BOOTSEL` and plug in the Pico. It appears as an `RPI-RP2`
   mass-storage device.
2. Copy the UF2 image to it:

   ```sh
   cp build-pico/zephyr/zephyr.uf2 /media/$USER/RPI-RP2/
   ```

ESP32-S3-DevKitC-1 (the board has two USB-C ports; use the **UART** port for
flashing and the console):

```sh
west flash -d build-esp32s3
```

If the port is not auto-detected, pass it explicitly, for example
`west flash -d build-esp32s3 --esp-device /dev/ttyACM0`.

## Host side

The ROS 2 transport runs over the board's **native USB** port, not the UART
console bridge. On the ESP32-S3-DevKitC-1 that is the port labelled **USB**
(next to **UART**); on the Pico it is the single USB port.

Start a Zenoh router and point ROS 2 at `rmw_zenoh_cpp`:

```sh
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
ros2 run rmw_zenoh_cpp rmw_zenohd   # or: zenohd --listen tcp/0.0.0.0:7447
```

Plug the board's native USB port into the host. A new USB Ethernet interface
appears, and the firmware runs a DHCPv4 server that leases `192.0.2.1` to the
host (the device itself is `192.0.2.2`). If your host does not run a DHCP
client on that interface, configure it manually (the host MAC is
`00:00:5e:00:53:01`):

```sh
sudo ip addr add 192.0.2.1/24 dev <iface>
sudo ip link set <iface> up
```

Then, in a shell that has `RMW_IMPLEMENTATION=rmw_zenoh_cpp` exported:

```sh
ros2 node list       # shows /loco_pico
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

The build also enables `Z_FEATURE_UNSTABLE_API` and sets the transport lease to
`60000` ms with an expire factor of `2`, matching the default `rmw_zenohd`
session so the router keeps the device alive.

## Status

The firmware builds for both targets. End-to-end runtime verification
(`ros2 node list`, `ros2 topic echo /blink`) requires the board to be attached
to a host with the router running.