#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from std_msgs.msg import Int32

rclpy.init()
node = Node('led_ctrl')
pub = node.create_publisher(Int32, 'esp32_rx_int32', 10)

print("Ready. Type 0 or 1, then Enter. Ctrl+C to quit.")
try:
    while True:
        val = input("> ")
        msg = Int32()
        msg.data = int(val)
        pub.publish(msg)
except KeyboardInterrupt:
    pass

node.destroy_node()
rclpy.shutdown()
