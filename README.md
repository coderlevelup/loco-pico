# loco-pico

A minimal Zephyr firmware that blinks an onboard LED and reports the LED state to
ROS 2 as a `std_msgs/msg/Bool` topic, using
[Pico-ROS](https://github.com/Pico-ROS/Pico-ROS-software) over Zenoh
(`rmw_zenoh` protocol). No micro-ROS agent and no DDS bridge are required: the
firmware speaks the `rmw_zenoh` wire format directly and connects to a Zenoh
router on the host.

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

## Build (host side)

See `docs/` and `scripts/` for the host setup, or the summary in this README as
the project fills in.
