// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SvcGeometryDrawerManager.generated.h"

class USvcLineDrawer;
class USvcCubeDrawer;
class USvcPointDrawer;

UENUM(BlueprintType)
enum class EGeometryDrawType : uint8
{
	Point,
	Line,
	Cube,
	All
};


USTRUCT()
struct FIdArrayWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FString> IDs;
};

/**
 * 管理所有绘制组件（点、线、立方体）
 */
UCLASS()
class SEVNCEGEOMETRYDRAWER_API USvcGeometryDrawerManager : public UObject
{
	GENERATED_BODY()

public:
	void InitializeManager(UWorld* InWorld);
	void DeinitializeManager();

	FString DrawLine(const TArray<FVector>& Positions, const FLinearColor& Color, float Thickness);
	FString DrawPoint(const FVector& Location, const FLinearColor& Color, float Size);
	FString DrawCube(const FVector& Center, const FVector& Extent, const FLinearColor& Color);

	bool RemoveDrawObject(const FString& ObjectId);
	bool ClearDrawObjects(EGeometryDrawType Type);

private:
	UPROPERTY()
	AActor* m_HookActor = nullptr;
	UPROPERTY()
	USvcLineDrawer* m_LineDrawer = nullptr;
	UPROPERTY()
	USvcPointDrawer* m_PointDrawer = nullptr;
	UPROPERTY()
	USvcCubeDrawer* m_CubeDrawer = nullptr;
	
	UPROPERTY()
	TMap<FString, UPrimitiveComponent*> m_DrawObjects;
	UPROPERTY()
	TMap<EGeometryDrawType, FIdArrayWrapper> m_TypeGroups;
};

