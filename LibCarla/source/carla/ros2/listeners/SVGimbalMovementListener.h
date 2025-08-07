// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB).
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include <memory>

namespace carla {
	namespace ros2 {

		class SVGimbalMovementListenerImpl;
		class SVGimbalMovementSubscriber;

		class SVGimbalMovementListener {
		public:
			SVGimbalMovementListener(SVGimbalMovementSubscriber* owner);
			~SVGimbalMovementListener();
			SVGimbalMovementListener(const SVGimbalMovementListener&) = delete;
			SVGimbalMovementListener& operator=(const SVGimbalMovementListener&) = delete;
			SVGimbalMovementListener(SVGimbalMovementListener&&) = delete;
			SVGimbalMovementListener& operator=(SVGimbalMovementListener&&) = delete;

			void SetOwner(SVGimbalMovementSubscriber* owner);

			std::unique_ptr<SVGimbalMovementListenerImpl> _impl;
		};
	}
}
