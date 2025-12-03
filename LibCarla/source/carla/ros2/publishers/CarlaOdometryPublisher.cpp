// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include "CarlaOdometryPublisher.h"

#include <cmath>
#include <cstring>
#include <string>

#include "carla/ros2/types/OdometryPubSubTypes.h"
#include "carla/ros2/types/Odometry.h"
#include "carla/ros2/types/TwistWithCovariance.h"
#include "carla/ros2/types/Twist.h"
#include "carla/ros2/types/Vector3.h"
#include "carla/ros2/types/Header.h"
#include "carla/ros2/types/Time.h"
#include "carla/ros2/listeners/CarlaListener.h"

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/topic/qos/TopicQos.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>

#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/qos/QosPolicies.h>

namespace carla
{
namespace ros2
{

	namespace efd = eprosima::fastdds::dds;
	using erc = eprosima::fastrtps::types::ReturnCode_t;

	struct CarlaOdometryPublisherImpl
	{
		efd::DomainParticipant *m_Participant { nullptr };
		efd::Publisher *m_Publisher { nullptr };
		efd::Topic *m_Topic { nullptr };
		efd::DataWriter *m_DataWriter { nullptr };
		efd::TypeSupport m_Type { new nav_msgs::msg::OdometryPubSubType() };
		CarlaListener m_Listener {};
		nav_msgs::msg::Odometry m_Odometry {};
	};

	bool CarlaOdometryPublisher::Init()
	{
		if (_impl->m_Type == nullptr)
		{
			std::cerr << "Invalid TypeSupport" << std::endl;
			return false;
		}

		efd::DomainParticipantQos participantQos = efd::PARTICIPANT_QOS_DEFAULT;
		participantQos.name(_name);

		auto factory = efd::DomainParticipantFactory::get_instance();
		_impl->m_Participant = factory->create_participant(6, participantQos);
		if (_impl->m_Participant == nullptr)
		{
			std::cerr << "Failed to create DomainParticipant" << std::endl;
			return false;
		}

		_impl->m_Type.register_type(_impl->m_Participant);

		efd::PublisherQos publisherQos = efd::PUBLISHER_QOS_DEFAULT;
		_impl->m_Publisher = _impl->m_Participant->create_publisher(publisherQos, nullptr);
		if (_impl->m_Publisher == nullptr)
		{
			std::cerr << "Failed to create Publisher" << std::endl;
			return false;
		}

		efd::TopicQos topicQos = efd::TOPIC_QOS_DEFAULT;
		const std::string base { "rt/carla/" };
		std::string topicName = base;
		if (!_parent.empty())
		{
			topicName += _parent + "/";
		}
		topicName += _name;

		std::cout << "[CarlaOdometryPublisher] Creating topic: " << topicName << std::endl;
		_impl->m_Topic = _impl->m_Participant->create_topic(topicName, _impl->m_Type->getName(), topicQos);
		if (_impl->m_Topic == nullptr)
		{
			std::cerr << "[CarlaOdometryPublisher] Failed to create Topic: " << topicName << std::endl;
			return false;
		}
		std::cout << "[CarlaOdometryPublisher] Successfully created topic: " << topicName << std::endl;

		efd::DataWriterQos writerQos = efd::DATAWRITER_QOS_DEFAULT;
		writerQos.endpoint().history_memory_policy = eprosima::fastrtps::rtps::PREALLOCATED_WITH_REALLOC_MEMORY_MODE;

		// CarlaListenerImpl 继承自 DataWriterListener，但此处类型前置声明为不完全类型，
		// 与其它 Publisher 一样使用 C 风格转换以避免编译器在此处进行继承关系检查。
		efd::DataWriterListener *listener = (efd::DataWriterListener *)_impl->m_Listener._impl.get();
		_impl->m_DataWriter = _impl->m_Publisher->create_datawriter(_impl->m_Topic, writerQos, listener);
		if (_impl->m_DataWriter == nullptr)
		{
			std::cerr << "Failed to create DataWriter" << std::endl;
			return false;
		}

		// 一般情况下，child_frame_id 代表车辆底盘坐标系，frame_id 为上层坐标系（例如 map 或 odom）
		_frame_id = _name;

		std::cout << "[CarlaOdometryPublisher] Init() completed successfully. Topic: " << topicName << std::endl;
		return true;
	}

