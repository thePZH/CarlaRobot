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
namespace carla_msgs {
	namespace msg {
		SVWheeledRobotControl::SVWheeledRobotControl()
		{
			// 使用 setter 方法初始化分量
			m_linear.x(0.0);
			m_linear.y(0.0);
			m_linear.z(0.0);
			m_angular.x(0.0);
			m_angular.y(0.0);
			m_angular.z(0.0);
		}

		SVWheeledRobotControl::~SVWheeledRobotControl()
		{
		}

		SVWheeledRobotControl::SVWheeledRobotControl(const SVWheeledRobotControl& x)
		{
			m_header = x.m_header;
			m_linear = x.m_linear;
			m_angular = x.m_angular;
		}

		SVWheeledRobotControl::SVWheeledRobotControl(SVWheeledRobotControl&& x) noexcept
		{
			m_header = std::move(x.m_header);
			m_linear = std::move(x.m_linear);
			m_angular = std::move(x.m_angular);
		}

		SVWheeledRobotControl& SVWheeledRobotControl::operator=(const SVWheeledRobotControl& x)
		{
			m_header = x.m_header;
			m_linear = x.m_linear;
			m_angular = x.m_angular;
            
			return *this;
		}

		SVWheeledRobotControl& SVWheeledRobotControl::operator=(SVWheeledRobotControl&& x) noexcept
		{
			m_header = std::move(x.m_header);
			m_linear = std::move(x.m_linear);
			m_angular = std::move(x.m_angular);
            
			return *this;
		}

		bool SVWheeledRobotControl::operator==(const SVWheeledRobotControl& x) const
		{
			return (m_header == x.m_header &&
					m_linear.x() == x.m_linear.x() &&
					m_linear.y() == x.m_linear.y() &&
					m_linear.z() == x.m_linear.z() &&
					m_angular.x() == x.m_angular.x() &&
					m_angular.y() == x.m_angular.y() &&
					m_angular.z() == x.m_angular.z());
		}


		bool SVWheeledRobotControl::operator!=(const SVWheeledRobotControl& x) const
		{
			return !(*this == x);
		}

		// Header functions
		void SVWheeledRobotControl::header(const std_msgs::msg::Header& _header)
		{
			m_header = _header;
		}

		void SVWheeledRobotControl::header(std_msgs::msg::Header&& _header)
		{
			m_header = std::move(_header);
		}


		const std_msgs::msg::Header& SVWheeledRobotControl::header() const
		{
			return m_header;
		}

		std_msgs::msg::Header& SVWheeledRobotControl::header()
		{
			return m_header;
		}

		// Linear vector functions
		void SVWheeledRobotControl::linear(const geometry_msgs::msg::Vector3& _linear)
		{
			m_linear = _linear;
		}

		void SVWheeledRobotControl::linear(geometry_msgs::msg::Vector3&& _linear)
		{
			m_linear = std::move(_linear);
		}

		const geometry_msgs::msg::Vector3& SVWheeledRobotControl::linear() const
		{
			return m_linear;
		}

		geometry_msgs::msg::Vector3& SVWheeledRobotControl::linear()
		{
			return m_linear;
		}

		// Angular vector functions
		void SVWheeledRobotControl::angular(const geometry_msgs::msg::Vector3& _angular)
		{
			m_angular = _angular;
		}

		void SVWheeledRobotControl::angular(geometry_msgs::msg::Vector3&& _angular)
		{
			m_angular = std::move(_angular);
		}

		const geometry_msgs::msg::Vector3& SVWheeledRobotControl::angular() const
		{
			return m_angular;
		}

		geometry_msgs::msg::Vector3& SVWheeledRobotControl::angular()
		{
			return m_angular;
		}

		size_t SVWheeledRobotControl::getMaxCdrSerializedSize(size_t current_alignment)
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

			const size_t float_size = 4;
			const size_t float_align = 4;

			for (int i = 0; i < 6; ++i) {
				current_alignment += float_size + eprosima::fastcdr::Cdr::alignment(current_alignment, float_align);
			}

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

			dcdr >> m_linear.x();
			dcdr >> m_linear.y();
			dcdr >> m_linear.z();

			dcdr >> m_angular.x();
			dcdr >> m_angular.y();
			dcdr >> m_angular.z();
		}

		size_t getKeyMaxCdrSerializedSize(size_t current_alignment)
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
