#pragma once

#include "CoreMinimal.h"
#include "LineTraceResult.generated.h"

/**
 * 射线检测结果
 */
USTRUCT(BlueprintType)
struct SEVNCELINETRACE_API FLineTraceResult
{
	GENERATED_BODY()

	/** 是否命中 */
	UPROPERTY(BlueprintReadOnly)
	bool bHit = false;

	/** 命中位置（世界坐标，单位：米） */
	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	/** 深度（从射线起点到命中点的距离，单位：米） */
	UPROPERTY(BlueprintReadOnly)
	float Depth = 0.0f;

	FLineTraceResult()
		: bHit(false)
		, Location(FVector::ZeroVector)
		, Depth(0.0f)
	{
	}
};

