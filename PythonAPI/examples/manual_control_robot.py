"""

    W            : 油门
    S            : 刹车
    A/D          : 左右旋转

    Q            : 切换前进/倒档
    Space        : 手刹
    
    LEFT/RIGHT   : 云台旋转
    UP/DOWN      : 相机旋转
    R            : 重置云台、相机旋转
    T            ：重置相机倍率
    N            : 可巡检区域点云
    G            : 获取表计坐标
    
    I            : 放大
    O            ：缩小
    
    ` or N       : next sensor
    [1-9]        : change to sensor [1-9]

    F1           : toggle HUD
    H/?          : toggle help
    ESC          : quit
    

"""

# ==============================================================================
# -- imports -------------------------------------------------------------------
# ==============================================================================

import carla

from carla import ColorConverter as cc

import argparse
import collections
import datetime
import logging
import math
import random
import re
import os
import weakref
import matplotlib.pyplot as plt
import numpy as np
import json
import sys

# --- 提前插入工具函数 ---


try:
    import pygame
    from pygame.locals import KMOD_CTRL
    from pygame.locals import KMOD_SHIFT
    from pygame.locals import K_DOWN
    from pygame.locals import K_ESCAPE
    from pygame.locals import K_F1
    from pygame.locals import K_F2
    from pygame.locals import K_F5
    from pygame.locals import K_F6
    from pygame.locals import K_F7
    from pygame.locals import K_LEFT
    from pygame.locals import K_RIGHT
    from pygame.locals import K_SPACE
    from pygame.locals import K_UP
    from pygame.locals import K_a
    from pygame.locals import K_d
    from pygame.locals import K_s
    from pygame.locals import K_w
    from pygame.locals import K_c
    from pygame.locals import K_e
    
    from pygame.locals import K_r
    from pygame.locals import K_q
    from pygame.locals import K_f
    from pygame.locals import K_g
    from pygame.locals import K_h
    from pygame.locals import K_n
    from pygame.locals import K_y
    from pygame.locals import K_u
    from pygame.locals import K_i
    from pygame.locals import K_o
    from pygame.locals import K_t
    from pygame.locals import K_v
    
    from pygame.locals import K_t
    from pygame.locals import K_g
    from pygame.locals import K_b
    from pygame.locals import K_EQUALS

except ImportError:
    raise RuntimeError('cannot import pygame, make sure pygame package is installed')

try:
    import numpy as np
except ImportError:
    raise RuntimeError('cannot import numpy, make sure numpy package is installed')

OBJECT_TO_COLOR = [
    (255, 255, 255),
    (128, 64, 128),
    (244, 35, 232),
    (70, 70, 70),
    (102, 102, 156),
    (190, 153, 153),
    (153, 153, 153),
    (250, 170, 30),
    (220, 220, 0),
    (107, 142,  35),
    (152, 251, 152),
    (70, 130, 180),
    (220, 20, 60),
    (255, 0, 0),
    (0, 0, 142),
    (0, 0, 70),
    (0,  60, 100),
    (0,  80, 100),
    (0, 0, 230),
    (119, 11, 32),
    (110, 190, 160),
    (170, 120, 50),
    (55, 90, 80),
    (45, 60, 150),
    (157, 234, 50),
    (81, 0, 81),
    (150, 100, 100),
    (230, 150, 140),
    (180, 165, 180),
]

# ==============================================================================
# -- Global functions ----------------------------------------------------------
# ==============================================================================
def get_actor_display_name(actor, truncate=250):
    name = ' '.join(actor.type_id.replace('_', '.').title().split('.')[1:])
    return (name[:truncate - 1] + u'\u2026') if len(name) > truncate else name

def get_actor_blueprints(world, filter, generation):
    bps = world.get_blueprint_library().filter(filter)

    if generation.lower() == "all":
        return bps

    # If the filter returns only one bp, we assume that this one needed
    # and therefore, we ignore the generation
    if len(bps) == 1:
        return bps

    try:
        int_generation = int(generation)
        # Check if generation is in available generations
        if int_generation in [1, 2, 3, 4]:
            bps = [x for x in bps if int(x.get_attribute('generation')) == int_generation]
            return bps
        else:
            print("   Warning! Actor Generation is not valid. No actor will be spawned.")
            return []
    except:
        print("   Warning! Actor Generation is not valid. No actor will be spawned.")
        return []


