#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
原地旋转测试脚本

功能：
1. 测试机器人原地旋转功能
2. 每10秒递增旋转速度
3. 持续显示IMU和Odometry数据

使用方法：
    python3 test_spin_rotation.py [--robot-name robot01] [--carla-host localhost] [--carla-port 2000]
"""

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Imu
from nav_msgs.msg import Odometry
from geometry_msgs.msg import Twist
import time
import argparse
import sys
import math

# 尝试导入 carla
try:
    import carla
    CARLA_AVAILABLE = True
except ImportError:
    CARLA_AVAILABLE = False
    print("警告: carla 模块未找到")


class IMUSubscriber(Node):
    """IMU 数据订阅者"""
    
    def __init__(self, robot_name='robot01'):
        super().__init__('imu_subscriber')
        self.robot_name = robot_name
        self.topic_name = f'/carla/{robot_name}/imu/imu_data'
        self.subscription = self.create_subscription(
            Imu,
            self.topic_name,
            self.imu_callback,
            10)
        self.message_count = 0
        self.last_print_time = time.time()
        self.latest_msg = None
        
    def imu_callback(self, msg):
        """IMU 数据回调函数"""
        self.message_count += 1
        self.latest_msg = msg
        current_time = time.time()
        
        # 每 0.5 秒打印一次数据
        if current_time - self.last_print_time >= 0.5:
            self.get_logger().info(
                f'[IMU #{self.message_count}] '
                f'角速度Z: {msg.angular_velocity.z:.3f} rad/s ({math.degrees(msg.angular_velocity.z):.1f}°/s)'
            )
            self.last_print_time = current_time


class OdometrySubscriber(Node):
    """Odometry 数据订阅者"""
    
    def __init__(self, robot_name='robot01'):
        super().__init__('odometry_subscriber')
        self.robot_name = robot_name
        self.topic_name = f'/carla/{robot_name}/odom'
        self.subscription = self.create_subscription(
            Odometry,
            self.topic_name,
            self.odom_callback,
            10)
        self.message_count = 0
        self.last_print_time = time.time()
        self.latest_msg = None
        
    def odom_callback(self, msg):
        """Odometry 数据回调函数"""
        self.message_count += 1
        self.latest_msg = msg
        current_time = time.time()
        
        # 每 0.5 秒打印一次数据
        if current_time - self.last_print_time >= 0.5:
            pos = msg.pose.pose.position
            ang_vel = msg.twist.twist.angular
            
            # 从四元数计算yaw角度
            q = msg.pose.pose.orientation
            yaw = math.atan2(2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z))
            yaw_deg = math.degrees(yaw)
            
            self.get_logger().info(
                f'[Odom #{self.message_count}] '
                f'位置: ({pos.x:.2f}, {pos.y:.2f}) | '
                f'Yaw: {yaw_deg:.1f}° | '
                f'角速度Z: {ang_vel.z:.3f} rad/s ({math.degrees(ang_vel.z):.1f}°/s)'
            )
            self.last_print_time = current_time


class RobotController(Node):
    """机器人控制器"""
    
    def __init__(self, robot_name='robot01'):
        super().__init__('robot_controller')
        self.robot_name = robot_name
        
        # 创建 cmd_vel 发布者
        self.cmd_vel_publisher = self.create_publisher(
            Twist,
            f'/carla/{robot_name}/cmd_vel',
            10)
        
        self.get_logger().info(f'已创建控制发布者: cmd_vel')
        
    def send_cmd_vel(self, linear_velocity=0.0, angular_velocity=0.0):
        """
        发送速度控制命令 (cmd_vel)
        
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
        # 减少日志输出频率，避免影响性能
        # self.get_logger().info(
        #     f'[cmd_vel] 发布: 线速度={linear_velocity:.3f} m/s, '
        #     f'角速度={angular_velocity:.3f} rad/s ({math.degrees(angular_velocity):.1f}°/s)'
        # )


def get_robot_actor(world, robot_name='robot01'):
    """通过名称查找机器人actor"""
    if not CARLA_AVAILABLE:
        return None
    
    try:
        # 尝试通过 role_name 查找
        actors = world.get_actors()
        for actor in actors:
            if hasattr(actor, 'attributes') and actor.attributes.get('role_name') == robot_name:
                return actor
            # 也尝试通过类型和名称匹配
            if 'robot' in str(actor.type_id).lower() or robot_name in str(actor.id):
                return actor
    except Exception as e:
        print(f"查找机器人actor时出错: {e}")
    
    return None


