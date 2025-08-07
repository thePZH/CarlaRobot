#include "SVGimbalMovementSubscriber.h"
#include "carla/ros2/types/Twist.h"
#include "carla/ros2/types/TwistPubSubTypes.h"
#include "carla/ros2/ROS2CallbackData.h"
#include "carla/ros2/listeners/SVGimbalMovementListener.h"

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>

#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/qos/SubscriberQos.hpp>
#include <fastdds/dds/topic/qos/TopicQos.hpp>

#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/qos/QosPolicies.h>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>

namespace carla {
namespace ros2 {

  namespace efd = eprosima::fastdds::dds;
  using erc = eprosima::fastrtps::types::ReturnCode_t;

  struct SVGimbalMovementSubscriberImpl {
    efd::DomainParticipant* _participant { nullptr };
    efd::Subscriber* _subscriber { nullptr };
    efd::Topic* _topic { nullptr };
    efd::DataReader* _datareader { nullptr };
    // 修改：使用 TwistPubSubType 替代自定义类型
    efd::TypeSupport _type { new geometry_msgs::msg::TwistPubSubType() };
    SVGimbalMovementListener _listener {nullptr};
    // 修改：存储 Twist 消息
    geometry_msgs::msg::Twist _event {};
    GimbalRotation _control {};
    bool _new_message {false};
    bool _alive {true};
    void* _vehicle {nullptr};
  };

  bool SVGimbalMovementSubscriber::Init() {
    std::cout << "TwistSubscriber::Init()" << std::endl;
    if (_impl->_type == nullptr) {
        std::cerr << "Invalid TypeSupport" << std::endl;
        return false;
    }

    efd::DomainParticipantQos pqos = efd::PARTICIPANT_QOS_DEFAULT;
    pqos.name(_name);
    auto factory = efd::DomainParticipantFactory::get_instance();
    _impl->_participant = factory->create_participant(6, pqos);
    if (_impl->_participant == nullptr) {
        std::cerr << "Failed to create DomainParticipant" << std::endl;
        return false;
    }
    _impl->_type.register_type(_impl->_participant);

    efd::SubscriberQos subqos = efd::SUBSCRIBER_QOS_DEFAULT;
    _impl->_subscriber = _impl->_participant->create_subscriber(subqos, nullptr);
    if (_impl->_subscriber == nullptr) {
      std::cerr << "Failed to create Subscriber" << std::endl;
      return false;
    }

    efd::TopicQos tqos = efd::TOPIC_QOS_DEFAULT;
    const std::string base { "rt/carla/" };
    const std::string publisher_type {"/gimbal_control_cmd"};
    std::string topic_name = base;
    if (!_parent.empty())
      topic_name += _parent + "/";
    topic_name += _name;
    topic_name += publisher_type;
    _impl->_topic = _impl->_participant->create_topic(topic_name, _impl->_type->getName(), tqos);
    if (_impl->_topic == nullptr) {
        std::cerr << "Failed to create Topic: " << topic_name << std::endl;
        return false;
    } else {
        std::cout << "Created topic: " << topic_name << std::endl;
    }

    efd::DataReaderQos rqos = efd::DATAREADER_QOS_DEFAULT;
  	std::cout << "[Init] Creating DataReader with QoS: RELIABLE + VOLATILE" << std::endl;

    efd::DataReaderListener* listener = (efd::DataReaderListener*)_impl->_listener._impl.get();
    _impl->_datareader = _impl->_subscriber->create_datareader(_impl->_topic, rqos, listener);
    if (_impl->_datareader == nullptr) {
        std::cerr << "Failed to create DataReader" << std::endl;
        return false;
    }
    
    std::cout << "TwistSubscriber::Init() Success" << std::endl;
    return true;
  }

