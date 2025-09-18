// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once
#include "RobotBoneControlOut.generated.h"

USTRUCT(BlueprintType)
struct CARLA_API FRobotBoneControlOutData
{
	GENERATED_BODY()
	FTransform World;
	FTransform Component;
	FTransform Relative;
};

USTRUCT(BlueprintType)
struct CARLA_API FRobotBoneControlOut
{
	GENERATED_BODY()

	UPROPERTY(Category = "Robot Bone Control", EditAnywhere, BlueprintReadWrite)
	TMap<FString, FRobotBoneControlOutData> BoneTransforms;
};