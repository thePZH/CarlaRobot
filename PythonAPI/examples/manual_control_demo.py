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

try:
    import pygame
    from pygame.locals import KMOD_CTRL
    from pygame.locals import K_DOWN
    from pygame.locals import K_ESCAPE
    from pygame.locals import K_F1
    from pygame.locals import K_LEFT
    from pygame.locals import K_RIGHT
    from pygame.locals import K_SPACE
    from pygame.locals import K_UP
    from pygame.locals import K_a
    from pygame.locals import K_d
    from pygame.locals import K_f
    from pygame.locals import K_h
    from pygame.locals import K_r
    from pygame.locals import K_n
    from pygame.locals import K_q
    from pygame.locals import K_s
    from pygame.locals import K_w
    from pygame.locals import K_i
    from pygame.locals import K_o
    from pygame.locals import K_t
    from pygame.locals import K_v
    from pygame.locals import K_g
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
        self.restart()
        self.world.on_tick(hud.on_world_tick)
        self.show_vehicle_telemetry = False

    def restart(self):
        self.player_max_speed = 1.589
        self.player_max_speed_fast = 3.713
        # Keep same camera config if the camera manager exists.
        cam_index = self.camera_manager.index if self.camera_manager is not None else 0
        cam_pos_index = self.camera_manager.transform_index if self.camera_manager is not None else 0
        # Get a random blueprint.
        blueprint_list = get_actor_blueprints(self.world, self._actor_filter, self._actor_generation)
        if not blueprint_list:
            raise ValueError("Couldn't find any blueprints with the specified filters")
        blueprint = random.choice(blueprint_list)
        blueprint.set_attribute('role_name', self.actor_role_name)
        if blueprint.has_attribute('terramechanics'):
            blueprint.set_attribute('terramechanics', 'true')
        if blueprint.has_attribute('color'):
            color = random.choice(blueprint.get_attribute('color').recommended_values)
            blueprint.set_attribute('color', color)
        if blueprint.has_attribute('driver_id'):
            driver_id = random.choice(blueprint.get_attribute('driver_id').recommended_values)
            blueprint.set_attribute('driver_id', driver_id)
        if blueprint.has_attribute('is_invincible'):
            blueprint.set_attribute('is_invincible', 'true')
        # set the max speed
        if blueprint.has_attribute('speed'):
            self.player_max_speed = float(blueprint.get_attribute('speed').recommended_values[1])
            self.player_max_speed_fast = float(blueprint.get_attribute('speed').recommended_values[2])

        bp_lib = self.world.get_blueprint_library()
        vehicle_bp = bp_lib.find("vehicle.robot.01")
        #vehicle_bp = bp_lib.find("vehicle.lincoln.mkz")
        vehicle_bp.set_attribute("role_name", "ego")
        vehicle_bp.set_attribute("ros_name", "ego")

        spawn_point = carla.Transform(carla.Location(x=0, y=-5, z=0))
        # Spawn the player.
        if self.player is not None:
            # spawn_point = self.player.get_transform()
            # spawn_point.location.z += 2.0
            # spawn_point.rotation.roll = 0.0
            # spawn_point.rotation.pitch = 0.0
            self.destroy()

            self.player = self.world.try_spawn_actor(vehicle_bp, spawn_point)
            self.show_vehicle_telemetry = False
            self.modify_vehicle_physics(self.player)
        while self.player is None:
            # if not self.map.get_spawn_points():
            #     print('There are no spawn points available in your map/town.')
            #     print('Please add some Vehicle Spawn Point to your UE5 scene.')
            #     sys.exit(1)
            # spawn_points = self.map.get_spawn_points()
            self.player = self.world.try_spawn_actor(vehicle_bp, spawn_point)
            self.show_vehicle_telemetry = False
            self.modify_vehicle_physics(self.player)
        # Set up the sensors.
        self.collision_sensor = CollisionSensor(self.player, self.hud)
        self.gnss_sensor = GnssSensor(self.player)
        self.imu_sensor = IMUSensor(self.player)
        self.camera_manager = CameraManager(self.player, self.hud, self._gamma)
        self.camera_manager.transform_index = cam_pos_index
        self.camera_manager.set_sensor(cam_index, notify=False)
        actor_type = get_actor_display_name(self.player)
        self.hud.notification(actor_type)
        self.traffic_manager.update_vehicle_lights(self.player, True)

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
        pass
        # self.hud.tick(self, clock)

    def render(self, display):
        self.camera_manager.render(display)
        # self.hud.render(display)

    def destroy_sensors(self):
        self.camera_manager.sensor.destroy()
        self.camera_manager.sensor = None
        self.camera_manager.index = None

    def destroy(self):
        sensors = [
            self.camera_manager.sensor,
            self.collision_sensor.sensor,
            self.gnss_sensor.sensor,
            self.imu_sensor.sensor]
        for sensor in sensors:
            if sensor is not None:
                sensor.stop()
                sensor.destroy()
        if self.player is not None:
            self.player.destroy()


