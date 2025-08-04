#ifndef _FAST_DDS_GENERATED_CARLA_MSGS_MSG_ROBOTCONTROL_H_
#define _FAST_DDS_GENERATED_CARLA_MSGS_MSG_ROBOTCONTROL_H_

#include "Header.h"

#include <fastrtps/utils/fixed_size_string.hpp>
#include "carla/ros2/types/Vector3.h"

#include <stdint.h>
#include <array>
#include <string>
#include <vector>
#include <map>
#include <bitset>

#if defined(_WIN32)
#if defined(EPROSIMA_USER_DLL_EXPORT)
#define eProsima_user_DllExport __declspec( dllexport )
#else
#define eProsima_user_DllExport
#endif  // EPROSIMA_USER_DLL_EXPORT
#else
#define eProsima_user_DllExport
#endif  // _WIN32

#if defined(_WIN32)
#if defined(EPROSIMA_USER_DLL_EXPORT)
#if defined(CarlaEgoSVWheeledRobotControl_SOURCE)
#define CarlaEgoSVWheeledRobotControl_DllAPI __declspec( dllexport )
#else
#define CarlaEgoSVWheeledRobotControl_DllAPI __declspec( dllimport )
#endif // CarlaEgoSVWheeledRobotControl_SOURCE
#else
#define CarlaEgoSVWheeledRobotControl_DllAPI
#endif  // EPROSIMA_USER_DLL_EXPORT
#else
#define CarlaEgoSVWheeledRobotControl_DllAPI
#endif // _WIN32

namespace eprosima {
namespace fastcdr {
class Cdr;
} // namespace fastcdr
} // namespace eprosima

namespace carla_msgs {
    namespace msg {
        class SVWheeledRobotControl
        {
        public:
            eProsima_user_DllExport SVWheeledRobotControl();
            eProsima_user_DllExport ~SVWheeledRobotControl();
            eProsima_user_DllExport SVWheeledRobotControl(const SVWheeledRobotControl& x);
            eProsima_user_DllExport SVWheeledRobotControl(SVWheeledRobotControl&& x) noexcept;
            eProsima_user_DllExport SVWheeledRobotControl& operator =(const SVWheeledRobotControl& x);
            eProsima_user_DllExport SVWheeledRobotControl& operator =(SVWheeledRobotControl&& x) noexcept;
            eProsima_user_DllExport bool operator ==(const SVWheeledRobotControl& x) const;
            eProsima_user_DllExport bool operator !=(const SVWheeledRobotControl& x) const;
            
        	// Header functions
        	eProsima_user_DllExport void header(const std_msgs::msg::Header& _header);
        	eProsima_user_DllExport void header(std_msgs::msg::Header&& _header);
        	eProsima_user_DllExport const std_msgs::msg::Header& header() const;
        	eProsima_user_DllExport std_msgs::msg::Header& header();
            
        	// Linear vector functions
        	eProsima_user_DllExport void linear(const geometry_msgs::msg::Vector3& _linear);
        	eProsima_user_DllExport void linear(geometry_msgs::msg::Vector3&& _linear);
        	eProsima_user_DllExport const geometry_msgs::msg::Vector3& linear() const;
        	eProsima_user_DllExport geometry_msgs::msg::Vector3& linear();
            
        	// Angular vector functions
        	eProsima_user_DllExport void angular(const geometry_msgs::msg::Vector3& _angular);
        	eProsima_user_DllExport void angular(geometry_msgs::msg::Vector3&& _angular);
        	eProsima_user_DllExport const geometry_msgs::msg::Vector3& angular() const;
        	eProsima_user_DllExport geometry_msgs::msg::Vector3& angular();
            
            
            // CDR serialization
            eProsima_user_DllExport static size_t getMaxCdrSerializedSize(size_t current_alignment = 0);
            eProsima_user_DllExport static size_t getCdrSerializedSize(const carla_msgs::msg::SVWheeledRobotControl& data, size_t current_alignment = 0);
            eProsima_user_DllExport void serialize(eprosima::fastcdr::Cdr& cdr) const;
            eProsima_user_DllExport void deserialize(eprosima::fastcdr::Cdr& cdr);
            eProsima_user_DllExport static size_t getKeyMaxCdrSerializedSize(size_t current_alignment = 0);
            eProsima_user_DllExport static bool isKeyDefined();
            eProsima_user_DllExport void serializeKey(eprosima::fastcdr::Cdr& cdr) const;
            
        private:
            std_msgs::msg::Header m_header;
        	geometry_msgs::msg::Vector3 m_linear;  // 线速度 (m/s)
        	geometry_msgs::msg::Vector3 m_angular;
        };
    } // namespace msg
} // namespace carla_msgs

#endif // _FAST_DDS_GENERATED_CARLA_MSGS_MSG_CARLAEGOSVWheeledRobotControl_H_