# ==============================================================================
# -- World ---------------------------------------------------------------------
# ==============================================================================
class World(object):
    def __init__(self, carla_world, hud, traffic_manager, args):
        self.world = carla_world
        self.sync = args.sync
        self.traffic_manager = traffic_manager
        self.actor_role_name = args.rolename
        try:
            self.map = self.world.get_map()
        except RuntimeError as error:
            print('RuntimeError: {}'.format(error))
            print('  The server could not send the OpenDRIVE (.xodr) file:')
            print('  Make sure it exists, has the same name of your town, and is correct.')
            sys.exit(1)
        self.hud = hud
        self.player = None
        self.collision_sensor = None
        self.gnss_sensor = None
        self.imu_sensor = None
        self.camera_manager = None
        self._actor_filter = args.filter
        self._actor_generation = args.generation
        self._gamma = args.gamma
        # 记录机器人ID，用于destroy_robot
        self._robot_id = None
        self.restart()
        self.world.on_tick(hud.on_world_tick)
        self.show_vehicle_telemetry = False

    def restart(self):
        self.player_max_speed = 1.589
        self.player_max_speed_fast = 3.713
        # Keep same camera config if the camera manager exists.
        cam_index = self.camera_manager.index if self.camera_manager is not None else 0
        cam_pos_index = self.camera_manager.transform_index if self.camera_manager is not None else 0
        
        # 如果已有机器人，先停止监听，再销毁服务器端，最后清理Python引用
        if self._robot_id is not None:
            # 先停止正在监听的传感器（只停止当前激活的传感器）
            # 注意：只停止正在监听的传感器，避免对未监听的传感器调用 stop() 产生警告
            try:
                if self.collision_sensor is not None and self.collision_sensor.sensor is not None:
                    if hasattr(self.collision_sensor.sensor, 'is_listening') and self.collision_sensor.sensor.is_listening:
                        self.collision_sensor.sensor.stop()
            except Exception:
                pass
            try:
                if self.gnss_sensor is not None and self.gnss_sensor.sensor is not None:
                    if hasattr(self.gnss_sensor.sensor, 'is_listening') and self.gnss_sensor.sensor.is_listening:
                        self.gnss_sensor.sensor.stop()
            except Exception:
                pass
            try:
                if self.imu_sensor is not None and self.imu_sensor.sensor is not None:
                    if hasattr(self.imu_sensor.sensor, 'is_listening') and self.imu_sensor.sensor.is_listening:
                        self.imu_sensor.sensor.stop()
            except Exception:
                pass
            try:
                if self.camera_manager is not None and self.camera_manager.sensor is not None:
                    if hasattr(self.camera_manager.sensor, 'is_listening') and self.camera_manager.sensor.is_listening:
                        self.camera_manager.sensor.stop()
            except Exception:
                pass
            
            # 先销毁服务器端的机器人（这会自动停止并销毁所有传感器）
            try:
                self.world.destroy_robot(self._robot_id)
                # 等待一个tick，确保服务器端的传感器已经被完全销毁
                if self.sync:
                    self.world.tick()
                else:
                    self.world.wait_for_tick()
            except Exception as e:
                print(f"Failed to destroy existing robot: {e}")
            self._robot_id = None
            
            # 服务器端已销毁，现在可以安全地清理Python引用
            # 此时即使Python对象被清理，C++ 析构函数检查 IsAlive() 时已返回 false，不会触发警告
            self._cleanup_sensors()
            self.player = None
        
        # 使用 create_robot 创建机器人和传感器
        spawn_point = carla.Transform(carla.Location(x=5.0, y=2.0, z=1))
        json_params = {
            "robot": {
                "blueprint": "vehicle.robot.01",
                "attributes": {
                    "role_name": self.actor_role_name,
                    "ros_name": "robot01"
                },
                "transform": {
                    "location": {"x": spawn_point.location.x, "y": spawn_point.location.y, "z": spawn_point.location.z},
                    "rotation": {"pitch": spawn_point.rotation.pitch, "yaw": spawn_point.rotation.yaw, "roll": spawn_point.rotation.roll}
                }
            },
            "sensors": [
                {
                    "name": "FrontRGB",
                    "blueprint": "sensor.camera.rgb",
                    "attributes": {
                        "image_size_x": self.hud.dim[0],
                        "image_size_y": self.hud.dim[1],
                        "gamma": str(self._gamma)
                    }
                },
                {
                    "name": "LidarRayCast",
                    "blueprint": "sensor.lidar.ray_cast",
                    "attributes": {
                        "range": "200",
                        "upper_fov": "15.0",
                        "lower_fov": "-15.0",
                        "horizontal_fov": "180",
                        "ros_name": "sensor/lidar/points"
                    }
                },
                {
                    "name": "IMUSensor",
                    "blueprint": "sensor.other.imu",
                    "attributes": {
                        "ros_name": "imu/imu_data"
                    }
                }
            ]
        }
        
        json_str = self.world.create_robot(json.dumps(json_params))
        try:
            result = json.loads(json_str)
        except Exception as e:
            print(f"Failed to parse create_robot result: {e}")
            raise ValueError(f"Failed to create robot: {json_str}")
        
        if not result.get("ok"):
            error_msg = result.get("error", "Unknown error")
            raise ValueError(f"Failed to create robot: {error_msg}")
        
        # 获取机器人ID和车辆actor
        robot_id = int(result.get("robot_id", 0))
        if not robot_id:
            raise ValueError("Failed to get robot_id from create_robot result")
        
        self._robot_id = robot_id
        self.player = self.world.get_actor(robot_id)
        if self.player is None:
            raise ValueError("Failed to get player actor from robot_id")
        
        self.show_vehicle_telemetry = False
        self.modify_vehicle_physics(self.player)
        
        # 从返回的传感器列表中获取传感器actor
        sensor_dict = {}
        for s in result.get("sensors", []):
            sensor_name = s.get("name", "")
            sensor_id = s.get("id")
            if sensor_id is not None:
                sensor_dict[sensor_name] = int(sensor_id)
        
        # 创建传感器包装类（从已存在的actor获取数据）
        camera_sensors = []
        if "FrontRGB" in sensor_dict:
            rgb_actor = self.world.get_actor(sensor_dict["FrontRGB"])
            if rgb_actor is not None:
                camera_sensors.append(rgb_actor)
        if "FrontDepthRaw" in sensor_dict:
            depth_actor = self.world.get_actor(sensor_dict["FrontDepthRaw"])
            if depth_actor is not None:
                camera_sensors.append(depth_actor)
        if "LidarRayCast" in sensor_dict:
            lidar_actor = self.world.get_actor(sensor_dict["LidarRayCast"])
            if lidar_actor is not None:
                camera_sensors.append(lidar_actor)
        
        # 确保至少有一个相机传感器
        if len(camera_sensors) == 0:
            raise ValueError("No camera sensors found in create_robot result")
        
        # 初始化传感器包装（所有传感器都是可选的）
        if "CollisionSensor" in sensor_dict:
            collision_actor = self.world.get_actor(sensor_dict["CollisionSensor"])
            if collision_actor is not None:
                self.collision_sensor = CollisionSensorWrapper(collision_actor, self.hud)
            else:
                print("Warning: CollisionSensor actor is None")
                self.collision_sensor = None
        else:
            print("Info: CollisionSensor not found in create_robot result, skipping")
            self.collision_sensor = None
        
        if "GnssSensor" in sensor_dict:
            gnss_actor = self.world.get_actor(sensor_dict["GnssSensor"])
            if gnss_actor is not None:
                self.gnss_sensor = GnssSensorWrapper(gnss_actor)
            else:
                print("Warning: GnssSensor actor is None")
                self.gnss_sensor = None
        else:
            print("Info: GnssSensor not found in create_robot result, skipping")
            self.gnss_sensor = None
        
        if "IMUSensor" in sensor_dict:
            imu_actor = self.world.get_actor(sensor_dict["IMUSensor"])
            if imu_actor is not None:
                self.imu_sensor = IMUSensorWrapper(imu_actor)
            else:
                print("Warning: IMUSensor actor is None")
                self.imu_sensor = None
        else:
            print("Info: IMUSensor not found in create_robot result, skipping")
            self.imu_sensor = None
        
        # 初始化CameraManager并设置为外部传感器模式
        if self.camera_manager is None:
            self.camera_manager = CameraManager(self.player, self.hud, self._gamma)
        else:
            self.camera_manager._parent = self.player
        self.camera_manager.transform_index = cam_pos_index
        self.camera_manager.set_external_sensors(camera_sensors)
        
        actor_type = get_actor_display_name(self.player)
        self.hud.notification(actor_type)

        if self.sync:
            self.world.tick()
        else:
            self.world.wait_for_tick()
        
    def modify_vehicle_physics(self, actor):
        #If actor is not a vehicle, we cannot use the physics control
        try:
            physics_control = actor.get_physics_control()
            physics_control.use_sweep_wheel_collision = True
            actor.apply_physics_control(physics_control)
        except Exception:
            pass

    def tick(self, clock):
        self.hud.tick(self, clock)

    def render(self, display):
        if self.camera_manager is not None:
            self.camera_manager.render(display)
        self.hud.render(display)

    def _cleanup_sensors(self):
        # 清理传感器引用（只清理Python引用，不调用stop）
        # 注意：这个方法应该在 destroy_robot() 之后调用，此时服务器端的传感器已经被销毁
        # 所以只需要清理Python引用即可，不需要再停止传感器
        # 当Python对象被垃圾回收时，C++ 析构函数检查 IsAlive() 时会返回 false，不会触发警告
        self.collision_sensor = None
        self.gnss_sensor = None
        self.imu_sensor = None
        if self.camera_manager is not None:
            self.camera_manager.sensor = None
            # 直接清空 external_sensors 列表
            # 如果服务器端的传感器已经被 destroy_robot() 销毁，IsAlive() 应该返回 false
            # 这样即使C++析构函数被调用，也不会触发警告（因为 IsAlive() 为 false）
            self.camera_manager.external_sensors = []
            self.camera_manager.use_external = False

    def destroy(self):
        # 先停止正在监听的传感器，再销毁服务器端，最后清理Python引用
        if self._robot_id is not None:
            # 先停止正在监听的传感器（只停止当前激活的传感器）
            try:
                if self.collision_sensor is not None and self.collision_sensor.sensor is not None:
                    if hasattr(self.collision_sensor.sensor, 'is_listening') and self.collision_sensor.sensor.is_listening:
                        self.collision_sensor.sensor.stop()
            except Exception:
                pass
            try:
                if self.gnss_sensor is not None and self.gnss_sensor.sensor is not None:
                    if hasattr(self.gnss_sensor.sensor, 'is_listening') and self.gnss_sensor.sensor.is_listening:
                        self.gnss_sensor.sensor.stop()
            except Exception:
                pass
            try:
                if self.imu_sensor is not None and self.imu_sensor.sensor is not None:
                    if hasattr(self.imu_sensor.sensor, 'is_listening') and self.imu_sensor.sensor.is_listening:
                        self.imu_sensor.sensor.stop()
            except Exception:
                pass
            try:
                if self.camera_manager is not None and self.camera_manager.sensor is not None:
                    if hasattr(self.camera_manager.sensor, 'is_listening') and self.camera_manager.sensor.is_listening:
                        self.camera_manager.sensor.stop()
            except Exception:
                pass
            
            # 先销毁服务器端的机器人（这会自动停止并销毁所有传感器）
            try:
                self.world.destroy_robot(self._robot_id)
                # 等待一个tick，确保服务器端的传感器已经被完全销毁
                if self.sync:
                    self.world.tick()
                else:
                    self.world.wait_for_tick()
            except Exception as e:
                print(f"Failed to destroy robot: {e}")
            self._robot_id = None
            
            # 服务器端已销毁，现在可以安全地清理Python引用
            # 此时即使Python对象被清理，C++ 析构函数检查 IsAlive() 时已返回 false，不会触发警告
            self._cleanup_sensors()
            self.player = None


