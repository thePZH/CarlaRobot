#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
ROS2 机器人控制测试脚本

功能：
1. 订阅并验证 /carla/robot01/imu/imu_data 话题（持续显示数据）
2. 订阅并验证 /carla/robot01/odom 话题（持续显示数据）
3. 发布 /carla/robot01/cmd_vel 控制机器人移动（线速度 m/s 和角速度 rad/s）
4. 发布 /carla/robot01/cmd_ptz 控制云台
5. 实现跑来跑去、左转右转、原地转圈功能
6. 原地转圈时使用 set_transform 设置 rotation

消息格式说明：

1. cmd_vel (geometry_msgs::msg::Twist)
   - linear.x: 线速度 (0 ~ 1.67 m/s，对应 0 ~ 6 km/h)
   - angular.z: 角速度 (-2.0 ~ 2.0 rad/s，对应 -1.0 ~ 1.0 转向比例)
   - 其他字段未使用

2. cmd_ptz (geometry_msgs::msg::Twist)
   - angular.x: FOV (视场角)
   - angular.y: Pitch (俯仰角)
   - angular.z: Yaw (偏航角)
   - linear 字段未使用

使用方法：
    python3 test_ros2_robot_control.py [--robot-name robot01] [--carla-host localhost] [--carla-port 2000]
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
    print("警告: carla 模块未找到，原地转圈功能将不可用")


class IMUSubscriber(Node):
    """IMU 数据订阅者 - 持续显示数据"""
    
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
        
        # 每 0.5 秒打印一次数据（更频繁显示）
        if current_time - self.last_print_time >= 0.5:
            self.get_logger().info(
                f'[IMU #{self.message_count}] '
                f'加速度: ({msg.linear_acceleration.x:.3f}, {msg.linear_acceleration.y:.3f}, {msg.linear_acceleration.z:.3f}) m/s² | '
                f'角速度: ({msg.angular_velocity.x:.3f}, {msg.angular_velocity.y:.3f}, {msg.angular_velocity.z:.3f}) rad/s'
            )
            self.last_print_time = current_time


class OdometrySubscriber(Node):
    """Odometry 数据订阅者 - 持续显示数据"""
    
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
        
        # 每 0.5 秒打印一次数据（更频繁显示）
        if current_time - self.last_print_time >= 0.5:
            pos = msg.pose.pose.position
            lin_vel = msg.twist.twist.linear
            ang_vel = msg.twist.twist.angular
            
            self.get_logger().info(
                f'[Odom #{self.message_count}] '
                f'位置: ({pos.x:.2f}, {pos.y:.2f}, {pos.z:.2f}) m | '
                f'线速度: ({lin_vel.x:.3f}, {lin_vel.y:.3f}, {lin_vel.z:.3f}) m/s | '
                f'角速度: ({ang_vel.x:.3f}, {ang_vel.y:.3f}, {ang_vel.z:.3f}) rad/s'
            )
            self.last_print_time = current_time


