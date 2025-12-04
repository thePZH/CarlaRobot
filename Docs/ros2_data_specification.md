# CARLA ROS2 数据说明文档

本文档详细说明了 CARLA UE5 中通过 ROS2 Native Bridge 发布和订阅的数据格式、话题名称、消息结构以及值域范围。

## 目录

- [IMU 数据](#imu-数据)
- [Odometry 数据](#odometry-数据)
- [cmd_vel 控制命令](#cmd_vel-控制命令)
- [Lidar 点云数据](#lidar-点云数据)

---

## IMU 数据

### 话题信息

- **话题名称**: `/{parent}/{ros_name}`
  - 示例: `/robot01/imu/imu_data`
  - `parent`: 机器人的 `ros_name`（如 `robot01`）
  - `ros_name`: IMU 传感器的 `ros_name`（如 `imu/imu_data`）

- **消息类型**: `sensor_msgs::msg::Imu`
- **发布频率**: 与 UE 物理更新频率同步（通常 60 Hz）

### 消息结构

```cpp
std_msgs::msg::Header header
  - stamp: 时间戳 (sec + nanosec)
  - frame_id: 传感器坐标系名称（通常为传感器的 ros_name）

geometry_msgs::msg::Quaternion orientation
  - w, x, y, z: 四元数表示的姿态

geometry_msgs::msg::Vector3 angular_velocity
  - x, y, z: 角速度（单位：rad/s）

geometry_msgs::msg::Vector3 linear_acceleration
  - x, y, z: 线性加速度（单位：m/s²）
```

### 数据计算方式

#### 1. 线性加速度（Linear Acceleration）

- **计算方法**: 通过位置的三次采样进行二次插值的二阶导数计算
- **单位**: m/s²
- **特点**: 
  - 包含重力加速度（约 9.81 m/s²）
  - 静止时 Z 轴约为 9.81 m/s²
  - 数据在传感器本地坐标系中
- **值域**: 理论上无限制，取决于车辆运动状态
  - 典型范围: X/Y 轴约 -50 ~ +50 m/s²
  - Z 轴约 0 ~ 20 m/s²（静止时约 9.81）

#### 2. 角速度（Angular Velocity）

- **计算方法**: 从车辆物理引擎获取角速度，转换到传感器坐标系
- **单位**: rad/s
- **特点**: 
  - 数据在传感器本地坐标系中
  - 支持添加噪声和偏差（可通过参数配置）
- **值域**: 理论上无限制，取决于车辆旋转速度
  - 典型范围: 约 -10 ~ +10 rad/s

#### 3. 姿态（Orientation）

- **计算方法**: 从指北针（Compass）值计算四元数
- **坐标系**: 传感器本地坐标系（Sensor Local Frame）
- **特点**: 
  - Pitch 和 Roll 固定为 0
  - Yaw 从指北针计算得出，表示相对于北方的角度
  - 四元数满足归一化条件：w² + x² + y² + z² = 1
- **值域**: 
  - w, x, y, z ∈ [-1, 1]

**方向定义**：
- **坐标系**: IMU 四元数在**传感器本地坐标系**中表示
  - X 轴：前（Forward）- 传感器前向
  - Y 轴：左（Left）- 传感器左侧
  - Z 轴：上（Up）- 传感器上方
- **Yaw 角定义**：
  - Yaw 角表示传感器前向（X 轴）相对于**北方**的旋转角度
  - 计算公式：`yaw = (π/4) - compass`
  - Compass 值范围：[0, 2π)，表示从北方到传感器前向的夹角
- **CARLA 世界坐标系方向**：
  - **北方（North）**: Y 轴负方向 `(0, -1, 0)`
  - **南方（South）**: Y 轴正方向 `(0, 1, 0)`
  - **东方（East）**: X 轴正方向 `(1, 0, 0)`
  - **西方（West）**: X 轴负方向 `(-1, 0, 0)`
- **示例**：
  - 当传感器前向指向北方时：compass = 0，yaw = π/4
  - 当传感器前向指向东方时：compass = π/2，yaw = -π/4
  - 当传感器前向指向南方时：compass = π，yaw = -3π/4
  - 当传感器前向指向西方时：compass = 3π/2，yaw = -5π/4

### 坐标系

#### 传感器本地坐标系（IMU 四元数使用的坐标系）

- **X 轴**: 前（Forward）- 传感器前向方向
- **Y 轴**: 左（Left）- 传感器左侧方向
- **Z 轴**: 上（Up）- 传感器上方方向

#### CARLA 世界坐标系方向定义

在 CARLA 中，世界坐标系的方向定义如下（基于 OpenDRIVE 标准）：

- **北方（North）**: `(0, -1, 0)` - Y 轴负方向
- **南方（South）**: `(0, 1, 0)` - Y 轴正方向  
- **东方（East）**: `(1, 0, 0)` - X 轴正方向
- **西方（West）**: `(-1, 0, 0)` - X 轴负方向

**说明**：
- CARLA 使用右手坐标系：X(前), Y(右), Z(上)
- 北方定义为 Y 轴负方向，这是 OpenDRIVE 标准
- IMU 的 Compass 值表示从北方（Y轴负方向）到传感器前向的夹角
- IMU 四元数的 Yaw 角表示传感器前向相对于北方的旋转角度

### 使用示例

```python
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Imu

class IMUSubscriber(Node):
    def __init__(self):
        super().__init__('imu_subscriber')
        self.subscription = self.create_subscription(
            Imu,
            '/robot01/imu/imu_data',
            self.imu_callback,
            10
        )
    
    def imu_callback(self, msg):
        # 线性加速度（m/s²）
        accel = msg.linear_acceleration
        print(f"加速度: ({accel.x:.3f}, {accel.y:.3f}, {accel.z:.3f}) m/s²")
        
        # 角速度（rad/s）
        ang_vel = msg.angular_velocity
        print(f"角速度: ({ang_vel.x:.3f}, {ang_vel.y:.3f}, {ang_vel.z:.3f}) rad/s")
        
        # 姿态（四元数）
        orient = msg.orientation
        print(f"姿态: w={orient.w:.3f}, x={orient.x:.3f}, y={orient.y:.3f}, z={orient.z:.3f}")
```

---

## Odometry 数据

### 话题信息

- **话题名称**: `/{parent}/{ros_name}`
  - 示例: `/robot01/odom`
  - `parent`: 机器人的 `ros_name`（如 `robot01`）
  - `ros_name`: 默认为 `"odom"`

- **消息类型**: `nav_msgs::msg::Odometry`
- **发布频率**: 与 UE 物理更新频率同步（通常 60 Hz）

### 消息结构

```cpp
std_msgs::msg::Header header
  - stamp: 时间戳 (sec + nanosec)
  - frame_id: 上层坐标系（通常为 parent 的 ros_name）

string child_frame_id
  - 子坐标系名称（通常为 "odom"）

geometry_msgs::msg::PoseWithCovariance pose
  - pose.position: 位置 (x, y, z) - 单位：米
  - pose.orientation: 姿态四元数 (w, x, y, z)
  - covariance: 位姿协方差矩阵（当前未使用，全为0）

geometry_msgs::msg::TwistWithCovariance twist
  - twist.linear: 线速度 (x, y, z) - 单位：m/s
  - twist.angular: 角速度 (x, y, z) - 单位：rad/s
  - covariance: 速度协方差矩阵（当前未使用，全为0）
```

### 数据计算方式

#### 1. 位姿（Pose）

- **位置**: 从 UE 世界坐标系获取，Y 轴取反以匹配 ROS 坐标系
- **姿态**: 从 UE 欧拉角（roll, pitch, yaw，单位：度）转换为四元数
  - Roll/Pitch 符号翻转
  - 单位从度转换为弧度
- **坐标系转换**: 
  - CARLA: X(前), Y(右), Z(上)
  - ROS: X(前), Y(左), Z(上)
  - Y 轴取反

#### 2. 速度（Twist）

- **线速度**: 从车辆物理引擎获取，世界坐标系中的速度向量
- **角速度**: 从车辆物理引擎获取，世界坐标系中的角速度向量
- **单位**: 
  - 线速度: m/s
  - 角速度: rad/s

### 值域

| 字段 | 单位 | 典型值域 | 说明 |
|------|------|----------|------|
| `pose.position.x/y` | m | 无限制 | 取决于地图大小 |
| `pose.position.z` | m | 通常 > 0 | 车辆高度 |
| `pose.orientation` | 四元数 | w,x,y,z ∈ [-1,1] | 归一化四元数 |
| `twist.linear.x/y/z` | m/s | 无限制 | 取决于车辆速度 |
| `twist.angular.x/y/z` | rad/s | 无限制 | 取决于车辆旋转 |

### 使用示例

```python
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
import math

class OdometrySubscriber(Node):
    def __init__(self):
        super().__init__('odometry_subscriber')
        self.subscription = self.create_subscription(
            Odometry,
            '/robot01/odom',
            self.odom_callback,
            10
        )
    
    def odom_callback(self, msg):
        # 位置
        pos = msg.pose.pose.position
        print(f"位置: ({pos.x:.2f}, {pos.y:.2f}, {pos.z:.2f}) m")
        
        # 姿态（四元数转欧拉角）
        orient = msg.pose.pose.orientation
        yaw = math.atan2(2*(orient.w*orient.z + orient.x*orient.y),
                         1 - 2*(orient.y*orient.y + orient.z*orient.z))
        print(f"Yaw: {math.degrees(yaw):.1f}°")
        
        # 线速度
        linear = msg.twist.twist.linear
        print(f"线速度: ({linear.x:.3f}, {linear.y:.3f}, {linear.z:.3f}) m/s")
        
        # 角速度
        angular = msg.twist.twist.angular
        print(f"角速度: ({angular.x:.3f}, {angular.y:.3f}, {angular.z:.3f}) rad/s")
```

---

## cmd_vel 控制命令

### 话题信息

- **话题名称**: `/{parent}/{name}/cmd_vel`
  - 示例: `/robot01/cmd_vel`
  - `parent`: 机器人的 `ros_name`（如 `robot01`）
  - `name`: 机器人的 `ros_name`（如 `robot01`）

- **消息类型**: `geometry_msgs::msg::Twist`
- **订阅方式**: CARLA 订阅此话题以控制机器人运动

### 消息结构

```cpp
geometry_msgs::msg::Vector3 linear
  - x: 线速度（单位：m/s）
  - y: 未使用（应设为 0）
  - z: 未使用（应设为 0）

geometry_msgs::msg::Vector3 angular
  - x: 未使用（应设为 0）
  - y: 未使用（应设为 0）
  - z: 角速度（单位：rad/s）
```

### 控制方式

- **线速度控制**: 
  - 使用 `linear.x` 字段
  - 单位：**m/s**
  - 值域：**无限制**（直接控制速度）
  - 正值为前进，负值理论上支持但当前实现中不支持倒车

- **角速度控制**: 
  - 使用 `angular.z` 字段
  - 单位：**rad/s**
  - 值域：**无限制**（直接控制角速度）
  - 正值逆时针旋转，负值顺时针旋转

### 实现细节

在 CARLA 内部，`cmd_vel` 消息被转换为 `VehicleControl` 结构：

```cpp
VehicleControl control;
control.throttle = linear.x;  // 存储线速度 (m/s)
control.steer = angular.z;    // 存储角速度 (rad/s)
control.brake = 0.0f;
control.hand_brake = (linear.x <= 0.01f) ? true : false;
control.reverse = false;      // 不支持倒车
```

然后通过 `SetActorTargetVelocity` 和 `SetActorTargetAngularVelocity` 直接设置车辆的目标速度和角速度。

### 使用示例

```python
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist

class RobotController(Node):
    def __init__(self):
        super().__init__('robot_controller')
        self.publisher = self.create_publisher(
            Twist,
            '/robot01/cmd_vel',
            10
        )
    
    def send_cmd_vel(self, linear_velocity, angular_velocity):
        """
        发送速度控制命令
        
        Args:
            linear_velocity: 线速度 (m/s)
            angular_velocity: 角速度 (rad/s)
        """
        msg = Twist()
        msg.linear.x = float(linear_velocity)
        msg.linear.y = 0.0
        msg.linear.z = 0.0
        msg.angular.x = 0.0
        msg.angular.y = 0.0
        msg.angular.z = float(angular_velocity)
        
        self.publisher.publish(msg)
        self.get_logger().info(
            f'发布速度命令: 线速度={linear_velocity:.3f} m/s, '
            f'角速度={angular_velocity:.3f} rad/s'
        )

# 使用示例
controller = RobotController()
controller.send_cmd_vel(linear_velocity=1.0, angular_velocity=0.5)  # 前进并左转
controller.send_cmd_vel(linear_velocity=0.0, angular_velocity=1.0)   # 原地旋转
controller.send_cmd_vel(linear_velocity=0.0, angular_velocity=0.0)   # 停止
```

### 注意事项

1. **持续发送**: 需要持续发送命令以维持运动，停止发送会导致车辆停止
2. **倒车**: 当前实现不支持倒车（`reverse = false`）
3. **坐标系**: 
   - 线速度沿车辆前向（X 轴）
   - 角速度绕车辆垂直轴（Z 轴）
4. **原地旋转**: 设置 `linear.x = 0.0` 且 `angular.z ≠ 0.0` 可实现原地旋转

---

## Lidar 点云数据

### 话题信息

- **话题名称**: `/{parent}/{ros_name}`
  - 示例: `/robot01/sensor/lidar/points`
  - `parent`: 机器人的 `ros_name`（如 `robot01`）
  - `ros_name`: Lidar 传感器的 `ros_name`（如 `sensor/lidar/points`）

- **消息类型**: `sensor_msgs::msg::PointCloud2`
- **发布频率**: 取决于 Lidar 传感器配置（通常 10-20 Hz）

### 消息结构

```cpp
std_msgs::msg::Header header
  - stamp: 时间戳 (sec + nanosec)
  - frame_id: 传感器坐标系名称（通常为传感器的 ros_name）

uint32 height
  - 点云高度（通常为 1，表示无序点云）

uint32 width
  - 点云宽度（点的数量）

sensor_msgs::msg::PointField[] fields
  - 点字段描述符数组

uint32 point_step
  - 每个点的字节数

uint32 row_step
  - 每行的字节数

bool is_bigendian
  - 字节序（通常为 false，小端）

bool is_dense
  - 是否为密集点云（通常为 false）

uint8[] data
  - 点云数据（二进制格式）
```

### 点字段格式

Lidar 点云支持两种格式：

#### 格式 1: 基础格式（16 字节/点）

```
字段名     类型      偏移量    说明
x          FLOAT32   0         点的 X 坐标（米）
y          FLOAT32   4         点的 Y 坐标（米）
z          FLOAT32   8         点的 Z 坐标（米）
intensity  FLOAT32   12        强度值
```

#### 格式 2: 扩展格式（22 字节/点）

```
字段名     类型      偏移量    说明
x          FLOAT32   0         点的 X 坐标（米）
y          FLOAT32   4         点的 Y 坐标（米）
z          FLOAT32   8         点的 Z 坐标（米）
intensity  FLOAT32   12        强度值
ring       UINT16    16        激光通道编号
padding    UINT16    18        填充（对齐用）
time       FLOAT32   20        相对于扫描开始的时间（秒）
```

### 坐标系转换

- **CARLA → ROS**: Y 轴取反
  - CARLA: X(前), Y(右), Z(上)
  - ROS: X(前), Y(左), Z(上)
  - 转换: `y_ros = -y_carla`

### 值域

| 字段 | 类型 | 单位 | 值域 | 说明 |
|------|------|------|------|------|
| `x, y, z` | float32 | m | 取决于 Lidar 范围 | 点坐标 |
| `intensity` | float32 | 无 | 0.0 ~ 1.0 | 反射强度 |
| `ring` | uint16 | 无 | 0 ~ N-1 | 激光通道编号（N 为通道数） |
| `time` | float32 | s | 0.0 ~ rotation_time | 相对于扫描开始的时间 |

### 使用示例

```python
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import PointCloud2
import struct

class LidarSubscriber(Node):
    def __init__(self):
        super().__init__('lidar_subscriber')
        self.subscription = self.create_subscription(
            PointCloud2,
            '/robot01/sensor/lidar/points',
            self.lidar_callback,
            10
        )
    
    def lidar_callback(self, msg):
        # 检查点字段格式
        fields = {field.name: field for field in msg.fields}
        
        # 解析点云数据
        point_count = msg.width
        point_step = msg.point_step
        
        print(f"收到点云: {point_count} 个点")
        
        # 读取前几个点作为示例
        for i in range(min(5, point_count)):
            offset = i * point_step
            x = struct.unpack_from('f', msg.data, offset + fields['x'].offset)[0]
            y = struct.unpack_from('f', msg.data, offset + fields['y'].offset)[0]
            z = struct.unpack_from('f', msg.data, offset + fields['z'].offset)[0]
            intensity = struct.unpack_from('f', msg.data, offset + fields['intensity'].offset)[0]
            
            print(f"点 {i}: ({x:.3f}, {y:.3f}, {z:.3f}), 强度={intensity:.3f}")
```

### 使用 PCL 库处理点云（推荐）

```python
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import PointCloud2
from sensor_msgs_py import point_cloud2

class LidarSubscriber(Node):
    def __init__(self):
        super().__init__('lidar_subscriber')
        self.subscription = self.create_subscription(
            PointCloud2,
            '/robot01/sensor/lidar/points',
            self.lidar_callback,
            10
        )
    
    def lidar_callback(self, msg):
        # 使用 sensor_msgs_py 解析点云
        points = list(point_cloud2.read_points(
            msg, field_names=("x", "y", "z", "intensity"), skip_nans=True
        ))
        
        print(f"收到点云: {len(points)} 个点")
        
        # 处理点云数据
        for point in points[:5]:  # 显示前5个点
            x, y, z, intensity = point
            print(f"点: ({x:.3f}, {y:.3f}, {z:.3f}), 强度={intensity:.3f}")
```

---

## 话题命名规则

所有话题的命名遵循以下规则：

```
话题名称 = base + parent + "/" + name + suffix
```

其中：
- `base`: 基础前缀（当前为空字符串，之前为 `"rt/carla/"`）
- `parent`: 父级 `ros_name`（通常是机器人的 `ros_name`）
- `name`: 传感器或控制器的 `ros_name`
- `suffix`: 特定后缀（如 `/cmd_vel`）

### 示例话题名称

| 类型 | 话题名称示例 | 说明 |
|------|-------------|------|
| IMU | `/robot01/imu/imu_data` | `parent=robot01`, `name=imu/imu_data` |
| Odometry | `/robot01/odom` | `parent=robot01`, `name=odom` |
| cmd_vel | `/robot01/cmd_vel` | `parent=robot01`, `name=robot01`, `suffix=/cmd_vel` |
| Lidar | `/robot01/sensor/lidar/points` | `parent=robot01`, `name=sensor/lidar/points` |

---

## 坐标系说明

### CARLA 世界坐标系

- **X 轴**: 前（Forward）
- **Y 轴**: 右（Right）
- **Z 轴**: 上（Up）

**方向定义**（基于 OpenDRIVE 标准）：
- **北方（North）**: Y 轴负方向 `(0, -1, 0)`
- **南方（South）**: Y 轴正方向 `(0, 1, 0)`
- **东方（East）**: X 轴正方向 `(1, 0, 0)`
- **西方（West）**: X 轴负方向 `(-1, 0, 0)`

### ROS 坐标系

- **X 轴**: 前（Forward）
- **Y 轴**: 左（Left）
- **Z 轴**: 上（Up）

### 转换规则

从 CARLA 到 ROS 的坐标系转换：
- X 轴保持不变
- Y 轴取反：`y_ros = -y_carla`
- Z 轴保持不变

**方向转换**：
- CARLA 北方 `(0, -1, 0)` → ROS 北方 `(0, 1, 0)`（Y 轴取反后）
- CARLA 南方 `(0, 1, 0)` → ROS 南方 `(0, -1, 0)`
- 东方和西方保持不变（X 轴方向）

---

## 常见问题

### 1. 为什么 IMU 的 Z 轴加速度不是 0？

IMU 的线性加速度包含重力加速度。静止时，Z 轴约为 9.81 m/s²（重力加速度）。

### 2. cmd_vel 控制不生效？

- 检查话题名称是否正确
- 确保持续发送命令（停止发送会导致车辆停止）
- 检查机器人是否已正确创建并设置了 `ros_name`

### 3. Odometry 和 IMU 的角速度不一致？

这是正常的，因为：
- Odometry 的角速度来自车辆物理引擎，是世界坐标系
- IMU 的角速度经过传感器坐标系转换，可能包含传感器安装角度的影响

### 4. Lidar 点云数据格式如何选择？

- 基础格式（16 字节/点）：适用于不需要通道和时间信息的场景
- 扩展格式（22 字节/点）：适用于需要通道编号和时间戳的场景（如 SLAM）

### 5. IMU 四元数的 Yaw 角如何理解？

IMU 四元数的 Yaw 角表示传感器前向相对于**北方**的旋转角度：
- **北方**: CARLA 世界坐标系中 Y 轴负方向 `(0, -1, 0)`
- **计算公式**: `yaw = (π/4) - compass`，其中 compass 是从北方到传感器前向的夹角
- **示例**：
  - 传感器指向北方：compass = 0，yaw = π/4
  - 传感器指向东方：compass = π/2，yaw = -π/4
  - 传感器指向南方：compass = π，yaw = -3π/4
  - 传感器指向西方：compass = 3π/2，yaw = -5π/4

### 6. IMU 四元数在哪个坐标系下？

IMU 四元数在**传感器本地坐标系**中表示：
- X 轴：传感器前向
- Y 轴：传感器左侧
- Z 轴：传感器上方
- Yaw 角表示传感器前向相对于北方的角度

---

## 参考资源

- [CARLA 官方文档](https://carla-ue5.readthedocs.io/)
- [ROS2 消息类型文档](https://docs.ros2.org/)
- [sensor_msgs::msg::Imu](http://docs.ros.org/en/api/sensor_msgs/html/msg/Imu.html)
- [nav_msgs::msg::Odometry](http://docs.ros.org/en/api/nav_msgs/html/msg/Odometry.html)
- [geometry_msgs::msg::Twist](http://docs.ros.org/en/api/geometry_msgs/html/msg/Twist.html)
- [sensor_msgs::msg::PointCloud2](http://docs.ros.org/en/api/sensor_msgs/html/msg/PointCloud2.html)

---

**文档版本**: 1.0  
**最后更新**: 2024年