	bool CarlaOdometryPublisher::Publish()
	{
		if (!_impl->m_DataWriter)
		{
			std::cerr << "[CarlaOdometryPublisher] Publish() called but DataWriter is null" << std::endl;
			return false;
		}
		
		eprosima::fastrtps::rtps::InstanceHandle_t instanceHandle;
		erc returnCode = _impl->m_DataWriter->write(&_impl->m_Odometry, instanceHandle);

		if (returnCode == erc::ReturnCodeValue::RETCODE_OK)
		{
			static int32_t successCount = 0;
			if (successCount++ % 300 == 0) // 每5秒打印一次（假设60fps）
			{
				std::cout << "[CarlaOdometryPublisher] Publish() success (count: " << successCount << ")" << std::endl;
			}
			return true;
		}
		if (returnCode == erc::ReturnCodeValue::RETCODE_ERROR)
		{
			std::cerr << "RETCODE_ERROR" << std::endl;
			return false;
		}
		if (returnCode == erc::ReturnCodeValue::RETCODE_UNSUPPORTED)
		{
			std::cerr << "RETCODE_UNSUPPORTED" << std::endl;
			return false;
		}
		if (returnCode == erc::ReturnCodeValue::RETCODE_BAD_PARAMETER)
		{
			std::cerr << "RETCODE_BAD_PARAMETER" << std::endl;
			return false;
		}
		if (returnCode == erc::ReturnCodeValue::RETCODE_PRECONDITION_NOT_MET)
		{
			std::cerr << "RETCODE_PRECONDITION_NOT_MET" << std::endl;
			return false;
		}
		if (returnCode == erc::ReturnCodeValue::RETCODE_OUT_OF_RESOURCES)
		{
			std::cerr << "RETCODE_OUT_OF_RESOURCES" << std::endl;
			return false;
		}
		if (returnCode == erc::ReturnCodeValue::RETCODE_NOT_ENABLED)
		{
			std::cerr << "RETCODE_NOT_ENABLED" << std::endl;
			return false;
		}
		if (returnCode == erc::ReturnCodeValue::RETCODE_IMMUTABLE_POLICY)
		{
			std::cerr << "RETCODE_IMMUTABLE_POLICY" << std::endl;
			return false;
		}
		if (returnCode == erc::ReturnCodeValue::RETCODE_INCONSISTENT_POLICY)
		{
			std::cerr << "RETCODE_INCONSISTENT_POLICY" << std::endl;
			return false;
		}
		if (returnCode == erc::ReturnCodeValue::RETCODE_ALREADY_DELETED)
		{
			std::cerr << "RETCODE_ALREADY_DELETED" << std::endl;
			return false;
		}
		if (returnCode == erc::ReturnCodeValue::RETCODE_TIMEOUT)
		{
			std::cerr << "RETCODE_TIMEOUT" << std::endl;
			return false;
		}
		if (returnCode == erc::ReturnCodeValue::RETCODE_NO_DATA)
		{
			std::cerr << "RETCODE_NO_DATA" << std::endl;
			return false;
		}
		if (returnCode == erc::ReturnCodeValue::RETCODE_ILLEGAL_OPERATION)
		{
			std::cerr << "RETCODE_ILLEGAL_OPERATION" << std::endl;
			return false;
		}
		if (returnCode == erc::ReturnCodeValue::RETCODE_NOT_ALLOWED_BY_SECURITY)
		{
			std::cerr << "RETCODE_NOT_ALLOWED_BY_SECURITY" << std::endl;
			return false;
		}
		std::cerr << "UNKNOWN" << std::endl;
		return false;
	}