# ==============================================================================
# -- KeyboardControl -----------------------------------------------------------
# ==============================================================================
class KeyboardControl(object):
    """Class that handles keyboard input."""
    def __init__(self, world):
        self._world = world
        self.uuids = []
        if isinstance(world.player, carla.Vehicle):
            self._control = carla.VehicleControl()
            self._lights = carla.VehicleLightState.NONE
            world.player.set_light_state(self._lights)
        elif isinstance(world.player, carla.Walker):
            self._control = carla.WalkerControl()
            self._rotation = world.player.get_transform().rotation
        else:
            raise NotImplementedError("Actor type not supported")
        self._steer_cache = 0.0
        self.current_fov = 90
        # 初始化骨骼旋转状态
        self._bone_rotations = {}
        # 缓存初始骨骼信息，只保存 relative
        self._bones_cache = {}
        try:
            bone_control_out = world.player.get_bones_transform()
            for bone in bone_control_out.bones_transform:
                self._bones_cache[bone.name] = bone.relative
                # 初始化骨骼旋转状态
                self._bone_rotations[bone.name] = bone.relative.rotation
        except Exception as e:
            print(f"Failed to get bones transform: {e}")
            # 设置默认值
            self._bone_rotations["Camera"] = carla.Rotation()
            self._bone_rotations["Gimbal"] = carla.Rotation()

        world.hud.notification("Press 'H' or '?' for help.", seconds=4.0)

    def parse_events(self, client, world, clock, sync_mode):
        if isinstance(self._control, carla.VehicleControl):
            current_lights = self._lights
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return True
            elif event.type == pygame.KEYUP:
                if self._is_quit_shortcut(event.key):
                    return True
                elif event.key == K_F1:
                    # world.hud.toggle_info()
                    try:
                        # 点
                        p_json = {
                            "location": {"x": 0.0, "y": 0.0, "z": 2.0},
                            "color": {"r": 1.0, "g": 0.0, "b": 0.0, "a": 1.0},
                            "scale": 1
                        }
                        resp = world.world.draw_point(json.dumps(p_json))
                        print("draw_point resp:", resp)
                        try:
                            pid = json.loads(resp).get("id")
                            if pid:
                                self.uuids.append(pid)
                        except Exception as e:
                            print("parse point id failed", e, ", resp:", resp)
                        # 线
                        l_json = {
                            "locations": [
                                {"x": -1.0, "y": -1.0, "z": 1.0},
                                {"x": 1.0, "y": 1.0, "z": 1.0}
                            ],
                            "color": {"r": 0.0, "g": 1.0, "b": 0.0, "a": 1.0},
                            "scale": 1
                        }
                        resp = world.world.draw_line(json.dumps(l_json))
                        print("draw_line resp:", resp)
                        try:
                            lid = json.loads(resp).get("id")
                            if lid:
                                self.uuids.append(lid)
                        except Exception as e:
                            print("parse line id failed", e, ", resp:", resp)
                        # 立方体
                        c_json = {
                            "location": {"x": 1.0, "y": 0.0, "z": 0.5},
                            "scale": {"x": 1, "y": 1, "z": 1},
                            "color": {"r": 1, "g": 0, "b": 0, "a": 1.0}
                        }
                        resp = world.world.draw_cube(json.dumps(c_json))
                        print("draw_cube resp:", resp)
                        try:
                            cid = json.loads(resp).get("id")
                            if cid:
                                self.uuids.append(cid)
                        except Exception as e:
                            print("parse cube id failed", e, ", resp:", resp)

                        print("[F1] 绘制后缓存的uuid:", self.uuids)
                    except Exception as e:
                        print("draw geometry failed:", e)
                elif event.key == K_F2:
                    print("[F2] 当前将要删除uuids:", self.uuids)
                    # try:
                    #     for uid in list(self.uuids):
                    #         resp = world.world.remove_draw_object(json.dumps({"id": uid}))
                    #         print("remove resp:", resp)
                    #     self.uuids = []
                    #     print("Cleared all drawn geometries")
                    # except Exception as e:
                    #     print("clear geometry failed:", e)
                    
                    world.world.clear_draw_objects(json.dumps({"type": "all"}))
                elif event.key == K_h:
                    path = '/mnt/ssd1t/3DGSData/nmh/LCC_Results/nmh_01.lcc'
                    world.world.load_map(path)
                    
                elif event.key == K_n:
                    if world.camera_manager is not None:
                        world.camera_manager.next_sensor()
                elif event.key == K_q:
                    if world.camera_manager is None or world.camera_manager.sensor is None:
                        print("Camera sensor not available")
                        continue
                    json_params = {
                        "sensor_id": world.camera_manager.sensor.id,
                        "u": 100,
                        "v": 200
                    }
                    return_value = world.world.line_trace_single(json.dumps(json_params))
                    print(f"line_trace_single return_value: {return_value}")
                    json_params = {
                        "sensor_id": world.camera_manager.sensor.id,
                        "u": 300,
                        "v": 400
                    }
                    return_value = world.world.line_trace_single(json.dumps(json_params))
                    print(f"line_trace_single return_value: {return_value}")
                
                    json_params = {
                        "sensor_id": world.camera_manager.sensor.id,
                        "uvs": [
                            { "u": 0, "v": 0 },
                            { "u": 100, "v": 200 },
                            { "u": 300, "v": 400 }
                        ]
                    }
                    return_multi = world.world.line_trace_multiple(json.dumps(json_params))
                    print(f"[line_trace_multiple output: {return_multi}")
                elif event.key == K_e:
                    # 使用 restart 重新创建机器人
                    try:
                        world.restart()
                    except Exception as e:
                        print(f"Failed to restart robot: {e}")
                elif event.key == K_F5:
                    self._control_main_camera_free(world)
                elif event.key == K_F6:
                    self._control_main_camera_fixed(world)
                elif event.key == K_y:
                    try:
                        # 获取玩家当前位置作为参考点
                        player_transform = world.player.get_transform()
                        player_location = player_transform.location
                        
                        # 定义5个相对位置（相对于玩家位置）
                        fire_locations = [
                            (0.0, 0.0, 0.0),      # 玩家位置
                            (2.0, 0.0, 0.0),      # 前方2米
                            (-2.0, 0.0, 0.0),     # 后方2米
                            (0.0, 2.0, 0.0),      # 右侧2米
                            (0.0, -2.0, 0.0),     # 左侧2米
                        ]
                        
                        created_count = 0
                        for i, (dx, dy, dz) in enumerate(fire_locations):
                            # 计算世界坐标位置
                            fire_location = (
                                player_location.x + dx,
                                player_location.y + dy,
                                player_location.z + dz
                            )
                            
                            effect_json = {
                                "type": "effect",
                                "params": {
                                    "category": "fire",
                                    "transform": {
                                        "location": {"x": fire_location[0], "y": fire_location[1], "z": fire_location[2]},
                                        "rotation": {"pitch": 0, "yaw": 0, "roll": 0},
                                        "scale": {"x": 1, "y": 1, "z": 1}
                                    }
                                }
                            }
                            json_str = json.dumps(effect_json)
                            uuid = world.world.create_object(json_str)
                            if uuid:
                                self.uuids.append(uuid)
                                created_count += 1
                                print(f"[K_y] Created fire #{i+1} at ({fire_location[0]:.2f}, {fire_location[1]:.2f}, {fire_location[2]:.2f}), uuid: {uuid}")
                            else:
                                print(f"[K_y] Failed to create fire #{i+1}")
                        
                        world.hud.notification(f'Created {created_count}/5 fire effects')
                        print(f"[K_y] Total created: {created_count}/5 fires")
                    except Exception as e:
                        print(f"[K_y] Failed to create fire effects: {e}")
                        world.hud.error(f'Failed to create fires: {e}')
                elif event.key == K_u:
                # 批量销毁所有 effect 类型的对象
                    try:
                        # 使用新的 JSON 格式的 destroy_objects 接口
                        destroy_json = {
                            "type": "effect"
                        }
                        json_str = json.dumps(destroy_json)
                        result_str = world.world.destroy_objects(json_str)
                        
                        # 解析返回的 JSON 结果
                        try:
                            result = json.loads(result_str)
                            if result.get("ok", False):
                                destroyed_count = result.get("destroyed_count", 0)
                                world.hud.notification(f'Destroyed {destroyed_count} effect objects')
                                print(f"[K_u] Successfully destroyed {destroyed_count} effect objects")
                                # 清空本地 UUID 列表（因为服务器端已经删除了）
                                self.uuids = []
                            else:
                                error_msg = result.get("error", "Unknown error")
                                world.hud.error(f'Destroy failed: {error_msg}')
                                print(f"[K_u] Destroy failed: {error_msg}")
                        except json.JSONDecodeError as e:
                            print(f"[K_u] Failed to parse destroy_objects result: {e}, raw: {result_str}")
                            world.hud.error('Failed to parse destroy result')
                    except Exception as e:
                        print(f"[K_u] Failed to destroy effects: {e}")
                        world.hud.error(f'Failed to destroy: {e}')
                
                if isinstance(self._control, carla.VehicleControl):
                    pass #车辆事件
            
        if isinstance(self._control, carla.VehicleControl):
            self._parse_vehicle_keys(pygame.key.get_pressed(), clock.get_time())
            self._parse_sensor_keys(pygame.key.get_pressed(), clock.get_time(), world)
            self._control.reverse = self._control.gear < 0
            # Set automatic control-related vehicle lights
            if self._control.brake:
                current_lights |= carla.VehicleLightState.Brake
            else: # Remove the Brake flag
                current_lights &= ~carla.VehicleLightState.Brake
            if self._control.reverse:
                current_lights |= carla.VehicleLightState.Reverse
            else: # Remove the Reverse flag
                current_lights &= ~carla.VehicleLightState.Reverse
            if current_lights != self._lights: # Change the light state only if necessary
                world.player.set_light_state(carla.VehicleLightState(current_lights))
            # Apply control
            world.player.apply_control(self._control)

        elif isinstance(self._control, carla.WalkerControl):
            self._parse_walker_keys(pygame.key.get_pressed(), clock.get_time(), world)
            world.player.apply_control(self._control)

        self._lights = current_lights

    # 机器人移动
    def _parse_vehicle_keys(self, keys, milliseconds):
        velocity = self._world.player.get_velocity()
        speed = math.sqrt(velocity.x**2 + velocity.y**2 + velocity.z**2)

        if keys[K_w]:
            if self._control.gear < 0:
                self._control.brake = 1
                self._control.throttle = 0
                if speed < 0.01:
                    self._control.brake = 0
                    self._control.gear = 1 
                    self._control.throttle = 1
            else:
                self._control.throttle = 1
                self._control.brake = 0
        elif keys[K_s]:
            if self._control.gear > 0 :
                self._control.brake = 1
                self._control.throttle = 0
                if speed < 0.01:
                    self._control.brake = 0
                    self._control.gear = -1
                    self._control.throttle = 1
            else:
                self._control.throttle = 1
                self._control.brake = 0
                self._control.gear = -1
        else:
            self._control.throttle = 0
            self._control.brake = 0

        if speed < 0.001:
            if keys[K_a] and not keys[K_w] and not keys[K_s]:
                self._control.steer = 0.0
                robot_yaw_delta = -1.0
            elif keys[K_d] and not keys[K_w] and not keys[K_s]:
                self._control.steer = 0.0
                robot_yaw_delta = +1.0
            else:
                robot_yaw_delta = 0.0
            transform = self._world.player.get_transform()
            yaw = transform.rotation.yaw + robot_yaw_delta
            transform.rotation.yaw = yaw
            self._world.player.set_transform(transform)
        else:
            # 方向盘
            if keys[K_a]:
                if self._steer_cache > 0:
                    self._steer_cache = 0
                else:
                    self._steer_cache = -1
            elif keys[K_d]:
                if self._steer_cache < 0:
                    self._steer_cache = 0
                else:
                    self._steer_cache = 1
            else:
                self._steer_cache = 0.0

        self._control.steer = round(self._steer_cache, 1)
        self._control.hand_brake = keys[K_SPACE]
                
    def visualize_navigable_points(self, navigable_points):
        # 提取x,y坐标
        x_coords = [pt.x for pt in navigable_points]
        y_coords = [pt.y for pt in navigable_points]
    
        # 创建图形
        plt.figure(figsize=(12, 10), dpi=100)
    
        # 绘制散点图
        plt.scatter(x_coords, y_coords, s=10, c='blue', alpha=0.6, label='Navigation Points')
    
        # 添加标题和标签
        plt.title('Carla Navigable Area Points', fontsize=15)
        plt.xlabel('X Coordinate (meters)', fontsize=12)
        plt.ylabel('Y Coordinate (meters)', fontsize=12)
        plt.grid(True, linestyle='--', alpha=0.7)
        plt.legend()
        plt.axis('equal')
        plt.gca().invert_xaxis()
        plt.show()

    # 根据缓存的初始 relative Transform 构造新的 Transform，只修改 rotation
    def _get_bone_transform_with_rotation(self, bone_name, new_rotation):
        if bone_name in self._bones_cache:
            cached = self._bones_cache[bone_name]
            return carla.Transform(location=cached.location, rotation=new_rotation)
        else:
            return carla.Transform(location=carla.Location(0, 0, 0), rotation=new_rotation)

    # 应用骨骼旋转
    def _apply_bone_rotation(self, bone_name, new_rotation):
        bones_ctrl = carla.RobotBoneControlIn()
        bone_transform = carla.bone_transform()
        bone_transform.name = bone_name
        bone_transform.transform = self._get_bone_transform_with_rotation(bone_name, new_rotation)
        bones_ctrl.bone_transforms = [bone_transform]
        self._world.player.set_bones_transform(bones_ctrl)
        # 更新骨骼旋转状态
        self._bone_rotations[bone_name] = new_rotation

    
    def _parse_sensor_keys(self, keys, milliseconds, world, angle=1):
        if world.camera_manager is None or world.camera_manager.sensor is None:
            return
        sensor = world.camera_manager.sensor
        
        if keys[K_r]:# 重置所有骨骼到初始状态
            bones_ctrl = carla.RobotBoneControlIn()
            bone_transforms = []
    
            for bone_name, initial_transform in self._bones_cache.items():
                bone_transform = carla.bone_transform()
                bone_transform.name = bone_name
                bone_transform.transform = initial_transform 
                bone_transforms.append(bone_transform)
    
            bones_ctrl.bone_transforms = bone_transforms
            self._world.player.set_bones_transform(bones_ctrl)
    
            for bone_name in self._bone_rotations:
                if bone_name in self._bones_cache:
                    self._bone_rotations[bone_name] = self._bones_cache[bone_name].rotation
    
            world.hud.notification('All bones reset to initial state.')

        if keys[K_UP]:
            current_rotation = self._bone_rotations.get("Camera", carla.Rotation())
            new_pitch = max(-60, min(60, current_rotation.pitch + angle))
            new_rotation = carla.Rotation(
                pitch=new_pitch,
                yaw=current_rotation.yaw,
                roll=current_rotation.roll
            )
            self._apply_bone_rotation("Camera", new_rotation)
            world.hud.notification('Camera Bone Pitch: %.1f°' % new_pitch)

        elif keys[K_DOWN]:
            current_rotation = self._bone_rotations.get("Camera", carla.Rotation())
            new_pitch = max(-60, min(60, current_rotation.pitch - angle))
            new_rotation = carla.Rotation(
                pitch=new_pitch,
                yaw=current_rotation.yaw,
                roll=current_rotation.roll
            )
            self._apply_bone_rotation("Camera", new_rotation)
            world.hud.notification('Camera Bone Pitch: %.1f°' % new_pitch)

        # YAW - 控制 Gimbal 骨骼
        if keys[K_LEFT]:
            current_rotation = self._bone_rotations.get("Gimbal", carla.Rotation())
            new_yaw = current_rotation.yaw - angle
            new_rotation = carla.Rotation(
                pitch=current_rotation.pitch,
                yaw=new_yaw,
                roll=current_rotation.roll
            )
            self._apply_bone_rotation("Gimbal", new_rotation)
            world.hud.notification('Gimbal Bone Yaw: %.1f°' % new_yaw)

        elif keys[K_RIGHT]:
            current_rotation = self._bone_rotations.get("Gimbal", carla.Rotation())
            new_yaw = current_rotation.yaw + angle
            new_rotation = carla.Rotation(
                pitch=current_rotation.pitch,
                yaw=new_yaw,
                roll=current_rotation.roll
            )
            self._apply_bone_rotation("Gimbal", new_rotation)
            world.hud.notification('Gimbal Bone Yaw: %.1f°' % new_yaw)
    
        # FOV 控制
        if keys[K_i]:
            self.current_fov = max(10, self.current_fov - 1.0)
            sensor.set_fov(self.current_fov)
        elif keys[K_o]:
            self.current_fov = min(90, self.current_fov + 1.0)
            sensor.set_fov(self.current_fov)
        elif keys[K_t]:
            self.current_fov = 90
            sensor.set_fov(self.current_fov)
        if keys[K_v]:
            navigable_points = world.world.get_navigable_area_points(world.player.id, 50)
            self.visualize_navigable_points(navigable_points)
        if keys[K_g]:
            transforms = world.world.get_gauges_transform()
            print(f"tatal gauges: {len(transforms)}")
            print(f"First gauge pos: {transforms[0].location.x, transforms[0].location.y, transforms[0].location.z}")
        # 调试：打印骨骼信息
        if keys[K_b]:
            bone_control_out = self._world.player.get_bones_transform()
            bones = bone_control_out.bone_transforms
            for bone in bones:
                print("Bone:", bone.name)
                print("  World:", bone.world)
                print("  Component:", bone.component)
                print("  Relative:", bone.relative)
            

    def _control_main_camera_free(self, world):
        player = world.player
        if player is None:
            world.hud.error("Player actor not available for camera control")
            return

        transform = player.get_transform()
        desired_location = carla.Location(
            x=transform.location.x - 5.0,
            y=transform.location.y,
            z=transform.location.z + 3.0
        )
        payload = {
            "mode": "free",
            "transform": {
                "location": {"x": desired_location.x, "y": desired_location.y, "z": desired_location.z},
                "rotation": {"pitch": transform.rotation.pitch, "yaw": transform.rotation.yaw, "roll": 0.0}
            }
        }
        world.world.control_main_camera(json.dumps(payload))
        
    def _control_main_camera_fixed(self, world):
        player = world.player
        if player is None:
            world.hud.error("Player actor not available for camera control")
            return

        payload = {
            "mode": "fixed",
            "robot_id": player.id,
            "relative_transform": {
                "location": {"x": -2, "y": -2, "z": 2},
                "rotation": {"pitch": -30.0, "yaw": 0.0, "roll": 0.0}
            }
        }
        world.world.control_main_camera(json.dumps(payload))          
        
    @staticmethod
    def _is_quit_shortcut(key):
        return (key == K_ESCAPE)


