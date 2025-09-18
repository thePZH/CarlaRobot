#pragma once

#include "carla/MsgPack.h"
#include "carla/rpc/BoneTransformDataOut.h"
#include "carla/rpc/String.h"
#include "carla/rpc/Transform.h"

#ifdef LIBCARLA_INCLUDED_FROM_UE4
#include <util/enable-ue4-macros.h>
#include "Carla/Robot/RobotBoneControlOut.h"
#include <util/disable-ue4-macros.h>
#endif // LIBCARLA_INCLUDED_FROM_UE4

#include <vector>

namespace carla {
	namespace rpc {

		class RobotBoneControlOut {
		public:

			RobotBoneControlOut() = default;

			explicit RobotBoneControlOut(std::vector<rpc::BoneTransformDataOut> bone_transforms)
			  : bone_transforms(bone_transforms) {}

			std::vector<rpc::BoneTransformDataOut> bone_transforms;

			MSGPACK_DEFINE_ARRAY(bone_transforms);	// 要走RPC
		};

	} // namespace rpc
} // namespace carla