	void CarlaOdometryPublisher::SetData(
		int32_t seconds,
		uint32_t nanoseconds,
		const float *location,
		const float *rotation,
		const float *linear_velocity,
		const float *angular_velocity)
	{
		if (!location || !rotation || !linear_velocity || !angular_velocity)
		{
			return;
		}

		// 1. 时间戳与 Header
		builtin_interfaces::msg::Time time;
		time.sec(seconds);
		time.nanosec(nanoseconds);

		std_msgs::msg::Header header;
		header.stamp(std::move(time));
		header.frame_id(_header_frame_id.empty() ? _parent : _header_frame_id);   // 上层坐标系

		// 2. 位姿（Pose）——从 UE 的位置和欧拉角计算
		const float tx = location[0];
		const float ty = location[1];
		const float tz = location[2];

		// UE 的旋转：roll/pitch 需要符号翻转，单位从度转为弧度
		const float rx = (-rotation[0]) * (static_cast<float>(M_PI) / 180.0f);
		const float ry = (-rotation[1]) * (static_cast<float>(M_PI) / 180.0f);
		const float rz = (rotation[2]) * (static_cast<float>(M_PI) / 180.0f);

		const float cr = std::cos(rz * 0.5f);
		const float sr = std::sin(rz * 0.5f);
		const float cp = std::cos(rx * 0.5f);
		const float sp = std::sin(rx * 0.5f);
		const float cy = std::cos(ry * 0.5f);
		const float sy = std::sin(ry * 0.5f);

		geometry_msgs::msg::Vector3 translation;
		translation.x(tx);
		translation.y(-ty);  // 与 TF 中的转换保持一致
		translation.z(tz);

		geometry_msgs::msg::Quaternion rotationQuat;
		rotationQuat.w(cr * cp * cy + sr * sp * sy);
		rotationQuat.x(sr * cp * cy - cr * sp * sy);
		rotationQuat.y(cr * sp * cy + sr * cp * sy);
		rotationQuat.z(cr * cp * sy - sr * sp * cy);

		geometry_msgs::msg::Pose pose;
		pose.position().x(translation.x());
		pose.position().y(translation.y());
		pose.position().z(translation.z());
		pose.orientation(rotationQuat);

		geometry_msgs::msg::PoseWithCovariance poseWithCov;
		poseWithCov.pose(pose);
		// 协方差这里先全部置零，后续如果需要可以在 UE 侧计算并填充
		geometry_msgs::msg::geometry_msgs__PoseWithCovariance__double_array_36 poseCov {};
		std::memset(&poseCov, 0, sizeof(poseCov));
		poseWithCov.covariance(poseCov);

		// 3. 速度（Twist）：线速度、角速度
		geometry_msgs::msg::Vector3 linearVel;
		linearVel.x(linear_velocity[0]);
		linearVel.y(linear_velocity[1]);
		linearVel.z(linear_velocity[2]);

		geometry_msgs::msg::Vector3 angularVel;
		angularVel.x(angular_velocity[0]);
		angularVel.y(angular_velocity[1]);
		angularVel.z(angular_velocity[2]);

		geometry_msgs::msg::Twist twist;
		twist.linear(linearVel);
		twist.angular(angularVel);

		geometry_msgs::msg::TwistWithCovariance twistWithCov;
		twistWithCov.twist(twist);
		geometry_msgs::msg::geometry_msgs__TwistWithCovariance__double_array_36 twistCov {};
		std::memset(&twistCov, 0, sizeof(twistCov));
		twistWithCov.covariance(twistCov);

		// 4. 写入 Odometry 消息
		_impl->m_Odometry.header(std::move(header));
		_impl->m_Odometry.child_frame_id(_frame_id);
		_impl->m_Odometry.pose(poseWithCov);
		_impl->m_Odometry.twist(twistWithCov);
	}

	CarlaOdometryPublisher::CarlaOdometryPublisher(const char *ros_name, const char *parent)
		: _impl(std::make_shared<CarlaOdometryPublisherImpl>()), _header_frame_id(parent ? parent : "")
	{
		_name = ros_name;
		_parent = parent;
	}

	void CarlaOdometryPublisher::SetHeaderFrameId(const std::string &frame_id)
	{
		_header_frame_id = frame_id;
	}

	void CarlaOdometryPublisher::SetChildFrameId(const std::string &frame_id)
	{
		_frame_id = frame_id;
	}

	CarlaOdometryPublisher::~CarlaOdometryPublisher()
	{
		if (!_impl)
		{
			return;
		}

		if (_impl->m_DataWriter)
		{
			_impl->m_Publisher->delete_datawriter(_impl->m_DataWriter);
		}

		if (_impl->m_Publisher)
		{
			_impl->m_Participant->delete_publisher(_impl->m_Publisher);
		}

		if (_impl->m_Topic)
		{
			_impl->m_Participant->delete_topic(_impl->m_Topic);
		}

		if (_impl->m_Participant)
		{
			efd::DomainParticipantFactory::get_instance()->delete_participant(_impl->m_Participant);
		}
	}

	CarlaOdometryPublisher::CarlaOdometryPublisher(const CarlaOdometryPublisher &other)
	{
		_frame_id = other._frame_id;
		_name = other._name;
		_parent = other._parent;
		_impl = other._impl;
	}

	CarlaOdometryPublisher &CarlaOdometryPublisher::operator=(const CarlaOdometryPublisher &other)
	{
		_frame_id = other._frame_id;
		_name = other._name;
		_parent = other._parent;
		_impl = other._impl;

		return *this;
	}

	CarlaOdometryPublisher::CarlaOdometryPublisher(CarlaOdometryPublisher &&other)
	{
		_frame_id = std::move(other._frame_id);
		_name = std::move(other._name);
		_parent = std::move(other._parent);
		_impl = std::move(other._impl);
	}

	CarlaOdometryPublisher &CarlaOdometryPublisher::operator=(CarlaOdometryPublisher &&other)
	{
		_frame_id = std::move(other._frame_id);
		_name = std::move(other._name);
		_parent = std::move(other._parent);
		_impl = std::move(other._impl);

		return *this;
	}
}
}