# ==============================================================================
# -- HUD -----------------------------------------------------------------------
# ==============================================================================


class HUD(object):
    def __init__(self, width, height):
        self.dim = (width, height)
        font = pygame.font.Font(pygame.font.get_default_font(), 20)
        font_name = 'courier' if os.name == 'nt' else 'mono'
        fonts = [x for x in pygame.font.get_fonts() if font_name in x]
        default_font = 'ubuntumono'
        mono = default_font if default_font in fonts else fonts[0]
        mono = pygame.font.match_font(mono)
        self._font_mono = pygame.font.Font(mono, 12 if os.name == 'nt' else 14)
        self._notifications = FadingText(font, (width, 40), (0, height - 40))
        self.help = HelpText(pygame.font.Font(mono, 16), width, height)
        self.server_fps = 0
        self.frame = 0
        self.simulation_time = 0
        self._show_info = True
        self._info_text = []
        self._server_clock = pygame.time.Clock()

    def on_world_tick(self, timestamp):
        self._server_clock.tick()
        self.server_fps = self._server_clock.get_fps()
        self.frame = timestamp.frame
        self.simulation_time = timestamp.elapsed_seconds

    def tick(self, world, clock):
        self._notifications.tick(world, clock)
        if not self._show_info:
            return
        t = world.player.get_transform()
        v = world.player.get_velocity()
        c = world.player.get_control()
        # 安全访问传感器数据
        compass = world.imu_sensor.compass if world.imu_sensor else 0.0
        heading = 'N' if compass > 270.5 or compass < 89.5 else ''
        heading += 'S' if 90.5 < compass < 269.5 else ''
        heading += 'E' if 0.5 < compass < 179.5 else ''
        heading += 'W' if 180.5 < compass < 359.5 else ''
        colhist = world.collision_sensor.get_collision_history() if world.collision_sensor else collections.defaultdict(int)
        collision = [colhist[x + self.frame - 200] for x in range(0, 200)]
        max_col = max(1.0, max(collision)) if collision else 1.0
        collision = [x / max_col for x in collision] if max_col > 0 else [0] * 200
        vehicles = world.world.get_actors().filter('vehicle.*')
        accelerometer = world.imu_sensor.accelerometer if world.imu_sensor else (0.0, 0.0, 0.0)
        gyroscope = world.imu_sensor.gyroscope if world.imu_sensor else (0.0, 0.0, 0.0)
        gnss_lat = world.gnss_sensor.lat if world.gnss_sensor else 0.0
        gnss_lon = world.gnss_sensor.lon if world.gnss_sensor else 0.0
        self._info_text = [
            'Server:  % 16.0f FPS' % self.server_fps,
            'Client:  % 16.0f FPS' % clock.get_fps(),
            '',
            'Vehicle: % 20s' % get_actor_display_name(world.player, truncate=20),
            'Map:     % 20s' % world.map.name.split('/')[-1],
            'Simulation time: % 12s' % datetime.timedelta(seconds=int(self.simulation_time)),
            '',
            'Speed:   % 15.0f km/h' % (3.6 * math.sqrt(v.x**2 + v.y**2 + v.z**2)),
            u'Compass:% 17.0f\N{DEGREE SIGN} % 2s' % (compass, heading),
            'Accelero: (%5.1f,%5.1f,%5.1f)' % accelerometer,
            'Gyroscop: (%5.1f,%5.1f,%5.1f)' % gyroscope,
            'Location:% 20s' % ('(% 5.1f, % 5.1f)' % (t.location.x, t.location.y)),
            'GNSS:% 24s' % ('(% 2.6f, % 3.6f)' % (gnss_lat, gnss_lon)),
            'Height:  % 18.0f m' % t.location.z,
            '']
        if isinstance(c, carla.VehicleControl):
            self._info_text += [
                ('Throttle:', c.throttle, 0.0, 1.0),
                ('Steer:', c.steer, -1.0, 1.0),
                ('Brake:', c.brake, 0.0, 1.0),
                ('Reverse:', c.reverse),
                ('Hand brake:', c.hand_brake),
                ('Manual:', c.manual_gear_shift),
                'Gear:        %s' % {-1: 'R', 0: 'N'}.get(c.gear, c.gear)]

        elif isinstance(c, carla.WalkerControl):
            self._info_text += [
                ('Speed:', c.speed, 0.0, 5.556),
                ('Jump:', c.jump)]
        self._info_text += [
            '',
            'Collision:',
            collision,
            '',
            'Number of vehicles: % 8d' % len(vehicles)]
        if len(vehicles) > 1:
            self._info_text += ['Nearby vehicles:']
            distance = lambda l: math.sqrt((l.x - t.location.x)**2 + (l.y - t.location.y)**2 + (l.z - t.location.z)**2)
            vehicles = [(distance(x.get_location()), x) for x in vehicles if x.id != world.player.id]
            for d, vehicle in sorted(vehicles, key=lambda vehicles: vehicles[0]):
                if d > 200.0:
                    break
                vehicle_type = get_actor_display_name(vehicle, truncate=22)
                self._info_text.append('% 4dm %s' % (d, vehicle_type))

    def toggle_info(self):
        self._show_info = not self._show_info

    def notification(self, text, seconds=2.0):
        self._notifications.set_text(text, seconds=seconds)

    def error(self, text):
        self._notifications.set_text('Error: %s' % text, (255, 0, 0))

    def render(self, display):
        if self._show_info:
            info_surface = pygame.Surface((220, self.dim[1]))
            info_surface.set_alpha(100)
            display.blit(info_surface, (0, 0))
            v_offset = 4
            bar_h_offset = 100
            bar_width = 106
            for item in self._info_text:
                if v_offset + 18 > self.dim[1]:
                    break
                if isinstance(item, list):
                    if len(item) > 1:
                        points = [(x + 8, v_offset + 8 + (1.0 - y) * 30) for x, y in enumerate(item)]
                        pygame.draw.lines(display, (255, 136, 0), False, points, 2)
                    item = None
                    v_offset += 18
                elif isinstance(item, tuple):
                    if isinstance(item[1], bool):
                        rect = pygame.Rect((bar_h_offset, v_offset + 8), (6, 6))
                        pygame.draw.rect(display, (255, 255, 255), rect, 0 if item[1] else 1)
                    else:
                        rect_border = pygame.Rect((bar_h_offset, v_offset + 8), (bar_width, 6))
                        pygame.draw.rect(display, (255, 255, 255), rect_border, 1)
                        f = (item[1] - item[2]) / (item[3] - item[2])
                        if item[2] < 0.0:
                            rect = pygame.Rect((bar_h_offset + f * (bar_width - 6), v_offset + 8), (6, 6))
                        else:
                            rect = pygame.Rect((bar_h_offset, v_offset + 8), (f * bar_width, 6))
                        pygame.draw.rect(display, (255, 255, 255), rect)
                    item = item[0]
                if item:  # At this point has to be a str.
                    surface = self._font_mono.render(item, True, (255, 255, 255))
                    display.blit(surface, (8, v_offset))
                v_offset += 18
        self._notifications.render(display)
        self.help.render(display)


