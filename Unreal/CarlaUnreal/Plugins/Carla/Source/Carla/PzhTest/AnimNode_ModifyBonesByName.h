// FAnimNode_ModifyBonesByName.h
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "BoneIndices.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_ModifyBonesByName.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct FAnimNode_ModifyBonesByName : public FAnimNode_SkeletalControlBase
{
    GENERATED_BODY()

public:
    // 运行时外部接口写进来的数据
    TMap<FName, FTransform> BoneTransforms;

    // 是否使用组件空间
    UPROPERTY(EditAnywhere, Category="SkeletalControl")
    TEnumAsByte<EBoneControlSpace> BoneSpace = BCS_ComponentSpace;

public:
    FAnimNode_ModifyBonesByName() {}

    // 骨骼控制
    virtual void EvaluateSkeletalControl_AnyThread(
    FComponentSpacePoseContext& Output,
    TArray<FBoneTransform>& OutBoneTransforms) override
	{
	    if (BoneTransforms.Num() == 0)
	        return;

	    // 从当前 pose 拿到 BoneContainer（compact-pose <-> skeleton 映射信息）
	    const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	    for (const auto& Pair : BoneTransforms)
	    {
	        const FName BoneName = Pair.Key;
	        const FTransform& NewTransform = Pair.Value;

	        // 1) 得到 pose/skeleton 索引（注意这个返回 int32，可能是 INDEX_NONE）
	        int32 PoseBoneIndex = BoneContainer.GetPoseBoneIndexForBoneName(BoneName);
	        if (PoseBoneIndex == INDEX_NONE)
	        {
	            // 该骨骼不在当前 asset / pose 中
	            continue;
	        }

	        // 2) 把 pose 索引转换为 FSkeletonPoseBoneIndex 再转为 FCompactPoseBoneIndex
	        FSkeletonPoseBoneIndex SkeletonPoseIndex(PoseBoneIndex);
	        FCompactPoseBoneIndex CompactIndex = BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(SkeletonPoseIndex);

	        // CompactIndex 可能是无效的（不包含在 compact pose 中）
	        if (CompactIndex.GetInt() == INDEX_NONE)
	        {
	            continue;
	        }

	        // 3) 如果传入的是 local-space transform，需要把它变为 component-space
	        FTransform FinalComponentTransform = NewTransform;
	        if (BoneSpace == BCS_ParentBoneSpace)
	        {
	            // 找父节点的 component-space 变换并合成（示例）
				FCompactPoseBoneIndex ParentIndex = BoneContainer.GetParentBoneIndex(CompactIndex);
	            if (ParentIndex.GetInt() != INDEX_NONE)
	            {
	                const FTransform ParentCSTransform = Output.Pose.GetComponentSpaceTransform(ParentIndex);
	                // NewTransform 是相对于父（local），要转换为 component-space：
	                FinalComponentTransform = NewTransform * ParentCSTransform;
	            }
	            else
	            {
	                // 没有父：local 跟 component 相同（或按需要处理）
	                FinalComponentTransform = NewTransform;
	            }
	        }
	        // 如果 BoneSpace == BCS_ComponentSpace，直接使用 NewTransform（或做额外转换）

	        // 4) 把结果加到输出数组，框架会把这些变换合并到最终 pose
	        OutBoneTransforms.Add(FBoneTransform(CompactIndex, FinalComponentTransform));
	    }
	}

    virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override
    {
        return true;
    }

    virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override {}
};
