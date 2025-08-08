#pragma once
#include <memory>
#include <vector>
#include "CarlaSubscriber.h"
#include "carla/ros2/ROS2CallbackData.h"

struct GimbalControl;
namespace carla {
	namespace ros2 {

		struct SVGimbalMovementSubscriberImpl;

		class SVGimbalMovementSubscriber : public CarlaSubscriber {
		public:
			SVGimbalMovementSubscriber(void* robot, const char* ros_name = "", const char* parent = "");
			~SVGimbalMovementSubscriber();
			SVGimbalMovementSubscriber(const SVGimbalMovementSubscriber&);
			SVGimbalMovementSubscriber& operator=(const SVGimbalMovementSubscriber&);
			SVGimbalMovementSubscriber(SVGimbalMovementSubscriber&&);
			SVGimbalMovementSubscriber& operator=(SVGimbalMovementSubscriber&&);

			bool HasNewMessage();
			bool IsAlive();
			GimbalControl GetMessage();
			void* GetVehicle();

			bool Init();
			bool Read();
			const char* type() const override { return "Wheeled Robot control"; }

			//Do not call, for internal use only
			void ForwardMessage(GimbalControl control);
			void DestroySubscriber();
		private:
			void SetData(int32_t seconds, uint32_t nanoseconds, uint32_t actor_id, std::vector<float>&& data);

		private:
			std::shared_ptr<SVGimbalMovementSubscriberImpl> _impl;
		};
	}
}
