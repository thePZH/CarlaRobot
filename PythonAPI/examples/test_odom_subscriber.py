#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Odometry 话题订阅脚本

功能：
1. 订阅 CARLA 导出的 ROS2 里程计话题
2. 周期性打印位姿、线速度与角速度

使用方法：
	python3 test_odom_subscriber.py [--robot-name robot01] [--topic /carla/robot01/odom]
"""

import argparse
import math
import time

import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry


class OdometrySubscriber(Node):
	"""Odometry 数据订阅者"""

	def __init__(self, robot_name: str, topic_name: str):
		super().__init__('odom_subscriber')
		self.m_RobotName = robot_name
		self.m_TopicName = topic_name or f'/carla/{robot_name}/odom'
		self.m_LastPrintTime = time.time()
		self.m_MessageCount = 0

		self.m_Subscription = self.create_subscription(
			Odometry,
			self.m_TopicName,
			self._on_odom_message,
			10)

		self.get_logger().info(f'已订阅 odom 话题: {self.m_TopicName}')

	def _on_odom_message(self, msg: Odometry):
		self.m_MessageCount += 1
		now = time.time()
		if now - self.m_LastPrintTime < 0.1:
			return

		position = msg.pose.pose.position
		orientation = msg.pose.pose.orientation
		linear = msg.twist.twist.linear
		angular = msg.twist.twist.angular

		yaw = math.atan2(
			2.0 * (orientation.w * orientation.z + orientation.x * orientation.y),
			1.0 - 2.0 * (orientation.y * orientation.y + orientation.z * orientation.z))

		print(f'\n[Odometry #{self.m_MessageCount}]')
		print(f'  时间戳: {msg.header.stamp.sec}.{msg.header.stamp.nanosec:09d}')
		print(f'  坐标系: {msg.header.frame_id} -> {msg.child_frame_id}')
		print(f'  位置(m): X={position.x:.3f}, Y={position.y:.3f}, Z={position.z:.3f}')
		print(f'  姿态(四元数): w={orientation.w:.3f}, x={orientation.x:.3f}, y={orientation.y:.3f}, z={orientation.z:.3f}')
		print(f'  Yaw角度: {math.degrees(yaw):.2f}°')
		print(f'  线速度(m/s): X={linear.x:.3f}, Y={linear.y:.3f}, Z={linear.z:.3f}')
		print(f'  角速度(rad/s): X={angular.x:.3f}, Y={angular.y:.3f}, Z={angular.z:.3f}')

		self.m_LastPrintTime = now


def parse_arguments():
	parser = argparse.ArgumentParser(
		description='订阅 CARLA ROS2 odom 话题并输出数据',
		formatter_class=argparse.ArgumentDefaultsHelpFormatter)
	parser.add_argument(
		'--robot-name',
		default='robot01',
		help='CARLA 机器人/车辆的 ros_name')
	parser.add_argument(
		'--topic',
		default='',
		help='自定义 odom 话题名；留空则自动拼接 /carla/{robot_name}/odom')
	return parser.parse_args()


def main():
	args = parse_arguments()
	rclpy.init()
	node = OdometrySubscriber(args.robot_name, args.topic)

	print(f'\n{"=" * 70}')
	print('Odometry 话题订阅')
	print(f'机器人: {args.robot_name}')
	print(f'话题: {node.m_TopicName}')
	print(f'{"=" * 70}\n')

	try:
		rclpy.spin(node)
	except KeyboardInterrupt:
		print('\n用户中断，正在退出...')
	finally:
		print(f'\n共接收 {node.m_MessageCount} 条 odom 消息')
		node.destroy_node()
		rclpy.shutdown()


if __name__ == '__main__':
	main()

