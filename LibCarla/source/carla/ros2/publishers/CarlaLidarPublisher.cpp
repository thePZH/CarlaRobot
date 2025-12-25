#include "CarlaLidarPublisher.h"

#include <string>

#include "carla/ros2/types/PointCloud2PubSubTypes.h"
#include "carla/ros2/listeners/CarlaListener.h"

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/qos/PublisherQos.hpp>
#include <fastdds/dds/topic/qos/TopicQos.hpp>

#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/qos/QosPolicies.h>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>

namespace carla {
namespace ros2 {

  namespace efd = eprosima::fastdds::dds;
  using erc = eprosima::fastrtps::types::ReturnCode_t;

  struct CarlaLidarPublisherImpl {
    efd::DomainParticipant* _participant { nullptr };
    efd::Publisher* _publisher { nullptr };
    efd::Topic* _topic { nullptr };
    efd::DataWriter* _datawriter { nullptr };
    efd::TypeSupport _type { new sensor_msgs::msg::PointCloud2PubSubType() };
    CarlaListener _listener {};
    sensor_msgs::msg::PointCloud2 _lidar {};
  };

  bool CarlaLidarPublisher::Init() {
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

    efd::PublisherQos pubqos = efd::PUBLISHER_QOS_DEFAULT;
    _impl->_publisher = _impl->_participant->create_publisher(pubqos, nullptr);
    if (_impl->_publisher == nullptr) {
      std::cerr << "Failed to create Publisher" << std::endl;
      return false;
    }

    efd::TopicQos tqos = efd::TOPIC_QOS_DEFAULT;
    const std::string base { "rt/carla/" };
    std::string topic_name = base;
    if (!_parent.empty())
      topic_name += _parent + "/";
    topic_name += _name;
    _impl->_topic = _impl->_participant->create_topic(topic_name, _impl->_type->getName(), tqos);
    if (_impl->_topic == nullptr) {
        std::cerr << "Failed to create Topic" << std::endl;
        return false;
    }

    efd::DataWriterQos wqos = efd::DATAWRITER_QOS_DEFAULT;
    wqos.endpoint().history_memory_policy = eprosima::fastrtps::rtps::PREALLOCATED_WITH_REALLOC_MEMORY_MODE;
    efd::DataWriterListener* listener = (efd::DataWriterListener*)_impl->_listener._impl.get();
    _impl->_datawriter = _impl->_publisher->create_datawriter(_impl->_topic, wqos, listener);
    if (_impl->_datawriter == nullptr) {
        std::cerr << "Failed to create DataWriter" << std::endl;
        return false;
    }
    _frame_id = _name;
    return true;
  }