class RobotController(Node):
    """机器人控制器（发布 cmd_vel 和 cmd_ptz）"""
    
    def __init__(self, robot_name='robot01'):
        super().__init__('robot_controller')
        self.robot_name = robot_name
        
        # 创建 cmd_vel 发布者
        self.cmd_vel_publisher = self.create_publisher(
            Twist,
            f'/carla/{robot_name}/cmd_vel',
            10)
        
        # 创建 cmd_ptz 发布者
        self.cmd_ptz_publisher = self.create_publisher(
            Twist,
            f'/carla/{robot_name}/cmd_ptz',
            10)
        
        self.get_logger().info(f'已创建控制发布者: cmd_vel, cmd_ptz')
        
    def send_cmd_vel(self, linear_velocity=0.0, angular_velocity=0.0):
        """
        发送速度控制命令 (cmd_vel)
        
        参数:
        - linear_velocity: 线速度 (m/s)，范围 0 ~ 1.67 (对应 0 ~ 6 km/h)
        - angular_velocity: 角速度 (rad/s)，范围 -2.0 ~ 2.0 (对应 -1.0 ~ 1.0 转向比例)
        """
        msg = Twist()
        # 限制线速度范围: 0 ~ 1.67 m/s
        msg.linear.x = float(max(0.0, min(1.67, linear_velocity)))
        msg.linear.y = 0.0
        msg.linear.z = 0.0
        msg.angular.x = 0.0
        msg.angular.y = 0.0
        # 限制角速度范围: -2.0 ~ 2.0 rad/s
        msg.angular.z = float(max(-2.0, min(2.0, angular_velocity)))
        
        self.cmd_vel_publisher.publish(msg)
        self.get_logger().info(
            f'[cmd_vel] 发布: 线速度={msg.linear.x:.3f} m/s ({msg.linear.x*3.6:.2f} km/h), '
            f'角速度={msg.angular.z:.3f} rad/s ({math.degrees(msg.angular.z):.1f}°/s)'
        )
        
    def send_cmd_ptz(self, pitch=0.0, yaw=0.0, fov=0.0):
        """
        发送云台控制命令 (cmd_ptz)
        
        参数映射（根据 SVGimbalMovementListener::Twist2GimbalControl）:
        - angular.x: FOV (视场角)
        - angular.y: Pitch (俯仰角)
        - angular.z: Yaw (偏航角)
        """
        msg = Twist()
        msg.linear.x = 0.0
        msg.linear.y = 0.0
        msg.linear.z = 0.0
        msg.angular.x = float(fov)
        msg.angular.y = float(pitch)
        msg.angular.z = float(yaw)
        
        self.cmd_ptz_publisher.publish(msg)
        self.get_logger().info(
            f'[cmd_ptz] 发布: pitch={pitch:.2f}, yaw={yaw:.2f}, fov={fov:.2f}'
        )


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