# ==============================================================================
# -- FadingText ----------------------------------------------------------------
# ==============================================================================


class FadingText(object):
    def __init__(self, font, dim, pos):
        self.font = font
        self.dim = dim
        self.pos = pos
        self.seconds_left = 0
        self.surface = pygame.Surface(self.dim)

    def set_text(self, text, color=(255, 255, 255), seconds=2.0):
        text_texture = self.font.render(text, True, color)
        self.surface = pygame.Surface(self.dim)
        self.seconds_left = seconds
        self.surface.fill((0, 0, 0, 0))
        self.surface.blit(text_texture, (10, 11))

    def tick(self, _, clock):
        delta_seconds = 1e-3 * clock.get_time()
        self.seconds_left = max(0.0, self.seconds_left - delta_seconds)
        self.surface.set_alpha(500.0 * self.seconds_left)

    def render(self, display):
        display.blit(self.surface, self.pos)


# ==============================================================================
# -- HelpText ------------------------------------------------------------------
# ==============================================================================


class HelpText(object):
    """Helper class to handle text output using pygame"""
    def __init__(self, font, width, height):
        lines = __doc__.split('\n')
        self.font = font
        self.line_space = 18
        self.dim = (780, len(lines) * self.line_space + 12)
        self.pos = (0.5 * width - 0.5 * self.dim[0], 0.5 * height - 0.5 * self.dim[1])
        self.seconds_left = 0
        self.surface = pygame.Surface(self.dim)
        self.surface.fill((0, 0, 0, 0))
        for n, line in enumerate(lines):
            text_texture = self.font.render(line, True, (255, 255, 255))
            self.surface.blit(text_texture, (22, n * self.line_space))
            self._render = False
        self.surface.set_alpha(220)

    def toggle(self):
        self._render = not self._render

    def render(self, display):
        if self._render:
            display.blit(self.surface, self.pos)


