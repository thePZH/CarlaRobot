#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
测试 cmd_vel 发布频率对旋转速度的影响

功能：
1. 固定发布 angular.z = 10 rad/s 的旋转速度
2. 每3秒切换一次发布频率：1Hz <-> 20Hz
3. 订阅 odom 和 imu 话题，记录实际角速度数据
4. 按频率分段统计和对比数据

使用方法：
    python3 test_cmd_vel_frequency.py [--robot-name robot01] [--test-duration 60]
"""

import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
from sensor_msgs.msg import Imu
from geometry_msgs.msg import Twist
import time
import argparse
import sys
import math
from collections import defaultdict


class OdometrySubscriber(Node):
    """Odometry 数据订阅者 - 记录实际角速度"""
    
    def __init__(self, robot_name='robot01'):
        super().__init__('odometry_subscriber')
        self.robot_name = robot_name
        self.topic_name = f'/carla/{robot_name}/odom'
        self.subscription = self.create_subscription(
            Odometry,
            self.topic_name,
            self.odom_callback,
            10)
        self.data_by_freq = defaultdict(list)  # 按频率分组存储数据
        self.current_freq = None
        self.message_count = 0
        
    def set_current_freq(self, freq):
        """设置当前频率，用于数据分组"""
        self.current_freq = freq
        
    def odom_callback(self, msg):
        """Odometry 数据回调函数"""
        if self.current_freq is None:
            return
            
        self.message_count += 1
        ang_vel = msg.twist.twist.angular.z
        timestamp = time.time()
        
        self.data_by_freq[self.current_freq].append({
            'angular_vel': ang_vel,
            'timestamp': timestamp
        })
    
    def get_statistics(self, freq):
        """获取指定频率的角速度统计信息"""
        if freq not in self.data_by_freq or not self.data_by_freq[freq]:
            return None
        
        angular_vels = [d['angular_vel'] for d in self.data_by_freq[freq]]
        
        return {
            'count': len(angular_vels),
            'mean': sum(angular_vels) / len(angular_vels),
            'max': max(angular_vels),
            'min': min(angular_vels),
            'std': math.sqrt(sum((x - sum(angular_vels) / len(angular_vels))**2 
                                for x in angular_vels) / len(angular_vels))
        }
    
    def clear_data(self):
        """清空记录的数据"""
        self.data_by_freq.clear()
        self.message_count = 0


class IMUSubscriber(Node):
    """IMU 数据订阅者 - 记录陀螺仪角速度"""
    
    def __init__(self, robot_name='robot01'):
        super().__init__('imu_subscriber')
        self.robot_name = robot_name
        self.topic_name = f'/carla/{robot_name}/imu/imu_data'
        self.subscription = self.create_subscription(
            Imu,
            self.topic_name,
            self.imu_callback,
            10)
        self.data_by_freq = defaultdict(list)  # 按频率分组存储数据
        self.current_freq = None
        self.message_count = 0
        
    def set_current_freq(self, freq):
        """设置当前频率，用于数据分组"""
        self.current_freq = freq
        
    def imu_callback(self, msg):
        """IMU 数据回调函数"""
        if self.current_freq is None:
            return
            
        self.message_count += 1
        # IMU的角速度在angular_velocity字段中，z轴对应偏航角速度
        ang_vel = msg.angular_velocity.z
        timestamp = time.time()
        
        self.data_by_freq[self.current_freq].append({
            'angular_vel': ang_vel,
            'timestamp': timestamp
        })
    
    def get_statistics(self, freq):
        """获取指定频率的角速度统计信息"""
        if freq not in self.data_by_freq or not self.data_by_freq[freq]:
            return None
        
        angular_vels = [d['angular_vel'] for d in self.data_by_freq[freq]]
        
        return {
            'count': len(angular_vels),
            'mean': sum(angular_vels) / len(angular_vels),
            'max': max(angular_vels),
            'min': min(angular_vels),
            'std': math.sqrt(sum((x - sum(angular_vels) / len(angular_vels))**2 
                                for x in angular_vels) / len(angular_vels))
        }
    
    def clear_data(self):
        """清空记录的数据"""
        self.data_by_freq.clear()
        self.message_count = 0


class RobotController(Node):
    """机器人控制器（发布 cmd_vel）"""
    
    def __init__(self, robot_name='robot01'):
        super().__init__('robot_controller')
        self.robot_name = robot_name
        
        # 创建 cmd_vel 发布者
        self.cmd_vel_publisher = self.create_publisher(
            Twist,
            f'/carla/{robot_name}/cmd_vel',
            10)
        
        self.get_logger().info(f'已创建控制发布者: cmd_vel')
        self.publish_count = 0
        self.publish_count_by_freq = defaultdict(int)
        
    def send_cmd_vel(self, angular_velocity=1.0, current_freq=None):
        """
        发送速度控制命令 (cmd_vel) - 固定角速度
        
        参数:
        - angular_velocity: 角速度 (rad/s，固定为10.0)
        - current_freq: 当前发布频率，用于统计
        """
        msg = Twist()
        msg.linear.x = 0.0
        msg.linear.y = 0.0
        msg.linear.z = 0.0
        msg.angular.x = 0.0
        msg.angular.y = 0.0
        msg.angular.z = float(angular_velocity)
        
        self.cmd_vel_publisher.publish(msg)
        self.publish_count += 1
        if current_freq is not None:
            self.publish_count_by_freq[current_freq] += 1


def test_rotation_frequency(robot_name='robot01', test_duration=60.0):
    """测试不同发布频率下的旋转速度控制"""
    rclpy.init()
    
    # 创建订阅者
    odom_sub = OdometrySubscriber(robot_name)
    imu_sub = IMUSubscriber(robot_name)
    
    # 创建控制器
    controller = RobotController(robot_name)
    
    # 创建执行器
    executor = rclpy.executors.MultiThreadedExecutor()
    executor.add_node(odom_sub)
    executor.add_node(imu_sub)
    executor.add_node(controller)
    
    # 固定角速度
    TARGET_ANGULAR_VELOCITY = 1  # rad/s
    
    # 频率列表
    FREQUENCIES = [1.0, 20.0]  # 1Hz 和 20Hz
    FREQ_SWITCH_INTERVAL = 3.0  # 3秒切换一次
    
    print(f'\n{"="*70}')
    print(f'测试 cmd_vel 发布频率对旋转速度的影响')
    print(f'{"="*70}')
    print(f'机器人名称: {robot_name}')
    print(f'目标角速度: {TARGET_ANGULAR_VELOCITY:.2f} rad/s ({math.degrees(TARGET_ANGULAR_VELOCITY):.1f}°/s)')
    print(f'发布频率: {FREQUENCIES[0]}Hz <-> {FREQUENCIES[1]}Hz (每{FREQ_SWITCH_INTERVAL}秒切换)')
    print(f'测试总时长: {test_duration} 秒')
    print(f'{"="*70}\n')
    
    # 等待话题连接
    print('等待话题连接...')
    time.sleep(2.0)
    
    # 初始化
    start_time = time.time()
    current_freq_index = 0
    current_freq = FREQUENCIES[current_freq_index]
    last_freq_switch_time = start_time
    last_publish_time = 0.0
    
    print(f'开始测试，初始频率: {current_freq}Hz\n')
    
    try:
        while rclpy.ok():
            current_time = time.time()
            elapsed = current_time - start_time
            
            if elapsed >= test_duration:
                break
            
            # 非阻塞执行ROS2回调
            executor.spin_once(timeout_sec=0.001)
            
            # 检查是否需要切换频率（每3秒切换一次）
            if current_time - last_freq_switch_time >= FREQ_SWITCH_INTERVAL:
                current_freq_index = (current_freq_index + 1) % len(FREQUENCIES)
                current_freq = FREQUENCIES[current_freq_index]
                last_freq_switch_time = current_time
                
                # 更新订阅者的当前频率
                odom_sub.set_current_freq(current_freq)
                imu_sub.set_current_freq(current_freq)
                
                print(f'[{elapsed:.1f}s] 切换到 {current_freq}Hz 发布频率')
            
            # 按当前频率发布命令
            publish_interval = 1.0 / current_freq
            if current_time - last_publish_time >= publish_interval:
                controller.send_cmd_vel(
                    angular_velocity=TARGET_ANGULAR_VELOCITY,
                    current_freq=current_freq
                )
                last_publish_time = current_time
            
            time.sleep(0.001)  # 避免CPU占用过高
            
    except KeyboardInterrupt:
        print('\n\n用户中断测试')
    
    # 等待一小段时间让数据稳定
    time.sleep(0.5)
    
    # 停止机器人
    print('\n停止机器人...')
    for _ in range(10):
        msg = Twist()
        msg.angular.z = 0.0
        controller.cmd_vel_publisher.publish(msg)
        executor.spin_once(timeout_sec=0.01)
    time.sleep(1.0)
    
    # 打印统计结果
    print(f'\n{"="*70}')
    print(f'测试结果统计')
    print(f'{"="*70}\n')
    
    for freq in FREQUENCIES:
        print(f'\n{freq}Hz 发布频率统计:')
        print(f'  发布命令数: {controller.publish_count_by_freq[freq]}')
        
        # Odometry 统计
        odom_stats = odom_sub.get_statistics(freq)
        if odom_stats:
            print(f'  Odometry 数据:')
            print(f'    消息数: {odom_stats["count"]}')
            print(f'    平均角速度: {odom_stats["mean"]:.3f} rad/s ({math.degrees(odom_stats["mean"]):.1f}°/s)')
            print(f'    角速度范围: [{odom_stats["min"]:.3f}, {odom_stats["max"]:.3f}] rad/s')
            print(f'    标准差: {odom_stats["std"]:.3f} rad/s')
            error = abs(odom_stats["mean"] - TARGET_ANGULAR_VELOCITY)
            error_percent = error / abs(TARGET_ANGULAR_VELOCITY) * 100
            print(f'    误差: {error:.3f} rad/s ({error_percent:.1f}%)')
        else:
            print(f'  Odometry 数据: 无数据')
        
        # IMU 统计
        imu_stats = imu_sub.get_statistics(freq)
        if imu_stats:
            print(f'  IMU 数据:')
            print(f'    消息数: {imu_stats["count"]}')
            print(f'    平均角速度: {imu_stats["mean"]:.3f} rad/s ({math.degrees(imu_stats["mean"]):.1f}°/s)')
            print(f'    角速度范围: [{imu_stats["min"]:.3f}, {imu_stats["max"]:.3f}] rad/s')
            print(f'    标准差: {imu_stats["std"]:.3f} rad/s')
            error = abs(imu_stats["mean"] - TARGET_ANGULAR_VELOCITY)
            error_percent = error / abs(TARGET_ANGULAR_VELOCITY) * 100
            print(f'    误差: {error:.3f} rad/s ({error_percent:.1f}%)')
        else:
            print(f'  IMU 数据: 无数据')
    
    # 对比结果
    print(f'\n{"="*70}')
    print(f'频率对比分析')
    print(f'{"="*70}\n')
    
    if len(FREQUENCIES) == 2:
        freq1, freq2 = FREQUENCIES[0], FREQUENCIES[1]
        
        odom_stats1 = odom_sub.get_statistics(freq1)
        odom_stats2 = odom_sub.get_statistics(freq2)
        imu_stats1 = imu_sub.get_statistics(freq1)
        imu_stats2 = imu_sub.get_statistics(freq2)
        
        print(f'发布频率对比:')
        print(f'  {freq1}Hz: {controller.publish_count_by_freq[freq1]} 次命令')
        print(f'  {freq2}Hz: {controller.publish_count_by_freq[freq2]} 次命令')
        if controller.publish_count_by_freq[freq1] > 0:
            ratio = controller.publish_count_by_freq[freq2] / controller.publish_count_by_freq[freq1]
            print(f'  比率: {ratio:.1f}x\n')
        
        if odom_stats1 and odom_stats2:
            print(f'Odometry 角速度对比:')
            print(f'  {freq1}Hz: {odom_stats1["mean"]:.3f} rad/s (误差: {abs(odom_stats1["mean"] - TARGET_ANGULAR_VELOCITY) / abs(TARGET_ANGULAR_VELOCITY) * 100:.1f}%)')
            print(f'  {freq2}Hz: {odom_stats2["mean"]:.3f} rad/s (误差: {abs(odom_stats2["mean"] - TARGET_ANGULAR_VELOCITY) / abs(TARGET_ANGULAR_VELOCITY) * 100:.1f}%)')
            diff = abs(odom_stats2["mean"] - odom_stats1["mean"])
            print(f'  差异: {diff:.3f} rad/s\n')
            
            print(f'Odometry 稳定性对比 (标准差):')
            print(f'  {freq1}Hz: {odom_stats1["std"]:.3f} rad/s')
            print(f'  {freq2}Hz: {odom_stats2["std"]:.3f} rad/s\n')
        
        if imu_stats1 and imu_stats2:
            print(f'IMU 角速度对比:')
            print(f'  {freq1}Hz: {imu_stats1["mean"]:.3f} rad/s (误差: {abs(imu_stats1["mean"] - TARGET_ANGULAR_VELOCITY) / abs(TARGET_ANGULAR_VELOCITY) * 100:.1f}%)')
            print(f'  {freq2}Hz: {imu_stats2["mean"]:.3f} rad/s (误差: {abs(imu_stats2["mean"] - TARGET_ANGULAR_VELOCITY) / abs(TARGET_ANGULAR_VELOCITY) * 100:.1f}%)')
            diff = abs(imu_stats2["mean"] - imu_stats1["mean"])
            print(f'  差异: {diff:.3f} rad/s\n')
            
            print(f'IMU 稳定性对比 (标准差):')
            print(f'  {freq1}Hz: {imu_stats1["std"]:.3f} rad/s')
            print(f'  {freq2}Hz: {imu_stats2["std"]:.3f} rad/s\n')
    
    print(f'{"="*70}\n')
    
    # 清理
    odom_sub.destroy_node()
    imu_sub.destroy_node()
    controller.destroy_node()
    rclpy.shutdown()


def main():
    parser = argparse.ArgumentParser(
        description='测试 cmd_vel 发布频率对旋转速度的影响',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 使用默认参数测试（60秒）
  python3 test_cmd_vel_frequency.py
  
  # 指定机器人名称和测试时长
  python3 test_cmd_vel_frequency.py --robot-name robot01 --test-duration 120
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
        default=60.0,
        help='测试总时长（秒，默认: 60.0）'
    )
    
    args = parser.parse_args()
    
    try:
        test_rotation_frequency(
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
