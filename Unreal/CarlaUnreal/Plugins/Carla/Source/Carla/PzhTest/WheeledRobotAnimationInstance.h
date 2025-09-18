// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "CoreMinimal.h"
#include "VehicleAnimationInstance.h"
#include "WheeledRobotAnimationInstance.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class CARLA_API UWheeledRobotAnimationInstance : public UVehicleAnimationInstance
{
	GENERATED_BODY()
public:
	UPROPERTY(Blueprintreadwrite, EditAnywhere)
	float CameraPitch;
	
	UPROPERTY(Blueprintreadwrite, EditAnywhere)
	float Gimbalyaw;
	
	// UPROPERTY(Blueprintreadwrite, EditAnywhere)
	// FPoseSnapshot Snap;
	//
	// UPROPERTY(Blueprintreadwrite, EditAnywhere)
	// bool bUseSnapshot = false;
	//
	// virtual void NativePostEvaluateAnimation() override;
	void SetBonesTransform(const TMap<FName, FTransform>& InTransforms)
	{
		for (auto it = InTransforms.begin(); it != InTransforms.end(); ++it)
		{
			BoneNames.Emplace(it.Key());
			BoneTransforms.Emplace(it.Value());
		}
	}
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TArray<FName> BoneNames;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TArray<FTransform> BoneTransforms;

};