def test_spin_rotation(robot_name='robot01', carla_host='localhost', carla_port=2000, test_duration=120):
    """主测试函数"""
    rclpy.init()
    
    # 连接到CARLA（如果可用）
    robot_actor = None
    carla_world = None
    if CARLA_AVAILABLE:
        try:
            client = carla.Client(carla_host, carla_port)
            client.set_timeout(10.0)
            carla_world = client.get_world()
            robot_actor = get_robot_actor(carla_world, robot_name)
            if robot_actor:
                print(f"✓ 找到机器人actor: {robot_actor.id} ({robot_actor.type_id})")
            else:
                print(f"⚠ 未找到机器人actor '{robot_name}'")
        except Exception as e:
            print(f"⚠ 连接CARLA失败: {e}")
    
    # 创建订阅者
    imu_sub = IMUSubscriber(robot_name)
    odom_sub = OdometrySubscriber(robot_name)
    
    # 创建控制器
    controller = RobotController(robot_name)
    
    # 创建执行器
    executor = rclpy.executors.MultiThreadedExecutor()
    executor.add_node(imu_sub)
    executor.add_node(odom_sub)
    executor.add_node(controller)
    
    print(f'\n{"="*70}')
    print(f'原地旋转测试')
    print(f'{"="*70}')
    print(f'机器人名称: {robot_name}')
    print(f'测试时长: {test_duration} 秒')
    print(f'CARLA连接: {"✓" if robot_actor else "✗"}')
    print(f'\n订阅话题:')
    print(f'  - {imu_sub.topic_name}')
    print(f'  - {odom_sub.topic_name}')
    print(f'\n发布话题:')
    print(f'  - /carla/{robot_name}/cmd_vel')
    print(f'\n测试说明:')
    print(f'  - 每10秒递增旋转速度')
    print(f'  - 线速度始终为0（原地旋转）')
    print(f'  - 角速度从 0.1 rad/s 开始，每次增加 0.1 rad/s')
    print(f'{"="*70}\n')
    
    # 等待话题连接
    print('等待话题连接...')
    time.sleep(2.0)
    
    start_time = time.time()
    last_control_time = time.time()
    control_interval = 0.02  # 20ms 控制周期（50Hz，更频繁的控制以确保连贯性）
    
    # 旋转速度配置
    current_speed_index = 0
    speed_stage_start_time = time.time()
    speed_stage_duration = 10.0  # 每个速度阶段持续10秒
    angular_velocities = [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.2, 1.5, 2.0]  # rad/s
    
    print(f'\n开始测试...\n')
    
    try:
        while rclpy.ok():
            current_time = time.time()
            elapsed = current_time - start_time
            
            if elapsed >= test_duration:
                break
            
            # 非阻塞执行ROS2回调
            executor.spin_once(timeout_sec=0.01)
            
            # 控制逻辑（每20ms执行一次，50Hz频率确保连贯性）
            if current_time - last_control_time >= control_interval:
                last_control_time = current_time
                
                # 检查是否需要切换到下一个速度阶段
                speed_stage_elapsed = current_time - speed_stage_start_time
                if speed_stage_elapsed >= speed_stage_duration:
                    current_speed_index += 2
                    speed_stage_start_time = current_time
                    if current_speed_index >= len(angular_velocities):
                        current_speed_index = 0  # 循环回到第一个速度
                    print(f'\n[{elapsed:.1f}s] 切换到速度阶段 {current_speed_index + 1}/{len(angular_velocities)}: '
                          f'角速度 = {angular_velocities[current_speed_index]:.2f} rad/s '
                          f'({math.degrees(angular_velocities[current_speed_index]):.1f}°/s)\n')
                
                # 获取当前角速度
                current_angular_velocity = angular_velocities[current_speed_index]
                
                # 发送控制命令：线速度为0，角速度递增
                # 注意：即使角速度不变，也要持续发送消息以保持连贯旋转
                msg = Twist()
                msg.linear.x = 0.0
                msg.linear.y = 0.0
                msg.linear.z = 0.0
                msg.angular.x = 0.0
                msg.angular.y = 0.0
                msg.angular.z = float(current_angular_velocity)
                controller.cmd_vel_publisher.publish(msg)
            
            time.sleep(0.005)  # 5ms sleep，避免CPU占用过高但保持高频率
            
    except KeyboardInterrupt:
        print('\n\n用户中断测试')
    finally:
        # 停止机器人
        print('\n发送停止命令...')
        controller.send_cmd_vel(linear_velocity=0.0, angular_velocity=0.0)
        time.sleep(0.5)
        
        # 打印统计信息
        print(f'\n{"="*70}')
        print(f'测试完成统计:')
        print(f'{"="*70}')
        print(f'IMU 消息接收数: {imu_sub.message_count}')
        print(f'Odometry 消息接收数: {odom_sub.message_count}')
        print(f'测试时长: {time.time() - start_time:.1f} 秒')
        print(f'测试的速度阶段数: {current_speed_index + 1}')
        print(f'{"="*70}\n')
        
        # 清理
        imu_sub.destroy_node()
        odom_sub.destroy_node()
        controller.destroy_node()
        rclpy.shutdown()


def main():
    parser = argparse.ArgumentParser(
        description='原地旋转测试脚本',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 使用默认参数测试
  python3 test_spin_rotation.py
  
  # 指定机器人名称和CARLA连接
  python3 test_spin_rotation.py --robot-name robot01 --carla-host localhost --carla-port 2000 --test-duration 120
        """
    )
    parser.add_argument(
        '--robot-name',
        default='robot01',
        help='机器人名称（默认: robot01）'
    )
    parser.add_argument(
        '--carla-host',
        default='localhost',
        help='CARLA服务器地址（默认: localhost）'
    )
    parser.add_argument(
        '--carla-port',
        type=int,
        default=2000,
        help='CARLA服务器端口（默认: 2000）'
    )
    parser.add_argument(
        '--test-duration',
        type=int,
        default=120,
        help='测试时长（秒，默认: 120）'
    )
    
    args = parser.parse_args()
    
    try:
        test_spin_rotation(
            args.robot_name,
            args.carla_host,
            args.carla_port,
            args.test_duration
        )
    except Exception as e:
        print(f'错误: {e}', file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()