# ==============================================================================
# -- KeyboardControl -----------------------------------------------------------
# ==============================================================================
class KeyboardControl(object):
    """Class that handles keyboard input."""
    def __init__(self, world):
        self._world = world
        self._ackermann_enabled = False
        self._ackermann_reverse = 1
        if isinstance(world.player, carla.Vehicle):
            self._control = carla.VehicleControl()
            self._ackermann_control = carla.VehicleAckermannControl()
            self._lights = carla.VehicleLightState.NONE
            world.player.set_light_state(self._lights)
        elif isinstance(world.player, carla.Walker):
            self._control = carla.WalkerControl()
            self._rotation = world.player.get_transform().rotation
        else:
            raise NotImplementedError("Actor type not supported")
        self._steer_cache = 0.0
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
                    world.hud.toggle_info()
                elif event.key == K_h:
                    world.hud.help.toggle()
                elif event.key == K_n:
                    world.camera_manager.next_sensor()
                if isinstance(self._control, carla.VehicleControl):
                    if event.key == K_f:
                        # Toggle ackermann controller
                        self._ackermann_enabled = not self._ackermann_enabled
                        world.hud.show_ackermann_info(self._ackermann_enabled)
                        world.hud.notification("Ackermann Controller %s" %
                                               ("Enabled" if self._ackermann_enabled else "Disabled"))
                    if event.key == K_q:
                        if not self._ackermann_enabled:
                            self._control.gear = 1 if self._control.reverse else -1
                        else:
                            self._ackermann_reverse *= -1
                            # Reset ackermann control
                            self._ackermann_control = carla.VehicleAckermannControl()

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
            if not self._ackermann_enabled:
                world.player.apply_control(self._control)
            else:
                world.player.apply_ackermann_control(self._ackermann_control)
                # Update control to the last one applied by the ackermann controller.
                self._control = world.player.get_control()
                # Update hud with the newest ackermann control
                world.hud.update_ackermann_control(self._ackermann_control)

        elif isinstance(self._control, carla.WalkerControl):
            self._parse_walker_keys(pygame.key.get_pressed(), clock.get_time(), world)
            world.player.apply_control(self._control)

        self._lights = current_lights

    # 机器人移动
    def _parse_vehicle_keys(self, keys, milliseconds):
        if keys[K_w]:
            if not self._ackermann_enabled:
                # self._control.throttle = min(self._control.throttle + 0.1, 1.00)
                self._control.throttle = 1
            else:
                self._ackermann_control.speed += round(milliseconds * 0.005, 2) * self._ackermann_reverse
        else:
            if not self._ackermann_enabled:
                self._control.throttle = 0.0

        if keys[K_s]:
            if not self._ackermann_enabled:
                # self._control.brake = min(self._control.brake + 0.2, 1)
                self._control.brake = 1
            else:
                self._ackermann_control.speed -= min(abs(self._ackermann_control.speed), round(milliseconds * 0.005, 2)) * self._ackermann_reverse
                self._ackermann_control.speed = max(0, abs(self._ackermann_control.speed)) * self._ackermann_reverse
        else:
            if not self._ackermann_enabled:
                self._control.brake = 0

        # 如果速度 == 0，就做原地旋转，transform RPC
        velocity = self._world.player.get_velocity()
        speed = math.sqrt(velocity.x**2 + velocity.y**2 + velocity.z**2)
        if speed < 0.001:
            if keys[K_a]:
                self._control.steer = 0.0
                robot_yaw_delta = -1.0
            elif keys[K_d]:
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
            if not self._ackermann_enabled:
                self._control.steer = round(self._steer_cache, 1)
                self._control.hand_brake = keys[K_SPACE]
            else:
                self._ackermann_control.steer = round(self._steer_cache, 1)

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

    current_transform = carla.Transform()
    current_rotation = current_transform.rotation
    current_fov = 90.0
    def _parse_sensor_keys(self, keys, milliseconds, world, angle=0.1):
        if world.camera_manager.all_sensors[0] is None:
            return
        sensors = world.camera_manager.all_sensors
        if keys[K_r]:
            new_transform = carla.Transform()
            sensors[0].set_transform(new_transform)
            self.current_rotation = carla.Rotation()

        if keys[K_UP]:
            # PITCH 增加
            new_pitch = self.current_rotation.pitch + angle
            # 限制 pitch 在 [-60, 60]
            new_pitch = max(-60, min(60, new_pitch))
            new_rotation = carla.Rotation(
                pitch=new_pitch,
                yaw=self.current_rotation.yaw,
                roll=self.current_rotation.roll
            )
            new_transform = carla.Transform(
                location=self.current_transform.location,
                rotation=new_rotation
            )
            print(f"nnnnnn")
            sensors[1].set_transform(new_transform)
            world.hud.notification('Sensor Pitch: %.1f°' % new_pitch)
            self.current_rotation = new_rotation

        if keys[K_DOWN]:
            # PITCH 减少
            new_pitch = self.current_rotation.pitch - angle
            new_pitch = max(-60, min(60, new_pitch))
            new_rotation = carla.Rotation(
                pitch=new_pitch,
                yaw=self.current_rotation.yaw,
                roll=self.current_rotation.roll
            )
            new_transform = carla.Transform(
                location=self.current_transform.location,
                rotation=new_rotation
            )
            sensors[0].set_transform(new_transform)
            world.hud.notification('Sensor Pitch: %.1f°' % new_pitch)
            self.current_rotation = new_rotation

        if keys[K_LEFT]:
            # YAW 左转
            new_yaw = self.current_rotation.yaw - angle
            new_rotation = carla.Rotation(
                pitch=self.current_rotation.pitch,
                yaw=new_yaw,
                roll=self.current_rotation.roll
            )
            new_transform = carla.Transform(
                location=self.current_transform.location,
                rotation=new_rotation
            )
            sensors[0].set_transform(new_transform)
            world.hud.notification('Sensor Yaw: %.1f°' % new_yaw)
            self.current_rotation = new_rotation

        if keys[K_RIGHT]:
            # YAW 右转
            new_yaw = self.current_rotation.yaw + angle
            new_rotation = carla.Rotation(
                pitch=self.current_rotation.pitch,
                yaw=new_yaw,
                roll=self.current_rotation.roll
            )
            new_transform = carla.Transform(
                location=self.current_transform.location,
                rotation=new_rotation
            )
            sensors[0].set_transform(new_transform)
            world.hud.notification('Sensor Yaw: %.1f°' % new_yaw)
            self.current_rotation = new_rotation

        # zoom
        if keys[K_i]:
            self.current_fov = max(10, self.current_fov - 1.0)
            print(f"Current FOV: {self.current_fov}")
            sensors[0].set_fov(self.current_fov)
        if keys[K_o]:
            self.current_fov = min(90, self.current_fov + 1.0)
            print(f"Current FOV: {self.current_fov}")
            sensors[0].set_fov(self.current_fov)
        if keys[K_t]:
            self.current_fov = 90
            sensors[0].set_fov(90)
        if keys[K_v]:
            navigable_points = world.world.get_navigable_area_points(world.player.id, 50)
            self.visualize_navigable_points(navigable_points)
        if keys[K_g]:
            transforms = world.world.get_gauges_transform()
            print(f"tatal gauges: {len(transforms)}")
            print(f"First gauge pos: {transforms[0].location.x, transforms[0].location.y, transforms[0].location.z}")

    @staticmethod
    def _is_quit_shortcut(key):
        return (key == K_ESCAPE) or (key == K_q and pygame.key.get_mods() & KMOD_CTRL)


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

        self._show_ackermann_info = False
        self._ackermann_control = carla.VehicleAckermannControl()

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
        compass = world.imu_sensor.compass
        heading = 'N' if compass > 270.5 or compass < 89.5 else ''
        heading += 'S' if 90.5 < compass < 269.5 else ''
        heading += 'E' if 0.5 < compass < 179.5 else ''
        heading += 'W' if 180.5 < compass < 359.5 else ''
        colhist = world.collision_sensor.get_collision_history()
        collision = [colhist[x + self.frame - 200] for x in range(0, 200)]
        max_col = max(1.0, max(collision))
        collision = [x / max_col for x in collision]
        vehicles = world.world.get_actors().filter('vehicle.*')
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
            'Accelero: (%5.1f,%5.1f,%5.1f)' % (world.imu_sensor.accelerometer),
            'Gyroscop: (%5.1f,%5.1f,%5.1f)' % (world.imu_sensor.gyroscope),
            'Location:% 20s' % ('(% 5.1f, % 5.1f)' % (t.location.x, t.location.y)),
            'GNSS:% 24s' % ('(% 2.6f, % 3.6f)' % (world.gnss_sensor.lat, world.gnss_sensor.lon)),
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
            if self._show_ackermann_info:
                self._info_text += [
                    '',
                    'Ackermann Controller:',
                    '  Target speed: % 8.0f km/h' % (3.6*self._ackermann_control.speed),
                    ]
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

    def show_ackermann_info(self, enabled):
        self._show_ackermann_info = enabled

    def update_ackermann_control(self, ackermann_control):
        self._ackermann_control = ackermann_control

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
# -- CollisionSensor -----------------------------------------------------------
# ==============================================================================
class CollisionSensor(object):
    def __init__(self, parent_actor, hud):
        self.sensor = None
        self.history = []
        self._parent = parent_actor
        self.hud = hud
        world = self._parent.get_world()
        bp = world.get_blueprint_library().find('sensor.other.collision')
        self.sensor = world.spawn_actor(bp, carla.Transform(), attach_to=self._parent)
        # We need to pass the lambda a weak reference to self to avoid circular
        # reference.
        weak_self = weakref.ref(self)
        self.sensor.listen(lambda event: CollisionSensor._on_collision(weak_self, event))

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
# -- GnssSensor ----------------------------------------------------------------
# ==============================================================================


