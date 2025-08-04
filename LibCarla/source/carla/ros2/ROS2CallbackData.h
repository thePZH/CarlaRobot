// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include <variant>
#include <functional>
#include "carla/ros2/types/Vector3.h"

namespace carla {
namespace ros2 {

  struct VehicleControl
  {
    float   throttle;
    float   steer;
    float   brake;
    bool    hand_brake;
    bool    reverse;
    int32_t gear;
    bool    manual_gear_shift;
  };

  struct RobotControl
  {
    geometry_msgs::msg::Vector3 linear;   // 线速度 (m/s)
    geometry_msgs::msg::Vector3 angular;  // 角速度 (rad/s)
  };
  
  struct MessageControl
  {
    const char* message;
  };

  using ROS2CallbackData = std::variant<VehicleControl, RobotControl>;
  using ROS2MessageCallbackData = std::variant<MessageControl>;

  using ActorCallback = std::function<void(void *actor, ROS2CallbackData data)>;
  using ActorMessageCallback = std::function<void(void *actor, ROS2MessageCallbackData data)>;
  
} // namespace ros2
} // namespace carla
