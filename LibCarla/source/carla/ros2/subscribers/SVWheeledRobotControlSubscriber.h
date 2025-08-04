#pragma once
#include <memory>
#include <vector>
#include "CarlaSubscriber.h"
#include "carla/ros2/ROS2CallbackData.h"

namespace carla {
	namespace ros2 {

		struct SVWheeledRobotControlSubscriberImpl;

		class SVWheeledRobotControlSubscriber : public CarlaSubscriber {
		public:
			SVWheeledRobotControlSubscriber(void* robot, const char* ros_name = "", const char* parent = "");
			~SVWheeledRobotControlSubscriber();
			SVWheeledRobotControlSubscriber(const SVWheeledRobotControlSubscriber&);
			SVWheeledRobotControlSubscriber& operator=(const SVWheeledRobotControlSubscriber&);
			SVWheeledRobotControlSubscriber(SVWheeledRobotControlSubscriber&&);
			SVWheeledRobotControlSubscriber& operator=(SVWheeledRobotControlSubscriber&&);

			bool HasNewMessage();
			bool IsAlive();
			VehicleControl GetMessage();
			void* GetVehicle();

			bool Init();
			bool Read();
			const char* type() const override { return "Wheeled Robot control"; }

			//Do not call, for internal use only
			void ForwardMessage(VehicleControl control);
			void DestroySubscriber();
		private:
			void SetData(int32_t seconds, uint32_t nanoseconds, uint32_t actor_id, std::vector<float>&& data);

		private:
			std::shared_ptr<SVWheeledRobotControlSubscriberImpl> _impl;
		};
	}
}