class GnssSensor(object):
    def __init__(self, parent_actor):
        self.sensor = None
        self._parent = parent_actor
        self.lat = 0.0
        self.lon = 0.0
        world = self._parent.get_world()
        bp = world.get_blueprint_library().find('sensor.other.gnss')
        self.sensor = world.spawn_actor(bp, carla.Transform(carla.Location(x=1.0, z=2.8)), attach_to=self._parent)
        # We need to pass the lambda a weak reference to self to avoid circular
        # reference.
        weak_self = weakref.ref(self)
        self.sensor.listen(lambda event: GnssSensor._on_gnss_event(weak_self, event))

    @staticmethod
    def _on_gnss_event(weak_self, event):
        self = weak_self()
        if not self:
            return
        self.lat = event.latitude
        self.lon = event.longitude


# ==============================================================================
# -- IMUSensor -----------------------------------------------------------------
# ==============================================================================


class IMUSensor(object):
    def __init__(self, parent_actor):
        self.sensor = None
        self._parent = parent_actor
        self.accelerometer = (0.0, 0.0, 0.0)
        self.gyroscope = (0.0, 0.0, 0.0)
        self.compass = 0.0
        world = self._parent.get_world()
        bp = world.get_blueprint_library().find('sensor.other.imu')
        self.sensor = world.spawn_actor(
            bp, carla.Transform(), attach_to=self._parent)
        # We need to pass the lambda a weak reference to self to avoid circular
        # reference.
        weak_self = weakref.ref(self)
        self.sensor.listen(
            lambda sensor_data: IMUSensor._IMU_callback(weak_self, sensor_data))

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
            ['sensor.camera.rgb', cc.Raw, 'Camera RGB', {}],
            ['sensor.camera.depth', cc.Raw, 'Camera Depth (Raw)', {}],
            ['sensor.camera.depth', cc.LogarithmicDepth, 'Camera Depth (Logarithmic Gray Scale)', {}],
            ['sensor.lidar.ray_cast', None, 'Lidar (Ray-Cast)', {'range': '200', 'upper_fov': '15.0', 'lower_fov': '-15', 'horizontal_fov': '180.0'}],
        ]
        world = self._parent.get_world()
        bp_library = world.get_blueprint_library()
        for item in self.sensors:
            bp = bp_library.find(item[0])
            if item[0].startswith('sensor.camera'):
                camera_type = item[0].split('.')[-1]
                bp.set_attribute("ros_name", f"{camera_type}_{len(item[2])}")
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
        self.set_sensor(self.index, notify=False, force_respawn=True)

    def set_sensor(self, index, notify=True, force_respawn=False):
        # 忽略参数，直接创建所有4个传感器
        if hasattr(self, 'all_sensors') and self.all_sensors:
            # 如果已经创建过，先销毁
            for sensor in self.all_sensors:
                if sensor:
                    sensor.destroy()
    
        self.all_sensors = []  # 存储所有传感器
        self.all_surfaces = [None] * 4  # 存储4个传感器的表面
    
        # 创建所有传感器
        for i in range(len(self.sensors)):
            sensor = self._parent.get_world().spawn_actor(
                self.sensors[i][-1],
                self._camera_transforms[self.transform_index][0],
                attach_to=self._parent,
                attachment_type=self._camera_transforms[self.transform_index][1])
    
            weak_self = weakref.ref(self)
            # 为每个传感器指定索引
            sensor.listen(lambda image, idx=i: CameraManager._parse_image(weak_self, image, idx))
    
            self.all_sensors.append(sensor)
    
        # 设置当前索引为0（RGB相机）
        self.index = 0
        if notify:
            self.hud.notification("所有传感器已启动: RGB + Depth + LogDepth + Lidar")

    def next_sensor(self):
        pass
        # self.set_sensor(self.index + 1)

    def render(self, display):
        # 显示RGB主画面（全屏）
        if self.all_surfaces and self.all_surfaces[0] is not None:
            display.blit(self.all_surfaces[0], (0, 0))
    
        # 画中画设置 - 小窗口大小
        pip_size = (self.hud.dim[0] // 6, self.hud.dim[1] // 6)
        spacing = 5  # 窗口间距
    
        # 四个子窗口竖直一列，靠右边排列
        positions = [
            (self.hud.dim[0] - pip_size[0] - spacing, spacing),  # 第1个（顶）
            (self.hud.dim[0] - pip_size[0] - spacing, pip_size[1] + spacing * 2),  # 第2个
            (self.hud.dim[0] - pip_size[0] - spacing, pip_size[1] * 2 + spacing * 3),  # 第3个
            (self.hud.dim[0] - pip_size[0] - spacing, pip_size[1] * 3 + spacing * 4)   # 第4个（底）
        ]
    
        # 传感器名称（对应4个子窗口）
        sensor_names = [
            "RGB View",
            "Depth Raw",
            "Depth Log",
            "Lidar"
        ]
    
        # 显示4个子窗口
        for i in range(4):
            if self.all_surfaces[i] is not None:
                # 缩放图像到画中画大小
                pip_surface = pygame.transform.scale(self.all_surfaces[i], pip_size)
    
                # 画边框
                border_rect = pygame.Rect(
                    positions[i][0] - 2,
                    positions[i][1] - 2,
                    pip_size[0] + 4,
                    pip_size[1] + 4
                )
                pygame.draw.rect(display, (255, 255, 255), border_rect, 2)
    
                # 显示画中画
                display.blit(pip_surface, positions[i])
    
                # 添加标签（黑色背景确保可读性）
                font = pygame.font.SysFont('Arial', 10)
                label = font.render(sensor_names[i], True, (255, 255, 255))
    
                # 绘制半透明背景
                label_bg = pygame.Surface((label.get_width() + 4, label.get_height() + 2))
                label_bg.set_alpha(180)
                label_bg.fill((0, 0, 0))
    
                display.blit(label_bg, (positions[i][0] + 2, positions[i][1] + 2))
                display.blit(label, (positions[i][0] + 4, positions[i][1] + 3))


    @staticmethod
    def _parse_image(weak_self, image, sensor_index):
        self = weak_self()
        if not self:
            return
    
        if not hasattr(self, 'all_surfaces'):
            self.all_surfaces = [None] * 4
    
        sensor_type = self.sensors[sensor_index][0]
    
        if sensor_type == 'sensor.lidar.ray_cast':
            # 处理Lidar数据 - 优化版本
            points = np.frombuffer(image.raw_data, dtype=np.dtype('f4'))
            points = points.reshape(-1, 4)
        
            # 只取前两列（x,y）
            lidar_data = points[:, :2].copy()  # 使用copy避免视图问题
        
            # 缩放和居中
            scale_factor = 3.0
            lidar_data *= min(self.hud.dim) / (2.0 * self.lidar_range) * scale_factor
            lidar_data += (0.5 * self.hud.dim[0], 0.5 * self.hud.dim[1])
        
            # 限制范围并取整
            lidar_data = np.clip(lidar_data, 0, [self.hud.dim[0]-1, self.hud.dim[1]-1])
            lidar_data = lidar_data.astype(np.int32)
        
            # 使用向量化操作创建图像 - 大幅提升性能！
            lidar_img = np.zeros((self.hud.dim[1], self.hud.dim[0], 3), dtype=np.uint8)
        
            # 方法1：直接赋值（最快）
            valid_mask = (lidar_data[:, 0] < self.hud.dim[0]) & (lidar_data[:, 1] < self.hud.dim[1])
            valid_points = lidar_data[valid_mask]
            lidar_img[valid_points[:, 1], valid_points[:, 0]] = (255, 255, 255)
        
            surface = pygame.surfarray.make_surface(lidar_img.swapaxes(0, 1))  # 需要交换轴
        else:
            # 处理相机数据
            image.convert(self.sensors[sensor_index][1])
            array = np.frombuffer(image.raw_data, dtype=np.dtype("uint8"))
            array = np.reshape(array, (image.height, image.width, 4))
            array = array[:, :, :3]
            array = array[:, :, ::-1]
            surface = pygame.surfarray.make_surface(array.swapaxes(0, 1))
    
        # 存储到对应的表面
        self.all_surfaces[sensor_index] = surface
    
        # 如果是RGB相机（索引0），也设置给主surface用于兼容原有代码
        if sensor_index == 0:
            self.surface = surface


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