# ==============================================================================
# -- CollisionSensorWrapper ----------------------------------------------------
# ==============================================================================
class CollisionSensorWrapper(object):
    def __init__(self, sensor_actor, hud):
        self.sensor = sensor_actor
        self.history = []
        self.hud = hud
        # 监听碰撞事件
        weak_self = weakref.ref(self)
        self.sensor.listen(lambda event: CollisionSensorWrapper._on_collision(weak_self, event))

    def get_collision_history(self):
        history = collections.defaultdict(int)
        for frame, intensity in self.history:
            history[frame] += intensity
        return history

    @staticmethod
    def _on_collision(weak_self, event):
        self = weak_self()
        if not self:
            return
        actor_type = get_actor_display_name(event.other_actor)
        self.hud.notification('Collision with %r' % actor_type)
        impulse = event.normal_impulse
        intensity = math.sqrt(impulse.x**2 + impulse.y**2 + impulse.z**2)
        self.history.append((event.frame, intensity))
        if len(self.history) > 4000:
            self.history.pop(0)

# ==============================================================================
# -- GnssSensorWrapper ---------------------------------------------------------
# ==============================================================================
class GnssSensorWrapper(object):
    def __init__(self, sensor_actor):
        self.sensor = sensor_actor
        self.lat = 0.0
        self.lon = 0.0
        # 监听GNSS事件
        weak_self = weakref.ref(self)
        self.sensor.listen(lambda event: GnssSensorWrapper._on_gnss_event(weak_self, event))

    @staticmethod
    def _on_gnss_event(weak_self, event):
        self = weak_self()
        if not self:
            return
        self.lat = event.latitude
        self.lon = event.longitude


# ==============================================================================
# -- IMUSensorWrapper ----------------------------------------------------------
# ==============================================================================
class IMUSensorWrapper(object):
    def __init__(self, sensor_actor):
        self.sensor = sensor_actor
        self.accelerometer = (0.0, 0.0, 0.0)
        self.gyroscope = (0.0, 0.0, 0.0)
        self.compass = 0.0
        # 监听IMU事件
        weak_self = weakref.ref(self)
        self.sensor.listen(
            lambda sensor_data: IMUSensorWrapper._IMU_callback(weak_self, sensor_data))

    @staticmethod
    def _IMU_callback(weak_self, sensor_data):
        self = weak_self()
        if not self:
            return
        limits = (-99.9, 99.9)
        self.accelerometer = (
            max(limits[0], min(limits[1], sensor_data.accelerometer.x)),
            max(limits[0], min(limits[1], sensor_data.accelerometer.y)),
            max(limits[0], min(limits[1], sensor_data.accelerometer.z)))
        self.gyroscope = (
            max(limits[0], min(limits[1], math.degrees(sensor_data.gyroscope.x))),
            max(limits[0], min(limits[1], math.degrees(sensor_data.gyroscope.y))),
            max(limits[0], min(limits[1], math.degrees(sensor_data.gyroscope.z))))
        self.compass = math.degrees(sensor_data.compass)


