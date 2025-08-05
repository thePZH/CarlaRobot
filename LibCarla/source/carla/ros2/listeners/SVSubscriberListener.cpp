#include "SVSubscriberListener.h"
#include <iostream>

#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/core/status/SubscriptionMatchedStatus.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include "carla/ros2/types/SVWheeledRobotControl.h"
#include "carla/ros2/subscribers/SVWheeledRobotControlSubscriber.h"
#include "carla/ros2/ROS2CallbackData.h"

namespace carla {
namespace ros2 {

  namespace efd = eprosima::fastdds::dds;
  using erc = eprosima::fastrtps::types::ReturnCode_t;

    class SVSubscriberListenerImpl : public efd::DataReaderListener {
      public:
      void on_subscription_matched(
              efd::DataReader* reader,
              const efd::SubscriptionMatchedStatus& info) override;
      void on_data_available(efd::DataReader* reader) override;

      VehicleControl convert_to_vehicle(const carla_msgs::msg::SVWheeledRobotControl& message);

      int _matched {0};
      bool _first_connected {false};
      SVWheeledRobotControlSubscriber* _owner {nullptr};
      carla_msgs::msg::SVWheeledRobotControl _message {};
    };

    void SVSubscriberListenerImpl::on_subscription_matched(efd::DataReader* reader, const efd::SubscriptionMatchedStatus& info)
    {
    	std::cout << "[Callback] on_subscription_matched. Current count: "
						  << info.current_count << ", Total count: " << info.total_count << std::endl;
      if (info.current_count_change == 1) {
          _matched = info.total_count;
          _first_connected = true;
      } else if (info.current_count_change == -1) {
          _matched = info.total_count;
          if (_matched == 0) {
            _owner->DestroySubscriber();
          }
      } else {
          std::cerr << info.current_count_change
                  << " is not a valid value for PublicationMatchedStatus current count change" << std::endl;
      }
    }

    VehicleControl SVSubscriberListenerImpl::convert_to_vehicle(const carla_msgs::msg::SVWheeledRobotControl& message)
	{
    	VehicleControl control;
    	control.throttle = message.linear().x();	// 油门
    	control.steer = message.angular().z(); 	// yaw
		control.brake = message.linear().x() == 0 ? true : false;
        control.reverse = message.linear().x() < 0 ? true : false;
		return control;	
	}
    void SVSubscriberListenerImpl::on_data_available(efd::DataReader* reader)
    {
		std::cout << "ROS2 msg get" << std::endl;
      efd::SampleInfo info;
      eprosima::fastrtps::types::ReturnCode_t rcode = reader->take_next_sample(&_message, &info);
      if (rcode == erc::ReturnCodeValue::RETCODE_OK) {
      	std::cout << "ROS2 msg available!!!" << std::endl;
        VehicleControl control = convert_to_vehicle(_message);
        _owner->ForwardMessage(control);
      }
      if (rcode == erc::ReturnCodeValue::RETCODE_ERROR) {
          std::cerr << "RETCODE_ERROR" << std::endl;
      }
      if (rcode == erc::ReturnCodeValue::RETCODE_UNSUPPORTED) {
          std::cerr << "RETCODE_UNSUPPORTED" << std::endl;
      }
      if (rcode == erc::ReturnCodeValue::RETCODE_BAD_PARAMETER) {
          std::cerr << "RETCODE_BAD_PARAMETER" << std::endl;
      }
      if (rcode == erc::ReturnCodeValue::RETCODE_PRECONDITION_NOT_MET) {
          std::cerr << "RETCODE_PRECONDITION_NOT_MET" << std::endl;
      }
      if (rcode == erc::ReturnCodeValue::RETCODE_OUT_OF_RESOURCES) {
          std::cerr << "RETCODE_OUT_OF_RESOURCES" << std::endl;
      }
      if (rcode == erc::ReturnCodeValue::RETCODE_NOT_ENABLED) {
          std::cerr << "RETCODE_NOT_ENABLED" << std::endl;
      }
      if (rcode == erc::ReturnCodeValue::RETCODE_IMMUTABLE_POLICY) {
          std::cerr << "RETCODE_IMMUTABLE_POLICY" << std::endl;
      }
      if (rcode == erc::ReturnCodeValue::RETCODE_INCONSISTENT_POLICY) {
          std::cerr << "RETCODE_INCONSISTENT_POLICY" << std::endl;
      }
      if (rcode == erc::ReturnCodeValue::RETCODE_ALREADY_DELETED) {
          std::cerr << "RETCODE_ALREADY_DELETED" << std::endl;
      }
      if (rcode == erc::ReturnCodeValue::RETCODE_TIMEOUT) {
          std::cerr << "RETCODE_TIMEOUT" << std::endl;
      }
      if (rcode == erc::ReturnCodeValue::RETCODE_NO_DATA) {
          std::cerr << "RETCODE_NO_DATA" << std::endl;
      }
      if (rcode == erc::ReturnCodeValue::RETCODE_ILLEGAL_OPERATION) {
          std::cerr << "RETCODE_ILLEGAL_OPERATION" << std::endl;
      }
      if (rcode == erc::ReturnCodeValue::RETCODE_NOT_ALLOWED_BY_SECURITY) {
          std::cerr << "RETCODE_NOT_ALLOWED_BY_SECURITY" << std::endl;
      }
    }

    void SVSubscriberListener::SetOwner(SVWheeledRobotControlSubscriber* owner) {
        _impl->_owner = owner;
    }

    SVSubscriberListener::SVSubscriberListener(SVWheeledRobotControlSubscriber* owner) :
    _impl(std::make_unique<SVSubscriberListenerImpl>()) {
        _impl->_owner = owner;
    }

    SVSubscriberListener::~SVSubscriberListener() {}

}}