  bool CarlaLidarPublisher::Publish() {
    eprosima::fastrtps::rtps::InstanceHandle_t instance_handle;
    eprosima::fastrtps::types::ReturnCode_t rcode = _impl->_datawriter->write(&_impl->_lidar, instance_handle);
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


void CarlaLidarPublisher::SetData(int32_t seconds, uint32_t nanoseconds, size_t height, size_t width, float* data) {
    float* it = data;
    float* end = &data[height * width];
    for (++it; it < end; it += 4) {
        *it *= -1.0f;
    }
    std::vector<uint8_t> vector_data;
    const size_t size = height * width * sizeof(float);
    vector_data.resize(size);
    std::memcpy(&vector_data[0], &data[0], size);
    SetData(seconds, nanoseconds, height, width, std::move(vector_data));
  }

// data里面没有给ring和time，在这里进行估算
void CarlaLidarPublisher::SetDataWithRingAndTime(int32_t seconds, uint32_t nanoseconds, size_t height, size_t width, float* data,
                                                    const std::vector<uint32_t>& points_per_channel, float rotation_time) {
    // 计算总点数
    const size_t point_count = width / 4;
    
    // 新格式：x(4) + y(4) + z(4) + intensity(4) + ring(2) + padding(2) + time(4) = 24字节/点
    // 注意：ring后需要2字节padding以对齐到4字节边界，使time的offset为20
    const size_t new_point_size = 24; // 4+4+4+4+2+2+4 = 24字节
    const size_t new_data_size = point_count * new_point_size;
    std::vector<uint8_t> new_data;
    new_data.resize(new_data_size);
    
    // 构建ring映射：根据points_per_channel确定每个点属于哪个通道
    std::vector<uint16_t> ring_mapping;
    ring_mapping.reserve(point_count);
    for (size_t ch = 0; ch < points_per_channel.size(); ++ch) {
        for (uint32_t p = 0; p < points_per_channel[ch]; ++p) {
            ring_mapping.push_back(static_cast<uint16_t>(ch));
        }
    }
    
    // 计算每个点的时间戳（相对于消息时间戳的偏移）
    // 假设雷达旋转一周的时间为rotation_time秒，如果为0则使用默认值
    float scan_time = rotation_time;
    if (scan_time <= 0.0f) {
        // 默认值：假设64线雷达，10Hz，每圈0.1秒
        scan_time = 0.1f;
    }
    
    // 转换数据格式
    const float* src_ptr = data;
    uint8_t* dst_ptr = new_data.data();
    size_t point_idx = 0;
    
    for (size_t i = 0; i < point_count; ++i) {
        // 复制x, y, z, intensity（注意y坐标取反）
        float x = src_ptr[0];
        float y = -src_ptr[1]; // CARLA坐标系转ROS坐标系
        float z = src_ptr[2];
        float intensity = src_ptr[3];
        
        // 写入x
        std::memcpy(dst_ptr, &x, sizeof(float));
        dst_ptr += sizeof(float);
        
        // 写入y
        std::memcpy(dst_ptr, &y, sizeof(float));
        dst_ptr += sizeof(float);
        
        // 写入z
        std::memcpy(dst_ptr, &z, sizeof(float));
        dst_ptr += sizeof(float);
        
        // 写入intensity
        std::memcpy(dst_ptr, &intensity, sizeof(float));
        dst_ptr += sizeof(float);
        
        // 写入ring（UINT16）
        uint16_t ring = ring_mapping[point_idx];
        std::memcpy(dst_ptr, &ring, sizeof(uint16_t));
        dst_ptr += sizeof(uint16_t);
        
        // 添加2字节padding以对齐到4字节边界（使time的offset为20）
        uint16_t padding = 0;
        std::memcpy(dst_ptr, &padding, sizeof(uint16_t));
        dst_ptr += sizeof(uint16_t);
        
        // 写入time（FLOAT32）：计算相对于扫描开始的时间
        // 假设点按顺序扫描，时间均匀分布
        float point_time = (point_idx / static_cast<float>(point_count)) * scan_time;
        std::memcpy(dst_ptr, &point_time, sizeof(float));
        dst_ptr += sizeof(float);
        
        src_ptr += 4; // 移动到下一个点
        point_idx++;
    }
    
    SetDataWithRingAndTime(seconds, nanoseconds, height, width, std::move(new_data), points_per_channel, rotation_time);
}

// 最终写 ROS2 消息的函数。不关注 ring/time 的来源，只要拿到已经排好 24 字节一条的数据
void CarlaLidarPublisher::SetDataWithRingAndTimeFromUE(int32_t seconds, uint32_t nanoseconds, size_t height, size_t width, float* data,
                                                         const uint16_t* rings, const float* times) {
    // 计算总点数
    const size_t point_count = width / 4;
    
    // 新格式：x(4) + y(4) + z(4) + intensity(4) + ring(2) + padding(2) + time(4) = 24字节/点
    const size_t new_point_size = 24;
    const size_t new_data_size = point_count * new_point_size;
    std::vector<uint8_t> new_data;
    new_data.resize(new_data_size);
    
    // 转换数据格式，使用UE端提供的ring和time信息
    const float* src_ptr = data;
    uint8_t* dst_ptr = new_data.data();
    
    for (size_t i = 0; i < point_count; ++i) {
        // 复制x, y, z, intensity（注意y坐标取反）
        float x = src_ptr[0];
        float y = -src_ptr[1]; // CARLA坐标系转ROS坐标系
        float z = src_ptr[2];
        float intensity = src_ptr[3];
        
        // 写入x
        std::memcpy(dst_ptr, &x, sizeof(float));
        dst_ptr += sizeof(float);
        
        // 写入y
        std::memcpy(dst_ptr, &y, sizeof(float));
        dst_ptr += sizeof(float);
        
        // 写入z
        std::memcpy(dst_ptr, &z, sizeof(float));
        dst_ptr += sizeof(float);
        
        // 写入intensity
        std::memcpy(dst_ptr, &intensity, sizeof(float));
        dst_ptr += sizeof(float);
        
        // 写入ring（UINT16）- 使用UE端提供的数据
        uint16_t ring = rings[i];
        std::memcpy(dst_ptr, &ring, sizeof(uint16_t));
        dst_ptr += sizeof(uint16_t);
        
        // 添加2字节padding
        uint16_t padding = 0;
        std::memcpy(dst_ptr, &padding, sizeof(uint16_t));
        dst_ptr += sizeof(uint16_t);
        
        // 写入time（FLOAT32）- 使用UE端提供的数据
        float point_time = times[i];
        std::memcpy(dst_ptr, &point_time, sizeof(float));
        dst_ptr += sizeof(float);
        
        src_ptr += 4; // 移动到下一个点
    }
    
    // 调用内部方法设置数据（不需要points_per_channel，因为ring和time已经提供）
    std::vector<uint32_t> dummy_points_per_channel; // 空向量，不会被使用
    SetDataWithRingAndTime(seconds, nanoseconds, height, width, std::move(new_data), dummy_points_per_channel, 0.0f);
}

  void CarlaLidarPublisher::SetData(int32_t seconds, uint32_t nanoseconds, size_t height, size_t width, std::vector<uint8_t>&& data) {
    builtin_interfaces::msg::Time time;
    time.sec(seconds);
    time.nanosec(nanoseconds);

    std_msgs::msg::Header header;
    header.stamp(std::move(time));
    header.frame_id(std::string("rslidar"));

    sensor_msgs::msg::PointField descriptor1;
    descriptor1.name("x");
    descriptor1.offset(0);
    descriptor1.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor1.count(1);
    sensor_msgs::msg::PointField descriptor2;
    descriptor2.name("y");
    descriptor2.offset(4);
    descriptor2.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor2.count(1);
    sensor_msgs::msg::PointField descriptor3;
    descriptor3.name("z");
    descriptor3.offset(8);
    descriptor3.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor3.count(1);
    sensor_msgs::msg::PointField descriptor4;
    descriptor4.name("intensity");
    descriptor4.offset(12);
    descriptor4.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor4.count(1);

    const size_t point_size = 4 * sizeof(float);
    _impl->_lidar.header(std::move(header));
    _impl->_lidar.width(width / 4);
    _impl->_lidar.height(height);
    _impl->_lidar.is_bigendian(false);
    _impl->_lidar.fields({descriptor1, descriptor2, descriptor3, descriptor4});
    _impl->_lidar.point_step(point_size);
    _impl->_lidar.row_step(width * sizeof(float));
    _impl->_lidar.is_dense(false); //True if there are not invalid points
    _impl->_lidar.data(std::move(data));
  }

  void CarlaLidarPublisher::SetDataWithRingAndTime(int32_t seconds, uint32_t nanoseconds, size_t height, size_t width, std::vector<uint8_t>&& data,
                                                     const std::vector<uint32_t>& points_per_channel, float rotation_time) {
    builtin_interfaces::msg::Time time;
    time.sec(seconds);
    time.nanosec(nanoseconds);

    std_msgs::msg::Header header;
    header.stamp(std::move(time));
    header.frame_id(std::string("rslidar"));

    // 定义6个字段：x, y, z, intensity, ring, time
    sensor_msgs::msg::PointField descriptor1;
    descriptor1.name("x");
    descriptor1.offset(0);
    descriptor1.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor1.count(1);
    
    sensor_msgs::msg::PointField descriptor2;
    descriptor2.name("y");
    descriptor2.offset(4);
    descriptor2.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor2.count(1);
    
    sensor_msgs::msg::PointField descriptor3;
    descriptor3.name("z");
    descriptor3.offset(8);
    descriptor3.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor3.count(1);
    
    sensor_msgs::msg::PointField descriptor4;
    descriptor4.name("intensity");
    descriptor4.offset(12);
    descriptor4.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor4.count(1);
    
    sensor_msgs::msg::PointField descriptor5;
    descriptor5.name("ring");
    descriptor5.offset(16);
    descriptor5.datatype(sensor_msgs::msg::PointField__UINT16);
    descriptor5.count(1);
    
    sensor_msgs::msg::PointField descriptor6;
    descriptor6.name("time");
    descriptor6.offset(20); // 16(xyz+intensity) + 2(ring) + 2(padding) = 20
    descriptor6.datatype(sensor_msgs::msg::PointField__FLOAT32);
    descriptor6.count(1);

    const size_t point_size = 24; // 4+4+4+4+2+2(padding)+4 = 24字节
    const size_t point_count = data.size() / point_size;
    
    _impl->_lidar.header(std::move(header));
    _impl->_lidar.width(point_count);
    _impl->_lidar.height(height);
    _impl->_lidar.is_bigendian(false);
    _impl->_lidar.fields({descriptor1, descriptor2, descriptor3, descriptor4, descriptor5, descriptor6});
    _impl->_lidar.point_step(point_size);
    _impl->_lidar.row_step(point_count * point_size);
    _impl->_lidar.is_dense(true); // 包含ring和time字段，通常认为是dense的
    _impl->_lidar.data(std::move(data));
  }

  CarlaLidarPublisher::CarlaLidarPublisher(const char* ros_name, const char* parent) :
  _impl(std::make_shared<CarlaLidarPublisherImpl>()) {
    _name = ros_name;
    _parent = parent;
  }

  CarlaLidarPublisher::~CarlaLidarPublisher() {
      if (!_impl)
          return;

      if (_impl->_datawriter)
          _impl->_publisher->delete_datawriter(_impl->_datawriter);

      if (_impl->_publisher)
          _impl->_participant->delete_publisher(_impl->_publisher);

      if (_impl->_topic)
          _impl->_participant->delete_topic(_impl->_topic);

      if (_impl->_participant)
          efd::DomainParticipantFactory::get_instance()->delete_participant(_impl->_participant);
  }

  CarlaLidarPublisher::CarlaLidarPublisher(const CarlaLidarPublisher& other) {
    _frame_id = other._frame_id;
    _name = other._name;
    _parent = other._parent;
    _impl = other._impl;
  }

  CarlaLidarPublisher& CarlaLidarPublisher::operator=(const CarlaLidarPublisher& other) {
    _frame_id = other._frame_id;
    _name = other._name;
    _parent = other._parent;
    _impl = other._impl;

    return *this;
  }

  CarlaLidarPublisher::CarlaLidarPublisher(CarlaLidarPublisher&& other) {
    _frame_id = std::move(other._frame_id);
    _name = std::move(other._name);
    _parent = std::move(other._parent);
    _impl = std::move(other._impl);
  }

  CarlaLidarPublisher& CarlaLidarPublisher::operator=(CarlaLidarPublisher&& other) {
    _frame_id = std::move(other._frame_id);
    _name = std::move(other._name);
    _parent = std::move(other._parent);
    _impl = std::move(other._impl);

    return *this;
  }
}
}