# ==============================================================================
# -- CameraManager -------------------------------------------------------------
# ==============================================================================


class CameraManager(object):
    def __init__(self, parent_actor, hud, gamma_correction):
        self.sensor = None
        self.surface = None
        self._parent = parent_actor
        self.hud = hud
        self.raw_depth_image = None
        self.lidar_range = 50
        # 外部模式：由 create_robot 返回的传感器集合
        self.use_external = False
        self.external_sensors = []
        self.external_index = -1
        bound_x = 0.5 + self._parent.bounding_box.extent.x
        bound_y = 0.5 + self._parent.bounding_box.extent.y
        bound_z = 0.5 + self._parent.bounding_box.extent.z
        Attachment = carla.AttachmentType

        if not self._parent.type_id.startswith("walker.pedestrian"):
            self._camera_transforms = [
                (carla.Transform(carla.Location(x=-2.0*bound_x, y=+0.0*bound_y, z=2.0*bound_z), carla.Rotation(pitch=8.0)), Attachment.SpringArmGhost),
                (carla.Transform(carla.Location(x=+0.8*bound_x, y=+0.0*bound_y, z=1.3*bound_z)), Attachment.Rigid),
                (carla.Transform(carla.Location(x=+1.9*bound_x, y=+1.0*bound_y, z=1.2*bound_z)), Attachment.SpringArmGhost),
                (carla.Transform(carla.Location(x=-2.8*bound_x, y=+0.0*bound_y, z=4.6*bound_z), carla.Rotation(pitch=6.0)), Attachment.SpringArmGhost),
                (carla.Transform(carla.Location(x=-1.0, y=-1.0*bound_y, z=0.4*bound_z)), Attachment.Rigid)]
        else:
            self._camera_transforms = [
                (carla.Transform(carla.Location(x=-2.5, z=0.0), carla.Rotation(pitch=-8.0)), Attachment.SpringArmGhost),
                (carla.Transform(carla.Location(x=1.6, z=1.7)), Attachment.Rigid),
                (carla.Transform(carla.Location(x=2.5, y=0.5, z=0.0), carla.Rotation(pitch=-8.0)), Attachment.SpringArmGhost),
                (carla.Transform(carla.Location(x=-4.0, z=2.0), carla.Rotation(pitch=6.0)), Attachment.SpringArmGhost),
                (carla.Transform(carla.Location(x=0, y=-2.5, z=-0.0), carla.Rotation(yaw=90.0)), Attachment.Rigid)]

        self.transform_index = 1
        self.sensors = [
            ['sensor.camera.rgb', cc.Raw, 'Camera RGB', {'enable_postprocess_effects':'True', 'gamma':'5.5'}],
            ['sensor.camera.depth', cc.Raw, 'Camera Depth (Raw)', {}],
            ['sensor.camera.depth', cc.LogarithmicDepth, 'Camera Depth (Logarithmic Gray Scale)', {}],
            ['sensor.lidar.ray_cast', None, 'Lidar (Ray-Cast)', {'range': '200', 'upper_fov': '15.0', 'lower_fov': '-15', 'horizontal_fov': '180.0'}],
        ]
        world = self._parent.get_world()
        bp_library = world.get_blueprint_library()
        for item in self.sensors:
            bp = bp_library.find(item[0])
            if item[0].startswith('sensor.camera'):
                bp.set_attribute('image_size_x', str(hud.dim[0]))
                bp.set_attribute('image_size_y', str(hud.dim[1]))
                if bp.has_attribute('gamma'):
                    bp.set_attribute('gamma', str(gamma_correction))
                for attr_name, attr_value in item[3].items():
                    bp.set_attribute(attr_name, attr_value)
            elif item[0].startswith('sensor.lidar'):
                self.lidar_range = 50

                for attr_name, attr_value in item[3].items():
                    bp.set_attribute(attr_name, attr_value)
                    if attr_name == 'range':
                        self.lidar_range = float(attr_value)

            item.append(bp)
        self.index = None

    def toggle_camera(self):
        self.transform_index = (self.transform_index + 1) % len(self._camera_transforms)
        # 外部模式不重新生成/切换机位（由父子绑定/骨骼决定）
        if not self.use_external:
            self.set_sensor(self.index, notify=False, force_respawn=True)

    def set_sensor(self, index, notify=True, force_respawn=False):
        if self.use_external:
            return  # 外部模式禁用内部相机生成
        index = index % len(self.sensors)
        needs_respawn = True if self.index is None else \
            (force_respawn or (self.sensors[index][2] != self.sensors[self.index][2]))
        if needs_respawn:
            if self.sensor is not None:
                self.sensor.destroy()
                self.surface = None
            self.sensor = self._parent.get_world().spawn_actor(
                self.sensors[index][-1],
                self._camera_transforms[self.transform_index][0],
                attach_to=self._parent,
                attachment_type=self._camera_transforms[self.transform_index][1])
            weak_self = weakref.ref(self)
            self.sensor.listen(lambda image: CameraManager._parse_image(weak_self, image))
        if notify:
            self.hud.notification(self.sensors[index][2])
        self.index = index

    def next_sensor(self):
        # 外部模式：只在已创建的外部传感器集合中切换
        if self.use_external and len(self.external_sensors) > 0:
            self.external_index = (self.external_index + 1) % len(self.external_sensors)
            self._activate_external_by_index(self.external_index)
            return
        # 内部模式：维持原逻辑
        self.set_sensor(self.index + 1)

    def set_external_sensors(self, sensor_actors):
        # 切换到外部模式，记录集合并激活第一项
        self.use_external = True
        self.external_sensors = list(sensor_actors) if sensor_actors else []
        self.external_index = 0 if self.external_sensors else -1
        if self.external_index >= 0:
            self._activate_external_by_index(self.external_index)

    def _activate_external_by_index(self, idx):
        if idx < 0 or idx >= len(self.external_sensors):
            return
        actor = self.external_sensors[idx]
        # 停止旧的监听
        try:
            if self.sensor is not None and self.sensor.id != actor.id:
                self.sensor.stop()
        except Exception:
            pass
        self.sensor = actor
        self.surface = None
        # 选择解析模式：0 相机 / 3 激光
        t = getattr(actor, 'type_id', '')
        if isinstance(t, str) and t.startswith('sensor.lidar.ray_cast'):
            self.index = 3
            label = 'External Lidar'
            # 尝试从传感器属性读取range
            try:
                rng = None
                if hasattr(actor, 'attributes') and isinstance(actor.attributes, dict):
                    rng = actor.attributes.get('range')
                if rng is None and hasattr(actor, 'get_attribute'):
                    rng_attr = actor.get_attribute('range')
                    if rng_attr is not None:
                        rng = rng_attr
                if rng is not None:
                    self.lidar_range = float(str(rng))
            except Exception:
                self.lidar_range = 50
        else:
            self.index = 0
            label = 'External Camera RGB'
        weak_self = weakref.ref(self)
        self.sensor.listen(lambda image: CameraManager._parse_image(weak_self, image))
        self.hud.notification(label)

    def destroy_external_sensors(self):
        # 停止并销毁通过 create_robot 接管的所有外部传感器
        try:
            # 若当前显示的传感器属于外部集合，也一起处理
            for act in list(self.external_sensors):
                try:
                    act.stop()
                except Exception:
                    pass
                try:
                    act.destroy()
                except Exception:
                    pass
        except Exception:
            pass
        # 清理状态
        self.external_sensors = []
        self.external_index = -1
        self.use_external = False
        # 如果当前 self.sensor 已被销毁且引用失效，则置空
        try:
            if self.sensor is not None and (not hasattr(self.sensor, 'is_alive') or not self.sensor.is_alive):
                self.sensor = None
        except Exception:
            self.sensor = None

    def render(self, display):
        if self.surface is not None:
            display.blit(self.surface, (0, 0))

    @staticmethod
    def _parse_image(weak_self, image):
        self = weak_self()
        
        if not self:
            return
        # 外部模式优先根据实际传感器类型判定
        t = None
        try:
            t = getattr(self.sensor, 'type_id', '')
        except Exception:
            t = ''
        if (self.use_external and isinstance(t, str) and t.startswith('sensor.lidar')) or (not self.use_external and self.sensors[self.index][0] == 'sensor.lidar.ray_cast'):
            try:
                raw = image.raw_data
                # 兼容两种布局：
                # 旧格式：16 字节/点 (x,y,z,intensity float32)
                # 新格式：24 字节/点 (x,y,z,intensity float32 + ring uint16 + padding uint16 + time float32)
                stride24 = 24
                if len(raw) % stride24 == 0 and len(raw) >= stride24:
                    dtype24 = np.dtype([
                        ("x", "f4"), ("y", "f4"), ("z", "f4"),
                        ("intensity", "f4"),
                        ("ring", "u2"), ("pad", "u2"),
                        ("time", "f4"),
                    ])
                    points = np.frombuffer(raw, dtype=dtype24)
                    lidar_xy = np.stack([points["x"], points["y"]], axis=1)
                else:
                    points = np.frombuffer(raw, dtype=np.dtype('f4'))
                    points = np.reshape(points, (int(points.shape[0] / 4), 4))
                    lidar_xy = points[:, :2]

                # 自适应缩放：优先根据本帧点云的最大半径填充视窗，缩放至窗口的 90%；若数据异常再回退用 range。
                max_radius = np.max(np.abs(lidar_xy)) if lidar_xy.size > 0 else 1.0
                base_scale = min(self.hud.dim) / (2.0 * max(1e-3, float(self.lidar_range)))
                if np.isfinite(max_radius) and max_radius > 1e-3:
                    scale = 0.9 * min(self.hud.dim) / (2.0 * max_radius)
                else:
                    scale = base_scale

                lidar_xy = lidar_xy * scale
                center = np.array([0.5 * self.hud.dim[0], 0.5 * self.hud.dim[1]], dtype=np.float32)
                lidar_xy = lidar_xy + center
                mask = np.isfinite(lidar_xy).all(axis=1)
                lidar_xy = lidar_xy[mask]
                idx = np.clip(lidar_xy.astype(np.int32), [0, 0], [self.hud.dim[0]-1, self.hud.dim[1]-1])
                lidar_img = np.zeros((self.hud.dim[0], self.hud.dim[1], 3), dtype=np.uint8)
                if idx.size > 0:
                    # 提高可见度：绘制 3x3 点块
                    for dx in (-1, 0, 1):
                        for dy in (-1, 0, 1):
                            px = np.clip(idx[:, 0] + dx, 0, self.hud.dim[0]-1)
                            py = np.clip(idx[:, 1] + dy, 0, self.hud.dim[1]-1)
                            lidar_img[px, py] = (255, 255, 255)
                self.surface = pygame.surfarray.make_surface(lidar_img)
            except Exception:
                # 出错时清空画面但不中断
                lidar_img = np.zeros((self.hud.dim[0], self.hud.dim[1], 3), dtype=np.uint8)
                self.surface = pygame.surfarray.make_surface(lidar_img)
        else:
            # 相机：外部模式默认 Raw，内部模式沿用表配置
            try:
                self.raw_depth_image = image
                if self.use_external:
                    image.convert(cc.Raw)
                else:
                    image.convert(self.sensors[self.index][1])
                array = np.frombuffer(image.raw_data, dtype=np.dtype("uint8"))
                array = np.reshape(array, (image.height, image.width, 4))
                array = array[:, :, :3]
                array = array[:, :, ::-1]
                self.surface = pygame.surfarray.make_surface(array.swapaxes(0, 1))
            except Exception:
                pass


