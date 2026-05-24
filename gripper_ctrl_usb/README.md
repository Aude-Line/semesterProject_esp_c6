# gripper_ctrl_usb

ESP-IDF + micro-ROS gripper controller using custom UART transport (USB serial), not Wi-Fi.

## What changed vs gripper_ctrl

- micro-ROS transport switched from network (UDP/Wi-Fi) to custom UART transport
- UART transport callbacks added in `main/esp32_serial_transport.c`
- project name changed to `gripper_ctrl_usb`
- default config enables UART transport

The ROS topic interface is unchanged:

- Subscriber topic: `esp32_rx_int32`
- Payload: `std_msgs/msg/Int32`
- `0` turns gripper output off
- non-zero turns gripper output on

## Build and flash

From this folder:

```bash
idf.py set-target esp32s3
idf.py menuconfig
idf.py build flash monitor
```

In `menuconfig`, verify:

- Component config -> micro-ROS Settings -> micro-ROS middleware: enabled
- Component config -> micro-ROS Settings -> micro-ROS transport: UART
- Component config -> micro-ROS Settings -> UART transport settings:
  - TX pin: set for your board
  - RX pin: set for your board
  - baudrate: 115200 (default in transport source)

## Run micro-ROS agent on host

On your Linux PC:

```bash
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyUSB0 -b 115200
```

Replace `/dev/ttyUSB0` with your real device (often `/dev/ttyACM0` on ESP32-S3 USB CDC setups).

## Publish commands from ROS 2

You can keep using your existing helper:

```bash
python3 ../gripper_ctrl.py
```

Or publish directly:

```bash
ros2 topic pub /esp32_rx_int32 std_msgs/msg/Int32 "{data: 1}" -1
ros2 topic pub /esp32_rx_int32 std_msgs/msg/Int32 "{data: 0}" -1
```
