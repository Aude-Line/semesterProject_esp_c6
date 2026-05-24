# esp

## Intro
This is to control the activation of the pin6 via microros on an ESP c6

## Install Docker

On Ubuntu 22.04 (WSL), easiest install:

```bash
sudo apt update
sudo apt install -y docker.io
```

It is also possible to add docker to the usermd to not need to use sudo for commands, I didnt do it

## get the IP

```bash
hostname -I
```

## Build and flash ESP

Check if the USB is visible on WSL

```bash
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

If nothing appears, bind and attach the ESP USB device from **Windows PowerShell (Run as Administrator)**:

```powershell
usbipd list
# Find the ESP busid (example: 1-5)
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```

Notes:
- `bind` is persistent, so you usually do it once per device.
- `attach` is needed each time the device is re-plugged or after reboot.

Then verify again in WSL:

```bash
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Pull the docker image compatible with the microros component and link the docker files to the wsl files.

```bash
sudo docker pull espressif/idf:release-v5.2

sudo docker run --rm -it --privileged -v "$HOME/semesterProject_LMTS/esp:/work" -w /work espressif/idf:release-v5.2
```

This container is started with `--rm`, so it is removed automatically when you exit.
If you want to keep it, run it once without `--rm` and with a name:

```bash
sudo docker run -it --privileged --name esp_idf -v "$HOME/semesterProject_LMTS/esp:/work" -w /work espressif/idf:release-v5.2
```

Then restart it later with:

```bash
sudo docker start -ai esp_idf
```

then inside the docker

Install the missing Python tooling required by the micro-ROS build (recommended before first build):

```bash
python3 -m pip install --no-cache-dir colcon-common-extensions vcstool catkin_pkg "empy==3.3.4" "lark-parser==0.12.0"
```

If you had previous failed attempts, clean intermediate folders before rebuilding:

```bash
rm -rf gripper_ctrl/build
rm -rf /work/micro_ros_espidf_component/micro_ros_src
rm -rf /work/micro_ros_espidf_component/micro_ros_dev
```

Then configure and build. In `menuconfig` set the Wi-Fi parameters. If only one USB is connected, no need to define the USB port:

```bash
cd gripper_ctrl
idf.py set-target esp32c6
idf.py menuconfig
idf.py build
idf.py flash
idf.py monitor
```

## Run the microros agent

The agent needs to run (in it's own terminal) to allows the connection between the ESP and the computer, start the agent first (before powering or flashing the ESP).

```bash
sudo docker run -it --rm --net=host microros/micro-ros-agent:humble udp4 --port 8888 -v6
```

## Run the control file

Source humble
run python file

this will create a ros2 publisher and send on/off commands to the gripper

```bash
cd ~/semesterProject_LMTS/esp

source ~/ros2_humble/install/setup.bash

python3 gripper_ctrl.py

```


TODO add usb version