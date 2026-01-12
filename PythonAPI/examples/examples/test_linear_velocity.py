#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
测试线速度设置脚本

功能：
1. 发布cmd_vel话题
2. 线速度设置为5 m/s
3. 发布频率为1Hz
4. 角速度设置为0（只测试线速度）

使用方法：
    python3 test_linear_velocity.py [--robot-name robot01] [--test-duration 30]
"""

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import argparse
import sys
import time
import math


class LinearVelocityPublisher(Node):
	"""线速度测试发布者"""
	
	def __init__(self, robot_name='robot01'):
		super().__init__('linear_velocity_publisher')
		self.robot_name = robot_name
		
		# 创建 cmd_vel 发布者
		# 话题名称格式：/carla/{robot_name}/cmd_vel
		self.cmd_vel_publisher = self.create_publisher(
			Twist,
			f'/carla/{robot_name}/cmd_vel',
			10)
		
		self.get_logger().info(f'已创建cmd_vel发布者: /carla/{robot_name}/cmd_vel')
		self.publish_count = 0
	
	def publish_cmd_vel(self, linear_velocity=0.0, angular_velocity=0.0):
		"""
		发布速度控制命令 (cmd_vel)
		
		参数:
		- linear_velocity: 线速度 (m/s)
		- angular_velocity: 角速度 (rad/s)
		"""
		msg = Twist()
		msg.linear.x = float(linear_velocity)
		msg.linear.y = 0.0
		msg.linear.z = 0.0
		msg.angular.x = 0.0
		msg.angular.y = 0.0
		msg.angular.z = float(angular_velocity)
		
		self.cmd_vel_publisher.publish(msg)
		self.publish_count += 1
		
		self.get_logger().info(
			f'[发布 #{self.publish_count}] '
			f'线速度={linear_velocity:.2f} m/s, '
			f'角速度={angular_velocity:.2f} rad/s ({math.degrees(angular_velocity):.1f}°/s)'
		)


def test_linear_velocity(robot_name='robot01', test_duration=30.0):
	"""测试线速度设置"""
	rclpy.init()
	
	# 创建发布者
	publisher = LinearVelocityPublisher(robot_name)
	
	# 固定线速度
	TARGET_LINEAR_VELOCITY = 50.0  # m/s
	PUBLISH_FREQUENCY = 1.0  # Hz
	PUBLISH_INTERVAL = 1.0 / PUBLISH_FREQUENCY  # 秒
	
	print(f'\n{"="*70}')
	print(f'测试线速度设置')
	print(f'{"="*70}')
	print(f'机器人名称: {robot_name}')
	print(f'目标线速度: {TARGET_LINEAR_VELOCITY:.2f} m/s ({TARGET_LINEAR_VELOCITY * 3.6:.2f} km/h)')
	print(f'发布频率: {PUBLISH_FREQUENCY} Hz (每{PUBLISH_INTERVAL:.1f}秒发布一次)')
	print(f'测试时长: {test_duration} 秒')
	print(f'发布话题: /carla/{robot_name}/cmd_vel')
	print(f'{"="*70}\n')
	
	# 等待话题连接
	print('等待话题连接...')
	time.sleep(2.0)
	
	start_time = time.time()
	last_publish_time = 0.0
	
	print(f'\n开始测试...\n')
	
	try:
		while rclpy.ok():
			current_time = time.time()
			elapsed = current_time - start_time
			
			if elapsed >= test_duration:
				break
			
			# 非阻塞执行ROS2回调
			rclpy.spin_once(publisher, timeout_sec=0.01)
			
			# 按1Hz频率发布命令
			if current_time - last_publish_time >= PUBLISH_INTERVAL:
				publisher.publish_cmd_vel(
					linear_velocity=TARGET_LINEAR_VELOCITY,
					angular_velocity=0.0
				)
				last_publish_time = current_time
			
			time.sleep(0.01)  # 避免CPU占用过高
			
	except KeyboardInterrupt:
		print('\n\n用户中断测试')
	
	# 停止机器人
	print('\n停止机器人...')
	for _ in range(5):
		msg = Twist()
		msg.linear.x = 0.0
		msg.angular.z = 0.0
		publisher.cmd_vel_publisher.publish(msg)
		rclpy.spin_once(publisher, timeout_sec=0.1)
	time.sleep(1.0)
	
	# 打印统计信息
	print(f'\n{"="*70}')
	print(f'测试完成统计:')
	print(f'{"="*70}')
	print(f'发布命令数: {publisher.publish_count}')
	print(f'测试时长: {time.time() - start_time:.1f} 秒')
	print(f'{"="*70}\n')
	
	# 清理
	publisher.destroy_node()
	rclpy.shutdown()


def main():
	parser = argparse.ArgumentParser(
		description='测试线速度设置（发布cmd_vel，线速度5m/s，频率1Hz）',
		formatter_class=argparse.RawDescriptionHelpFormatter,
		epilog="""
示例:
  # 使用默认参数（robot01，30秒）
  python3 test_linear_velocity.py
  
  # 指定机器人名称和测试时长
  python3 test_linear_velocity.py --robot-name robot01 --test-duration 60
		"""
	)
	parser.add_argument(
		'--robot-name',
		default='robot01',
		help='机器人名称（默认: robot01）'
	)
	parser.add_argument(
		'--test-duration',
		type=float,
		default=30.0,
		help='测试时长（秒，默认: 30.0）'
	)
	
	args = parser.parse_args()
	
	try:
		test_linear_velocity(
			args.robot_name,
			args.test_duration
		)
	except Exception as e:
		print(f'错误: {e}', file=sys.stderr)
		import traceback
		traceback.print_exc()
		sys.exit(1)


if __name__ == '__main__':
	main()
