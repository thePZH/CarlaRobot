#include "SVWheeledRobotControlSubscriber.h"

#include "carla/ros2/types/SVWheeledRobotControl.h"
#include "carla/ros2/types/SVWheeledRobotControlPubSubTypes.h"
#include "carla/ros2/listeners/SVSubscriberListener.h"

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

  struct SVWheeledRobotControlSubscriberImpl {
    efd::DomainParticipant* _participant { nullptr };
    efd::Subscriber* _subscriber { nullptr };
    efd::Topic* _topic { nullptr };
    efd::DataReader* _datareader { nullptr };
    efd::TypeSupport _type { new carla_msgs::msg::SVWheeledRobotControlPubSubType() };
    SVSubscriberListener _listener {nullptr};
    carla_msgs::msg::SVWheeledRobotControl _event {};
    VehicleControl _control {};
    bool _new_message {false};
    bool _alive {true};
    void* _vehicle {nullptr};
  };

  bool SVWheeledRobotControlSubscriber::Init() {
    std::cout << "[Init] SVWheeledRobotControlSubscriber::Init() called." << std::endl;
    if (_impl->_type == nullptr) {
        std::cerr << "[Error] Invalid TypeSupport (_type is nullptr)" << std::endl;
        return false;
    }
  	std::cout << "[Init] Creating DomainParticipant on domain ID = 6" << std::endl;
    efd::DomainParticipantQos pqos = efd::PARTICIPANT_QOS_DEFAULT;
    pqos.name(_name);
    auto factory = efd::DomainParticipantFactory::get_instance();
    _impl->_participant = factory->create_participant(6, pqos);
    if (_impl->_participant == nullptr) {
        std::cerr << "[Error] Failed to create DomainParticipant" << std::endl;
        return false;
    }
  	std::cout << "[Init] DomainParticipant created successfully. Name = " << pqos.name() << std::endl;

  	std::cout << "[Init] Registering TypeSupport: " << _impl->_type->getName() << std::endl;
    _impl->_type.register_type(_impl->_participant);
  	std::cout << "[Init] Type registered successfully." << std::endl;
  	
	std::cout << "[Init] Creating Subscriber..." << std::endl;
    efd::SubscriberQos subqos = efd::SUBSCRIBER_QOS_DEFAULT;
    _impl->_subscriber = _impl->_participant->create_subscriber(subqos, nullptr);
    if (_impl->_subscriber == nullptr) {
      std::cerr << "Failed to create Subscriber" << std::endl;
      return false;
    }
  	std::cout << "[Init] Subscriber created successfully." << std::endl;

    efd::TopicQos tqos = efd::TOPIC_QOS_DEFAULT;
    const std::string base { "rt/carla/" };
    const std::string publisher_type {"/robot_control_cmd"};
    std::string topic_name = base;
    if (!_parent.empty())
      topic_name += _parent + "/";
    topic_name += _name;
    topic_name += publisher_type;

  	std::cout << "[Init] Constructed Topic Name: " << topic_name << std::endl;
  	std::cout << "[Init] Type Name: " << _impl->_type->getName() << std::endl;

    _impl->_topic = _impl->_participant->create_topic(topic_name, _impl->_type->getName(), tqos);
    if (_impl->_topic == nullptr) {
        std::cerr << "Failed to create Topic" << std::endl;
        return false;
    } else {
    	std::cout << "Created topic: " << topic_name << std::endl;
    }

    efd::DataReaderQos rqos = efd::DATAREADER_QOS_DEFAULT;
  	std::cout << "[Init] Creating DataReader with QoS: RELIABLE + VOLATILE" << std::endl;

    efd::DataReaderListener* listener = (efd::DataReaderListener*)_impl->_listener._impl.get();
    _impl->_datareader = _impl->_subscriber->create_datareader(_impl->_topic, rqos, listener);
    if (_impl->_datareader == nullptr) {
        std::cerr << "[Error] Failed to create DataReader for topic: " << topic_name << std::endl;
        return false;
    }
  	std::cout << "[Init] DataReader created successfully." << std::endl;
  	std::cout << "[Init] SVWheeledRobotControlSubscriber::Init() completed successfully." << std::endl;    return true;
  }

  bool SVWheeledRobotControlSubscriber::Read() {
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

  void SVWheeledRobotControlSubscriber::ForwardMessage(VehicleControl control) {
    _impl->_control = control;
    _impl->_new_message = true;
  }

  void SVWheeledRobotControlSubscriber::DestroySubscriber() {
    _impl->_alive = false;
  }

  VehicleControl SVWheeledRobotControlSubscriber::GetMessage() {
    _impl->_new_message = false;
    return _impl->_control;
  }

  bool SVWheeledRobotControlSubscriber::IsAlive() {
    return _impl->_alive;
  }

  bool SVWheeledRobotControlSubscriber::HasNewMessage() {
    return _impl->_new_message;
  }

  void* SVWheeledRobotControlSubscriber::GetVehicle() {
    return _impl->_vehicle;
  }

  SVWheeledRobotControlSubscriber::SVWheeledRobotControlSubscriber(void* vehicle, const char* ros_name, const char* parent) :
  _impl(std::make_shared<SVWheeledRobotControlSubscriberImpl>()) {
    std::cout << "SVWheeledRobotControlSubscriber::SVWheeledRobotControlSubscriber() get called" << std::endl;
    _impl->_listener.SetOwner(this);
    _impl->_vehicle = vehicle;
    _name = ros_name;
    _parent = parent;
  }

  SVWheeledRobotControlSubscriber::~SVWheeledRobotControlSubscriber() {
  	std::cout << "SVWheeledRobotControlSubscriber::～SVWheeledRobotControlSubscriber() get called" << std::endl;

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

  SVWheeledRobotControlSubscriber::SVWheeledRobotControlSubscriber(const SVWheeledRobotControlSubscriber& other) {
    _frame_id = other._frame_id;
    _name = other._name;
    _parent = other._parent;
    _impl = other._impl;
    _impl->_listener.SetOwner(this);
  }

  SVWheeledRobotControlSubscriber& SVWheeledRobotControlSubscriber::operator=(const SVWheeledRobotControlSubscriber& other) {
    _frame_id = other._frame_id;
    _name = other._name;
    _parent = other._parent;
    _impl = other._impl;
    _impl->_listener.SetOwner(this);

    return *this;
  }

  SVWheeledRobotControlSubscriber::SVWheeledRobotControlSubscriber(SVWheeledRobotControlSubscriber&& other) {
    _frame_id = std::move(other._frame_id);
    _name = std::move(other._name);
    _parent = std::move(other._parent);
    _impl = std::move(other._impl);
    _impl->_listener.SetOwner(this);
  }

  SVWheeledRobotControlSubscriber& SVWheeledRobotControlSubscriber::operator=(SVWheeledRobotControlSubscriber&& other) {
    _frame_id = std::move(other._frame_id);
    _name = std::move(other._name);
    _parent = std::move(other._parent);
    _impl = std::move(other._impl);
    _impl->_listener.SetOwner(this);

    return *this;
  }
}
}