# ==============================================================================
# -- game_loop() ---------------------------------------------------------------
# ==============================================================================


def game_loop(args):
    pygame.init()
    pygame.font.init()
    world = None
    original_settings = None

    try:
        client = carla.Client(args.host, args.port)
        client.set_timeout(2000.0)

        sim_world = client.get_world()
        traffic_manager = client.get_trafficmanager()
        if args.sync:
            original_settings = sim_world.get_settings()
            settings = sim_world.get_settings()
            if not settings.synchronous_mode:
                settings.synchronous_mode = True
                settings.fixed_delta_seconds = 0.05
            sim_world.apply_settings(settings)

            traffic_manager.set_synchronous_mode(True)

        display = pygame.display.set_mode(
            (args.width, args.height),
            pygame.HWSURFACE | pygame.DOUBLEBUF)
        display.fill((0,0,0))
        pygame.display.flip()

        hud = HUD(args.width, args.height)
        world = World(sim_world, hud, traffic_manager, args)
        controller = KeyboardControl(world)

        if args.sync:
            sim_world.tick()
        else:
            sim_world.wait_for_tick()

        clock = pygame.time.Clock()
        while True:
            if args.sync:
                sim_world.tick()
            clock.tick_busy_loop(60)
            if controller.parse_events(client, world, clock, args.sync):
                return
            world.tick(clock)
            world.render(display)
            pygame.display.flip()

    finally:

        if original_settings:
            sim_world.apply_settings(original_settings)

        if world is not None:
            world.destroy()

        pygame.quit()


# ==============================================================================
# -- main() --------------------------------------------------------------------
# ==============================================================================


def main():
    argparser = argparse.ArgumentParser(description='CARLA Manual Control Client')
    argparser.add_argument(
        '-v', '--verbose', action='store_true', dest='debug',
        help='print debug information')
    argparser.add_argument(
        '--host', metavar='H', default='127.0.0.1',
        help='IP of the host server (default: 127.0.0.1)')
    argparser.add_argument(
        '-p', '--port', metavar='P', default=2000, type=int,
        help='TCP port to listen to (default: 2000)')
    argparser.add_argument(
        '--res', metavar='WIDTHxHEIGHT', default='1280x720',
        help='window resolution (default: 1280x720)')
    argparser.add_argument(
        '--filter', metavar='PATTERN', default='vehicle.*',
        help='actor filter (default: "vehicle.*")')
    argparser.add_argument(
        '--generation', metavar='G', default='All',
        help='restrict to certain actor generation (values: "2","3","All" - default: "All")')
    argparser.add_argument(
        '--rolename', metavar='NAME', default='hero',
        help='actor role name (default: "hero")')
    argparser.add_argument(
        '--gamma', default=1.0, type=float,
        help='Gamma correction of the camera (default: 1.0)')
    argparser.add_argument(
        '--sync', action='store_true',
        help='Activate synchronous mode execution')
    args = argparser.parse_args()

    args.width, args.height = [int(x) for x in args.res.split('x')]

    log_level = logging.DEBUG if args.debug else logging.INFO
    logging.basicConfig(format='%(levelname)s: %(message)s', level=log_level)
    print('test')
    logging.info('listening to server %s:%s', args.host, args.port)

    print(__doc__)

    try:

        game_loop(args)

    except KeyboardInterrupt:
        print('\nCancelled by user. Bye!')


if __name__ == '__main__':

    main()
