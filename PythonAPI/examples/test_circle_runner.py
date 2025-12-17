#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
无限绕圈跑测试脚本

功能：
1. 持续向 /carla/{robot_name}/cmd_vel 发布线速度 + 角速度，让小车一直绕圈跑
2. 默认线速度 1.0 m/s，角速度 0.5 rad/s，可通过命令行参数调整
3. 无时间限制，仅在用户 Ctrl+C 终止时停止，并自动发送停止命令

使用示例：
    # 使用默认参数（robot01，线速度 1.0 m/s，角速度 0.5 rad/s，频率 20Hz）
    python3 test_circle_runner.py

    # 指定机器人名称
    python3 test_circle_runner.py --robot-name robot02

    # 调整速度和发布频率
    python3 test_circle_runner.py --linear 1.2 --angular 0.8 --rate 30
"""

import argparse
import math
import sys
import time

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist


class CircleRunner(Node):
	"""持续发布 cmd_vel 让小车绕圈跑"""

	def __init__(self, robot_name: str, linear: float, angular: float, rate: float):
		super().__init__('circle_runner')

		self.robot_name = robot_name

		# 限制速度范围，和文档中约定保持一致
		# 线速度：0 ~ 1.67 m/s（约 0~6 km/h）
		self.linear_speed = max(0.0, min(1.67, float(linear)))
		# 角速度：-2.0 ~ 2.0 rad/s
		self.angular_speed = max(-2.0, min(2.0, float(angular)))

		self.rate_hz = max(1.0, float(rate))
		self.period = 1.0 / self.rate_hz

		topic_name = f'/carla/{robot_name}/cmd_vel'
		self.publisher = self.create_publisher(Twist, topic_name, 10)

		self.get_logger().info(
			f'CircleRunner 启动，话题: {topic_name}\n'
			f'  线速度: {self.linear_speed:.3f} m/s ({self.linear_speed * 3.6:.2f} km/h)\n'
			f'  角速度: {self.angular_speed:.3f} rad/s ({math.degrees(self.angular_speed):.1f}°/s)\n'
			f'  发布频率: {self.rate_hz:.1f} Hz'
		)

	def publish_once(self) -> None:
		"""发布一次 cmd_vel"""
		msg = Twist()
		msg.linear.x = self.linear_speed
		msg.linear.y = 0.0
		msg.linear.z = 0.0
		msg.angular.x = 0.0
		msg.angular.y = 0.0
		msg.angular.z = self.angular_speed

		self.publisher.publish(msg)

	def stop(self, repeat: int = 5) -> None:
		"""发送若干次停止命令，确保小车停下"""
		self.get_logger().info('发送停止命令，让小车减速停车...')
		stop_msg = Twist()
		for _ in range(repeat):
			self.publisher.publish(stop_msg)
			rclpy.spin_once(self, timeout_sec=0.05)
			time.sleep(0.05)


def parse_arguments():
	parser = argparse.ArgumentParser(
		description='让小车以固定线速度和角速度无限绕圈跑（需要 Ctrl+C 手动退出）',
		formatter_class=argparse.RawDescriptionHelpFormatter,
		epilog="""
示例:
  # 默认参数
  python3 test_circle_runner.py

  # 指定机器人 + 调整线速度和角速度
  python3 test_circle_runner.py --robot-name robot02 --linear 1.2 --angular 0.8

  # 提高控制频率
  python3 test_circle_runner.py --rate 50
		"""
	)

	parser.add_argument(
		'--robot-name',
		default='robot01',
		help='CARLA 机器人/车辆的 ros_name（默认: robot01）',
	)
	parser.add_argument(
		'--linear',
		type=float,
		default=1.0,
		help='线速度 (m/s)，建议范围 0.0 ~ 1.67，默认 1.0',
	)
	parser.add_argument(
		'--angular',
		type=float,
		default=0.5,
		help='角速度 (rad/s)，建议范围 -2.0 ~ 2.0，默认 0.5',
	)
	parser.add_argument(
		'--rate',
		type=float,
		default=20.0,
		help='发布频率 (Hz)，默认 20.0',
	)

	return parser.parse_args()


def main():
	args = parse_arguments()

	rclpy.init()
	node = CircleRunner(
		robot_name=args.robot_name,
		linear=args.linear,
		angular=args.angular,
		rate=args.rate,
	)

	print(f'\n{"=" * 70}')
	print('无限绕圈跑测试 (Ctrl+C 退出)')
	print(f'机器人: {args.robot_name}')
	print(f'线速度: {node.linear_speed:.3f} m/s ({node.linear_speed * 3.6:.2f} km/h)')
	print(f'角速度: {node.angular_speed:.3f} rad/s ({math.degrees(node.angular_speed):.1f}°/s)')
	print(f'发布频率: {node.rate_hz:.1f} Hz')
	print(f'话题: /carla/{args.robot_name}/cmd_vel')
	print(f'{"=" * 70}\n')

	last_time = time.time()

	try:
		while rclpy.ok():
			now = time.time()
			if now - last_time >= node.period:
				last_time = now
				node.publish_once()

			# 非阻塞处理回调（虽然当前节点没有订阅者，但保持一致写法）
			rclpy.spin_once(node, timeout_sec=0.001)

			# 小 sleep 避免 CPU 满载
			time.sleep(0.001)

	except KeyboardInterrupt:
		print('\n捕获到 Ctrl+C，准备停止小车并退出...')
	finally:
		try:
			node.stop()
		except Exception:
			pass

		node.destroy_node()
		rclpy.shutdown()
		print('已退出 CircleRunner。')


if __name__ == '__main__':
	main()


