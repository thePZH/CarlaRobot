#ifdef _WIN32
// Remove linker warning LNK4221 on Visual Studio
namespace {
char dummy;
}  // namespace
#endif  // _WIN32

#include "SVWheeledRobotControl.h"
#include <fastcdr/Cdr.h>

#include <fastcdr/exceptions/BadParamException.h>
using namespace eprosima::fastcdr::exception;

#include <utility>

#define builtin_interfaces_msg_Time_max_cdr_typesize 8ULL;
#define std_msgs_msg_Header_max_cdr_typesize 268ULL;
#define carla_msgs_msg_SVWheeledRobotControl_max_cdr_typesize 300ULL;
#define builtin_interfaces_msg_Time_max_key_cdr_typesize 0ULL;
#define std_msgs_msg_Header_max_key_cdr_typesize 0ULL;
#define carla_msgs_msg_SVWheeledRobotControl_max_key_cdr_typesize 0ULL;

// 构造函数
		carla_msgs::msg::SVWheeledRobotControl::SVWheeledRobotControl()
		{
			// 使用 setter 方法初始化分量
			m_linear.x(0.0);
			m_linear.y(0.0);
			m_linear.z(0.0);
			m_angular.x(0.0);
			m_angular.y(0.0);
			m_angular.z(0.0);
		}

		carla_msgs::msg::SVWheeledRobotControl::~SVWheeledRobotControl()
		{
		}

		carla_msgs::msg::SVWheeledRobotControl::SVWheeledRobotControl(const SVWheeledRobotControl& x)
		{
			m_header = x.m_header;
			m_linear = x.m_linear;
			m_angular = x.m_angular;
		}

		carla_msgs::msg::SVWheeledRobotControl::SVWheeledRobotControl(SVWheeledRobotControl&& x) noexcept
		{
			m_header = std::move(x.m_header);
			m_linear = std::move(x.m_linear);
			m_angular = std::move(x.m_angular);
		}

		carla_msgs::msg::SVWheeledRobotControl& carla_msgs::msg::SVWheeledRobotControl::operator=(const SVWheeledRobotControl& x)
		{
			m_header = x.m_header;
			m_linear = x.linear();
			m_angular = x.angular();
            
			return *this;
		}

		carla_msgs::msg::SVWheeledRobotControl& carla_msgs::msg::SVWheeledRobotControl::operator=(SVWheeledRobotControl&& x) noexcept
		{
			m_header = std::move(x.m_header);
			m_linear = std::move(x.m_linear);
			m_angular = std::move(x.m_angular);
            
			return *this;
		}

		bool carla_msgs::msg::SVWheeledRobotControl::operator==(const SVWheeledRobotControl& x) const
		{
			return (m_header == x.m_header &&
					m_linear.x() == x.m_linear.x() &&
					m_linear.y() == x.m_linear.y() &&
					m_linear.z() == x.m_linear.z() &&
					m_angular.x() == x.m_angular.x() &&
					m_angular.y() == x.m_angular.y() &&
					m_angular.z() == x.m_angular.z());
		}


		bool carla_msgs::msg::SVWheeledRobotControl::operator!=(const SVWheeledRobotControl& x) const
		{
			return !(*this == x);
		}

		// Header functions
		void carla_msgs::msg::SVWheeledRobotControl::header(const std_msgs::msg::Header& _header)
		{
			m_header = _header;
		}

		void carla_msgs::msg::SVWheeledRobotControl::header(std_msgs::msg::Header&& _header)
		{
			m_header = std::move(_header);
		}


		const std_msgs::msg::Header& carla_msgs::msg::SVWheeledRobotControl::header() const
		{
			return m_header;
		}

		std_msgs::msg::Header& carla_msgs::msg::SVWheeledRobotControl::header()
		{
			return m_header;
		}

		// Linear vector functions
		void carla_msgs::msg::SVWheeledRobotControl::linear(const geometry_msgs::msg::Vector3& _linear)
		{
			m_linear = _linear;
		}

		void carla_msgs::msg::SVWheeledRobotControl::linear(geometry_msgs::msg::Vector3&& _linear)
		{
			m_linear = std::move(_linear);
		}

		const geometry_msgs::msg::Vector3& carla_msgs::msg::SVWheeledRobotControl::linear() const
		{
			return m_linear;
		}

		geometry_msgs::msg::Vector3& carla_msgs::msg::SVWheeledRobotControl::linear()
		{
			return m_linear;
		}

		// Angular vector functions
		void carla_msgs::msg::SVWheeledRobotControl::angular(const geometry_msgs::msg::Vector3& _angular)
		{
			m_angular = _angular;
		}

		void carla_msgs::msg::SVWheeledRobotControl::angular(geometry_msgs::msg::Vector3&& _angular)
		{
			m_angular = std::move(_angular);
		}

		const geometry_msgs::msg::Vector3& carla_msgs::msg::SVWheeledRobotControl::angular() const
		{
			return m_angular;
		}

		geometry_msgs::msg::Vector3& carla_msgs::msg::SVWheeledRobotControl::angular()
		{
			return m_angular;
		}

		size_t carla_msgs::msg::SVWheeledRobotControl::getMaxCdrSerializedSize(size_t current_alignment)
		{
			size_t initial_alignment = current_alignment;

			current_alignment += std_msgs::msg::Header::getMaxCdrSerializedSize(current_alignment);
			current_alignment += 3 * sizeof(double) + eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(double)); // linear
			current_alignment += 3 * sizeof(double) + eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(double)); // angular

			return current_alignment - initial_alignment;
		}
		// getCdrSerializedSize
		size_t carla_msgs::msg::SVWheeledRobotControl::getCdrSerializedSize(
				const carla_msgs::msg::SVWheeledRobotControl& data,
				size_t current_alignment)
		{
			size_t initial_alignment = current_alignment;

			current_alignment += std_msgs::msg::Header::getCdrSerializedSize(data.header(), current_alignment);

			current_alignment += std_msgs::msg::Header::getCdrSerializedSize(data.header(), current_alignment);
			current_alignment += 3 * sizeof(double) + eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(double)); // linear
			current_alignment += 3 * sizeof(double) + eprosima::fastcdr::Cdr::alignment(current_alignment, sizeof(double)); // angular

			return current_alignment - initial_alignment;
		}
		// serialize
		void carla_msgs::msg::SVWheeledRobotControl::serialize(
				eprosima::fastcdr::Cdr& scdr) const
		{
			scdr << m_header;

			scdr << m_linear.x();
			scdr << m_linear.y();
			scdr << m_linear.z();

			scdr << m_angular.x();
			scdr << m_angular.y();
			scdr << m_angular.z();
		}

		// deserialize
		void carla_msgs::msg::SVWheeledRobotControl::deserialize(
				eprosima::fastcdr::Cdr& dcdr)
		{
			dcdr >> m_header;

			// Deserialize linear vector components
			double x, y, z;
			dcdr >> x;
			dcdr >> y;
			dcdr >> z;
			m_linear.x(x);
			m_linear.y(y);
			m_linear.z(z);
    
			// Deserialize angular vector components
			dcdr >> x;
			dcdr >> y;
			dcdr >> z;
			m_angular.x(x);
			m_angular.y(y);
			m_angular.z(z);
		}

		size_t carla_msgs::msg::SVWheeledRobotControl::getKeyMaxCdrSerializedSize(size_t current_alignment)
		{
			static_cast<void>(current_alignment);
			return carla_msgs_msg_SVWheeledRobotControl_max_key_cdr_typesize;
		}

		bool carla_msgs::msg::SVWheeledRobotControl::isKeyDefined()
		{
		    return false;
		}

		void carla_msgs::msg::SVWheeledRobotControl::serializeKey(
		        eprosima::fastcdr::Cdr& scdr) const
		{
		    (void) scdr;
		}