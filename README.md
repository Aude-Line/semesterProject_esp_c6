# esp

## Intro
This folder contains two ESP projects for controlling the gripper pin (GPIO6) through micro-ROS on ESP32-C6:

- `gripper_ctrl`: Wi-Fi/UDP transport
- `gripper_ctrl_usb`: USB serial transport over UART

Both projects subscribe to the same ROS 2 topic:

- Topic: `esp32_rx_int32`
- Type: `std_msgs/msg/Int32`
- Valid commands: `0` (off), `1` (on)

## LED status (both versions)

The onboard RGB LED indicates communication/output state:

- Red: agent not reachable (startup retry or connection lost)
- Off: agent connected and output pin is off (`0`)
- White: agent connected and output pin is on (`1`)

## Common setup (WSL + Docker)

Install Docker:

```bash
sudo apt update
sudo apt install -y docker.io
```

Check if ESP USB is visible in WSL:

```bash
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

If nothing appears, bind and attach the ESP USB device from **Windows PowerShell (Run as Administrator)**:
Find the ESP busid (example: 1-5):
```powershell
usbipd list
```
Then use the right bus as BUSID:
```powershell
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```

Notes:
- `bind` is persistent, so you usually do it once per device.
- `attach` is needed each time the device is re-plugged or after reboot.

Create ESP-IDF Docker image:

```bash
sudo docker pull espressif/idf:release-v5.2
sudo docker run -it --privileged --name esp_idf -v "$HOME/semesterProject_LMTS/esp:/work" -w /work espressif/idf:release-v5.2
```

Install micro-ROS Python tooling inside container (recommended once):

```bash
python3 -m pip install --no-cache-dir colcon-common-extensions vcstool catkin_pkg "empy==3.3.4" "lark-parser==0.12.0"
```

Then the next time you'll need it, restart it with:

```bash
sudo docker start -ai esp_idf
```

## Wi-Fi version (`gripper_ctrl`)

For the Wi-Fi version, the host computer and the ESP must be connected to the same Wi-Fi network. After flashing the correct firmware, the ESP no longer needs a USB data connection to the computer. It only needs USB power (for example from a computer USB port, a power bank, ...)

Recommended order:
1. Start the micro-ROS agent.
2. Connect the ESP to the computer via USB cable.
3. Build and flash the ESP.
4. Run the control script.

If the ESP was already flashed, for the Wi-Fi version:
1. Start the micro-ROS agent.
2. Power on the ESP, or press the reset button if it was already powered on before starting the agent.
3. Run the control script.

### Run micro-ROS agent (Wi-Fi/UDP)

Start the agent first. It must already be running when the ESP firmware initializes, otherwise the connection can fail.

```bash
sudo docker run -it --rm --net=host microros/micro-ros-agent:humble udp4 --port 8888 -v6
```

### Build and flash

Inside container:

```bash
rm -rf /work/gripper_ctrl/build
rm -rf /work/micro_ros_espidf_component/micro_ros_src
rm -rf /work/micro_ros_espidf_component/micro_ros_dev

cd /work/gripper_ctrl
idf.py set-target esp32c6
idf.py menuconfig
idf.py build
idf.py flash
idf.py monitor
```

In `menuconfig` configure Wi-Fi and agent UDP parameters:

- micro-ROS Settings -> micro-ROS network interface: WLAN
- micro-ROS Settings -> Wi-Fi configuration -> Wi-Fi SSID/password
- micro-ROS Settings -> Agent IP and port (default 8888)

Get the agent host IP (WSL terminal):

```bash
hostname -I
```

Use the first IPv4 address shown there as `Agent IP` in `menuconfig`.

Tip:
- If your WSL IP changes after reboot/network reconnect, run `hostname -I` again and update `Agent IP` before rebuilding/flashing.

## USB version (`gripper_ctrl_usb`)
First, build and flash the ESP, as the agent monopolizes the UART port.
Then start the agent. Since it must already be running when the ESP firmware initializes, press the reset button on the ESP.

For this version, you want to disconnect the USB cable as little as possible, as every time the cable is disconnected the port needs to be re-attached from PowerShell as Administrator (see section above).

Recommended order:
1. Connect the ESP to the computer via USB cable.
2. Build and flash the ESP.
3. Start the micro-ROS agent.
4. Press the reset button on the ESP.
5. Run the control script.

If the ESP was already flashed, for the USB version:
1. Connect the ESP to the computer via USB cable.
2. Start the micro-ROS agent.
3. Press the reset button on the ESP.
4. Run the control script.

### Run micro-ROS agent (USB serial)

Use the actual port shown by `ls /dev/ttyUSB* /dev/ttyACM*`:

```bash
sudo docker run -it --rm --net=host --device=/dev/ttyUSB0 microros/micro-ros-agent:humble serial --dev /dev/ttyUSB0 -b 115200 -v6
```

If your device is on another port, replace accordingly.

### Build and flash

Inside container:

```bash
rm -rf /work/gripper_ctrl_usb/build
rm -rf /work/micro_ros_espidf_component/micro_ros_src
rm -rf /work/micro_ros_espidf_component/micro_ros_dev

cd /work/gripper_ctrl_usb
idf.py set-target esp32c6
idf.py menuconfig
idf.py build
idf.py flash
```

In `menuconfig` set UART transport:

- micro-ROS Settings -> micro-ROS middleware: micro-ROS over eProsima Micro XRCE-DDS
- micro-ROS Settings -> Micro XRCE-DDS over UART
- micro-ROS Settings -> UART Settings:
	- TX pin: `16`
	- RX pin: `17`
	- RTS pin: `-1`
	- CTS pin: `-1`

Important:
- Do not run `idf.py monitor` on the same serial port while running the serial micro-ROS agent, because XRCE binary traffic and logs will mix.

## Run the control script (both versions)
To test if the setup is correct, run the test control script.

In a ROS 2 shell:

```bash
cd ~/semesterProject_LMTS/esp
source ~/ros2_humble/install/setup.bash
python3 gripper_ctrl.py
```

The script accepts only `0` or `1` and prints `wrong character` for any other input.