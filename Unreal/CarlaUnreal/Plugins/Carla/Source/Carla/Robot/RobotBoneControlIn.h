#pragma once
#include "RobotBoneControlIn.generated.h"

USTRUCT(BlueprintType)
struct CARLA_API FRobotBoneControlIn
{
	GENERATED_BODY()
	// 相对父骨骼的transform
	UPROPERTY(Category = "Robot Bone Control", EditAnywhere, BlueprintReadWrite)
	TMap<FString, FTransform> BoneTransforms;
};
