#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
IMU传感器话题订阅脚本

功能：
1. 订阅IMU传感器话题
2. 将话题内容输出到控制台

使用方法：
    python3 test_imu_subscriber.py [--robot-name robot01]
"""

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Imu
import argparse
import sys
import math
import time


class IMUSubscriber(Node):
	"""IMU 数据订阅者"""
	
	def __init__(self, robot_name='robot01'):
		super().__init__('imu_subscriber')
		self.robot_name = robot_name
		# 话题名称格式：/carla/{robot_name}/imu/imu_data
		self.topic_name = f'/carla/{robot_name}/imu/imu_data'
		
		self.subscription = self.create_subscription(
			Imu,
			self.topic_name,
			self.imu_callback,
			10)
		
		self.message_count = 0
		self.last_print_time = time.time()
		
		self.get_logger().info(f'已订阅IMU话题: {self.topic_name}')
	
	def imu_callback(self, msg):
		"""IMU 数据回调函数"""
		self.message_count += 1
		current_time = time.time()
		
		# 每0.1秒打印一次数据，避免输出过于频繁
		if current_time - self.last_print_time >= 0.1:
			# 线性加速度（m/s²）
			accel = msg.linear_acceleration
			
			# 角速度（rad/s）
			ang_vel = msg.angular_velocity
			
			# 姿态（四元数）
			orient = msg.orientation
			
			# 从四元数计算yaw角度
			yaw = math.atan2(2.0 * (orient.w * orient.z + orient.x * orient.y),
							1.0 - 2.0 * (orient.y * orient.y + orient.z * orient.z))
			yaw_deg = math.degrees(yaw)
			
			# 输出到控制台
			print(f'\n[IMU消息 #{self.message_count}]')
			print(f'  时间戳: {msg.header.stamp.sec}.{msg.header.stamp.nanosec:09d}')
			print(f'  坐标系: {msg.header.frame_id}')
			print(f'  线性加速度: X={accel.x:.3f}, Y={accel.y:.3f}, Z={accel.z:.3f} m/s²')
			print(f'  角速度: X={ang_vel.x:.3f}, Y={ang_vel.y:.3f}, Z={ang_vel.z:.3f} rad/s')
			print(f'  角速度(度/秒): X={math.degrees(ang_vel.x):.1f}°, Y={math.degrees(ang_vel.y):.1f}°, Z={math.degrees(ang_vel.z):.1f}°')
			print(f'  姿态(四元数): w={orient.w:.3f}, x={orient.x:.3f}, y={orient.y:.3f}, z={orient.z:.3f}')
			print(f'  Yaw角度: {yaw_deg:.1f}°')
			
			self.last_print_time = current_time


def main():
	parser = argparse.ArgumentParser(
		description='订阅IMU传感器话题并输出到控制台',
		formatter_class=argparse.RawDescriptionHelpFormatter,
		epilog="""
示例:
  # 使用默认参数（robot01）
  python3 test_imu_subscriber.py
  
  # 指定机器人名称
  python3 test_imu_subscriber.py --robot-name robot01
		"""
	)
	parser.add_argument(
		'--robot-name',
		default='robot01',
		help='机器人名称（默认: robot01）'
	)
	
	args = parser.parse_args()
	
	rclpy.init()
	
	# 创建订阅者
	imu_sub = IMUSubscriber(args.robot_name)
	
	print(f'\n{"="*70}')
	print(f'IMU传感器话题订阅')
	print(f'{"="*70}')
	print(f'机器人名称: {args.robot_name}')
	print(f'订阅话题: {imu_sub.topic_name}')
	print(f'{"="*70}\n')
	print('等待IMU数据...\n')
	
	try:
		rclpy.spin(imu_sub)
	except KeyboardInterrupt:
		print('\n\n用户中断')
	finally:
		# 打印统计信息
		print(f'\n{"="*70}')
		print(f'统计信息:')
		print(f'{"="*70}')
		print(f'接收到的IMU消息数: {imu_sub.message_count}')
		print(f'{"="*70}\n')
		
		# 清理
		imu_sub.destroy_node()
		rclpy.shutdown()


if __name__ == '__main__':
	main()
