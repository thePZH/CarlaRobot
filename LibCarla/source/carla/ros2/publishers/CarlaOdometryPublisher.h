// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB).
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include <memory>
#include <string>

#include "CarlaPublisher.h"

namespace carla
{
namespace ros2
{

	struct CarlaOdometryPublisherImpl;

	// 发布 nav_msgs::msg::Odometry（/odom），用于底盘反馈（线速度、角速度等）
	class CarlaOdometryPublisher : public CarlaPublisher
	{
	public:
		CarlaOdometryPublisher(const char *ros_name = "odom", const char *parent = "");
		~CarlaOdometryPublisher();
		CarlaOdometryPublisher(const CarlaOdometryPublisher &);
		CarlaOdometryPublisher &operator=(const CarlaOdometryPublisher &);
		CarlaOdometryPublisher(CarlaOdometryPublisher &&);
		CarlaOdometryPublisher &operator=(CarlaOdometryPublisher &&);

		bool Init();
		bool Publish();

		// location/rotation/linear_velocity/angular_velocity 均为长度为 3 的 float 数组
		// location:  世界坐标 (x, y, z)
		// rotation:  UE 欧拉角 (roll, pitch, yaw) [度]
		// linear_velocity:  世界系线速度 (vx, vy, vz)
		// angular_velocity: 世界系角速度 (wx, wy, wz)
		void SetData(
			int32_t seconds,
			uint32_t nanoseconds,
			const float *location,
			const float *rotation,
			const float *linear_velocity,
			const float *angular_velocity);

		void SetHeaderFrameId(const std::string &frame_id);
		void SetChildFrameId(const std::string &frame_id);

		const char *type() const override
		{
			return "odometry";
		}

	private:
		std::shared_ptr<CarlaOdometryPublisherImpl> _impl;
		std::string _header_frame_id;
	};
}
}