def test_robot_control(robot_name='robot01', carla_host='localhost', carla_port=2000, test_duration=60):
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
                print(f"⚠ 未找到机器人actor '{robot_name}'，原地转圈功能将不可用")
        except Exception as e:
            print(f"⚠ 连接CARLA失败: {e}，原地转圈功能将不可用")
    
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
    print(f'ROS2 机器人控制测试')
    print(f'{"="*70}')
    print(f'机器人名称: {robot_name}')
    print(f'测试时长: {test_duration} 秒')
    print(f'CARLA连接: {"✓" if robot_actor else "✗"}')
    print(f'\n订阅话题:')
    print(f'  - {imu_sub.topic_name}')
    print(f'  - {odom_sub.topic_name}')
    print(f'\n发布话题:')
    print(f'  - /carla/{robot_name}/cmd_vel (线速度: 0~1.67 m/s, 角速度: -2.0~2.0 rad/s)')
    print(f'  - /carla/{robot_name}/cmd_ptz')
    print(f'\n{"="*70}\n')
    
    # 等待话题连接
    print('等待话题连接...')
    time.sleep(2.0)
    
    start_time = time.time()
    last_control_time = time.time()
    control_interval = 0.1  # 100ms 控制周期
    
    # 测试阶段状态
    test_stage = 0
    stage_start_time = time.time()
    stage_duration = 5.0  # 每个阶段持续5秒
    
    # 原地转圈相关
    spin_start_time = None
    spin_angular_velocity = 0.0  # rad/s
    initial_yaw = None  # 初始yaw角度
    
    try:
        while rclpy.ok():
            current_time = time.time()
            elapsed = current_time - start_time
            
            if elapsed >= test_duration:
                break
            
            # 非阻塞执行ROS2回调
            executor.spin_once(timeout_sec=0.01)
            
            # 控制逻辑（每100ms执行一次）
            if current_time - last_control_time >= control_interval:
                last_control_time = current_time
                stage_elapsed = current_time - stage_start_time
                
                # 检查是否需要切换到下一个阶段
                if stage_elapsed >= stage_duration:
                    test_stage = (test_stage + 1) % 6  # 6个阶段循环
                    stage_start_time = current_time
                    stage_elapsed = 0.0
                    spin_start_time = None  # 重置原地转圈状态
                    initial_yaw = None  # 重置初始yaw角度
                
                # 根据阶段执行不同的控制
                if test_stage == 0:
                    # 阶段0: 前进
                    controller.send_cmd_vel(linear_velocity=1.0, angular_velocity=0.0)
                    if int(stage_elapsed) == 0:
                        print(f'[{elapsed:.1f}s] 阶段0: 前进 (1.0 m/s)')
                
                elif test_stage == 1:
                    # 阶段1: 前进+右转
                    controller.send_cmd_vel(linear_velocity=0.8, angular_velocity=-0.5)
                    if int(stage_elapsed) == 0:
                        print(f'[{elapsed:.1f}s] 阶段1: 前进+右转 (0.8 m/s, -0.5 rad/s)')
                
                elif test_stage == 2:
                    # 阶段2: 前进+左转
                    controller.send_cmd_vel(linear_velocity=0.8, angular_velocity=0.5)
                    if int(stage_elapsed) == 0:
                        print(f'[{elapsed:.1f}s] 阶段2: 前进+左转 (0.8 m/s, 0.5 rad/s)')
                
                elif test_stage == 3:
                    # 阶段3: 停止
                    controller.send_cmd_vel(linear_velocity=0.0, angular_velocity=0.0)
                    if int(stage_elapsed) == 0:
                        print(f'[{elapsed:.1f}s] 阶段3: 停止')
                
                elif test_stage == 4:
                    # 阶段4: 原地转圈（速度为0，角速度不为0）
                    # 使用 set_transform 手动设置 rotation
                    if robot_actor:
                        if spin_start_time is None:
                            spin_start_time = current_time
                            spin_angular_velocity = 1.0  # rad/s
                            # 记录初始yaw角度
                            initial_transform = robot_actor.get_transform()
                            initial_yaw = initial_transform.rotation.yaw
                            print(f'[{elapsed:.1f}s] 阶段4: 原地转圈 (使用set_transform, 初始yaw={initial_yaw:.1f}°)')
                        
                        # 计算当前应该旋转的总角度（从初始角度开始累积）
                        spin_elapsed = current_time - spin_start_time
                        total_rotation_deg = math.degrees(spin_angular_velocity * spin_elapsed)
                        new_yaw = initial_yaw + total_rotation_deg
                        
                        # 获取当前transform（保持位置不变）
                        current_transform = robot_actor.get_transform()
                        new_rotation = carla.Rotation(
                            pitch=current_transform.rotation.pitch,
                            yaw=new_yaw,
                            roll=current_transform.rotation.roll
                        )
                        
                        # 设置新的transform（只改变rotation）
                        new_transform = carla.Transform(
                            location=current_transform.location,
                            rotation=new_rotation
                        )
                        robot_actor.set_transform(new_transform)
                        
                        # 不发送cmd_vel（速度为0）
                        controller.send_cmd_vel(linear_velocity=0.0, angular_velocity=0.0)
                    else:
                        # 如果没有CARLA连接，尝试用cmd_vel（可能不会工作）
                        controller.send_cmd_vel(linear_velocity=0.0, angular_velocity=1.0)
                        if int(stage_elapsed) == 0:
                            print(f'[{elapsed:.1f}s] 阶段4: 原地转圈 (尝试cmd_vel，可能无效)')
                
                elif test_stage == 5:
                    # 阶段5: 停止
                    controller.send_cmd_vel(linear_velocity=0.0, angular_velocity=0.0)
                    if int(stage_elapsed) == 0:
                        print(f'[{elapsed:.1f}s] 阶段5: 停止')
            
            time.sleep(0.01)  # 避免CPU占用过高
            
    except KeyboardInterrupt:
        print('\n\n用户中断测试')
    finally:
        # 停止机器人
        print('\n发送停止命令...')
        controller.send_cmd_vel(linear_velocity=0.0, angular_velocity=0.0)
        controller.send_cmd_ptz(pitch=0.0, yaw=0.0, fov=0.0)
        time.sleep(0.5)
        
        # 打印统计信息
        print(f'\n{"="*70}')
        print(f'测试完成统计:')
        print(f'{"="*70}')
        print(f'IMU 消息接收数: {imu_sub.message_count}')
        print(f'Odometry 消息接收数: {odom_sub.message_count}')
        print(f'测试时长: {time.time() - start_time:.1f} 秒')
        print(f'{"="*70}\n')
        
        # 清理
        imu_sub.destroy_node()
        odom_sub.destroy_node()
        controller.destroy_node()
        rclpy.shutdown()


def main():
    parser = argparse.ArgumentParser(
        description='ROS2 机器人控制测试脚本',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 使用默认参数测试
  python3 test_ros2_robot_control.py
  
  # 指定机器人名称和CARLA连接
  python3 test_ros2_robot_control.py --robot-name robot01 --carla-host localhost --carla-port 2000
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
        default=60,
        help='测试时长（秒，默认: 60）'
    )
    
    args = parser.parse_args()
    
    try:
        test_robot_control(
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
