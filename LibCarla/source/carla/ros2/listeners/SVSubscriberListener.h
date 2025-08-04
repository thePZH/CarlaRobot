// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB).
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include <memory>

namespace carla {
namespace ros2 {

  class SVSubscriberListenerImpl;
  class SVWheeledRobotControlSubscriber;

  class SVSubscriberListener {
    public:
      SVSubscriberListener(SVWheeledRobotControlSubscriber* owner);
      ~SVSubscriberListener();
      SVSubscriberListener(const SVSubscriberListener&) = delete;
      SVSubscriberListener& operator=(const SVSubscriberListener&) = delete;
      SVSubscriberListener(SVSubscriberListener&&) = delete;
      SVSubscriberListener& operator=(SVSubscriberListener&&) = delete;

      void SetOwner(SVWheeledRobotControlSubscriber* owner);

      std::unique_ptr<SVSubscriberListenerImpl> _impl;
  };
}
}
