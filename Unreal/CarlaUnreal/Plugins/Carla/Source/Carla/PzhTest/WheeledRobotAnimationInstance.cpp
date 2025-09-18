// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.


#include "WheeledRobotAnimationInstance.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"



// void UWheeledRobotAnimationInstance::NativePostEvaluateAnimation()
// {
// 	Super::NativePostEvaluateAnimation();
//     
// 	if (PendingBoneTransforms.Num() == 0)
// 		return;
//     
// 	USkeletalMeshComponent* SkelComp = GetSkelMeshComponent();
// 	if (!SkelComp)
// 		return;
//     
// 	// 确保我们有有效的骨骼网格和骨架
// 	if (!SkelComp->GetSkeletalMeshAsset() || !SkelComp->GetSkeletalMeshAsset()->GetSkeleton())
// 		return;
//     
// 	// 获取参考骨架
// 	const FReferenceSkeleton& RefSkeleton = SkelComp->GetSkeletalMeshAsset()->GetRefSkeleton();
//     
// 	// 获取骨骼容器
// 	const FBoneContainer& BoneContainer = GetRequiredBones();
//     
// 	// 获取可写的组件空间变换
// 	TArray<FTransform>& ComponentSpaceTMs = SkelComp->GetEditableComponentSpaceTransforms();
//     
// 	// 记录实际修改的骨骼数量
// 	int32 ModifiedBones = 0;
//     
// 	for (auto& Pair : PendingBoneTransforms)
// 	{
// 		const FName BoneName = Pair.Key;
// 		const FTransform& NewTransform = Pair.Value;
//
// 		// 首先检查骨骼是否在参考骨架中
// 		int32 RefSkeletonBoneIndex = RefSkeleton.FindBoneIndex(BoneName);
// 		if (RefSkeletonBoneIndex == INDEX_NONE)
// 			continue;
//         
// 		// 获取姿势骨骼索引
// 		int32 PoseBoneIndex = BoneContainer.GetPoseBoneIndexForBoneName(BoneName);
// 		if (PoseBoneIndex == INDEX_NONE)
// 			continue;
//         
// 		// 转换为紧凑索引
// 		FCompactPoseBoneIndex CompactIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(PoseBoneIndex));
// 		if (!CompactIndex.IsValid())
// 			continue;
//         
// 		// 确保索引在有效范围内
// 		int32 CompactIntIndex = CompactIndex.GetInt();
// 		if (ComponentSpaceTMs.IsValidIndex(CompactIntIndex))
// 		{
// 			ComponentSpaceTMs[CompactIntIndex] = NewTransform;
// 			ModifiedBones++;
// 		}
// 	}
//     
// 	if (ModifiedBones > 0)
// 	{
// 		// 强制更新骨骼变换
// 		SkelComp->RefreshBoneTransforms();
// 		SkelComp->MarkRenderTransformDirty();
// 		SkelComp->MarkRenderDynamicDataDirty();
// 	}
// }