  bool SVGimbalMovementSubscriber::Read() {
    efd::SampleInfo info;
    eprosima::fastrtps::types::ReturnCode_t rcode = _impl->_datareader->take_next_sample(&_impl->_event, &info);
    if (rcode == erc::ReturnCodeValue::RETCODE_OK) {
        return true;
    }
    if (rcode == erc::ReturnCodeValue::RETCODE_ERROR) {
        std::cerr << "RETCODE_ERROR" << std::endl;
        return false;
    }
    if (rcode == erc::ReturnCodeValue::RETCODE_UNSUPPORTED) {
        std::cerr << "RETCODE_UNSUPPORTED" << std::endl;
        return false;
    }
    if (rcode == erc::ReturnCodeValue::RETCODE_BAD_PARAMETER) {
        std::cerr << "RETCODE_BAD_PARAMETER" << std::endl;
        return false;
    }
    if (rcode == erc::ReturnCodeValue::RETCODE_PRECONDITION_NOT_MET) {
        std::cerr << "RETCODE_PRECONDITION_NOT_MET" << std::endl;
        return false;
    }
    if (rcode == erc::ReturnCodeValue::RETCODE_OUT_OF_RESOURCES) {
        std::cerr << "RETCODE_OUT_OF_RESOURCES" << std::endl;
        return false;
    }
    if (rcode == erc::ReturnCodeValue::RETCODE_NOT_ENABLED) {
        std::cerr << "RETCODE_NOT_ENABLED" << std::endl;
        return false;
    }
    if (rcode == erc::ReturnCodeValue::RETCODE_IMMUTABLE_POLICY) {
        std::cerr << "RETCODE_IMMUTABLE_POLICY" << std::endl;
        return false;
    }
    if (rcode == erc::ReturnCodeValue::RETCODE_INCONSISTENT_POLICY) {
        std::cerr << "RETCODE_INCONSISTENT_POLICY" << std::endl;
        return false;
    }
    if (rcode == erc::ReturnCodeValue::RETCODE_ALREADY_DELETED) {
        std::cerr << "RETCODE_ALREADY_DELETED" << std::endl;
        return false;
    }
    if (rcode == erc::ReturnCodeValue::RETCODE_TIMEOUT) {
        std::cerr << "RETCODE_TIMEOUT" << std::endl;
        return false;
    }
    if (rcode == erc::ReturnCodeValue::RETCODE_NO_DATA) {
        std::cerr << "RETCODE_NO_DATA" << std::endl;
        return false;
    }
    if (rcode == erc::ReturnCodeValue::RETCODE_ILLEGAL_OPERATION) {
        std::cerr << "RETCODE_ILLEGAL_OPERATION" << std::endl;
        return false;
    }
    if (rcode == erc::ReturnCodeValue::RETCODE_NOT_ALLOWED_BY_SECURITY) {
        std::cerr << "RETCODE_NOT_ALLOWED_BY_SECURITY" << std::endl;
        return false;
    }
    std::cerr << "UNKNOWN" << std::endl;
    return false;
  }

  void SVGimbalMovementSubscriber::ForwardMessage(GimbalRotation control) {
    _impl->_control = control;
    _impl->_new_message = true;
  }

  void SVGimbalMovementSubscriber::DestroySubscriber() {
    _impl->_alive = false;
  }

  GimbalRotation SVGimbalMovementSubscriber::GetMessage() {
    _impl->_new_message = false;
    return _impl->_control;
  }

  bool SVGimbalMovementSubscriber::IsAlive() {
    return _impl->_alive;
  }

  bool SVGimbalMovementSubscriber::HasNewMessage() {
    return _impl->_new_message;
  }

  void* SVGimbalMovementSubscriber::GetVehicle() {
    return _impl->_vehicle;
  }

  SVGimbalMovementSubscriber::SVGimbalMovementSubscriber(void* vehicle, const char* ros_name, const char* parent) :
  _impl(std::make_shared<SVGimbalMovementSubscriberImpl>()) {
    _impl->_listener.SetOwner(this);
    _impl->_vehicle = vehicle;
    _name = ros_name;
    _parent = parent;
  }

  SVGimbalMovementSubscriber::~SVGimbalMovementSubscriber() {
      if (!_impl)
          return;

      if (_impl->_datareader)
          _impl->_subscriber->delete_datareader(_impl->_datareader);

      if (_impl->_subscriber)
          _impl->_participant->delete_subscriber(_impl->_subscriber);

      if (_impl->_topic)
          _impl->_participant->delete_topic(_impl->_topic);

      if (_impl->_participant)
          efd::DomainParticipantFactory::get_instance()->delete_participant(_impl->_participant);
  }

  SVGimbalMovementSubscriber::SVGimbalMovementSubscriber(const SVGimbalMovementSubscriber& other) {
    _frame_id = other._frame_id;
    _name = other._name;
    _parent = other._parent;
    _impl = other._impl;
    _impl->_listener.SetOwner(this);
  }

  SVGimbalMovementSubscriber& SVGimbalMovementSubscriber::operator=(const SVGimbalMovementSubscriber& other) {
    _frame_id = other._frame_id;
    _name = other._name;
    _parent = other._parent;
    _impl = other._impl;
    _impl->_listener.SetOwner(this);

    return *this;
  }

  SVGimbalMovementSubscriber::SVGimbalMovementSubscriber(SVGimbalMovementSubscriber&& other) {
    _frame_id = std::move(other._frame_id);
    _name = std::move(other._name);
    _parent = std::move(other._parent);
    _impl = std::move(other._impl);
    _impl->_listener.SetOwner(this);
  }

  SVGimbalMovementSubscriber& SVGimbalMovementSubscriber::operator=(SVGimbalMovementSubscriber&& other) {
    _frame_id = std::move(other._frame_id);
    _name = std::move(other._name);
    _parent = std::move(other._parent);
    _impl = std::move(other._impl);
    _impl->_listener.SetOwner(this);

    return *this;
  }
}
}