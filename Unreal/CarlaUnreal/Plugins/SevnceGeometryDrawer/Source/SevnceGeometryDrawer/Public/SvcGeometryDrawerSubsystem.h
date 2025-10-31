// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SvcGeometryDrawerManager.h"
#include "SvcGeometryDrawerSubsystem.generated.h"


class FDelegateHandle;

/**
 * 
 */
UCLASS()
class SEVNCEGEOMETRYDRAWER_API USvcGeometryDrawerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 初始化与销毁 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 返回内部管理器 */
	UFUNCTION(BlueprintCallable, Category = "Sevnce|GeometryDrawer")
	USvcGeometryDrawerManager* GetManager() const { return m_DrawerManager; }

public:
	/** 绘制接口 - 返回绘制对象ID */
	UFUNCTION(BlueprintCallable, Category="Sevnce|GeometryDrawer")
	FString DrawLine(const TArray<FVector>& Positions, const FLinearColor& Color = FLinearColor::Red, float Scale = 1.0f);

	UFUNCTION(BlueprintCallable, Category="Sevnce|GeometryDrawer")
	FString DrawPoint(const FVector& Location, const FLinearColor& Color = FLinearColor::Green, float Scale = 1.0f);

	UFUNCTION(BlueprintCallable, Category="Sevnce|GeometryDrawer")
	FString DrawCube(const FVector& Center, const FVector& Scale = FVector(1.f), const FLinearColor& Color = FLinearColor::Blue);

    // JSON 接口：解析后调用上面的非 JSON 接口
    UFUNCTION(BlueprintCallable, Category="Sevnce|GeometryDrawer|JSON")
    FString DrawPointJson(const FString& Json);

    UFUNCTION(BlueprintCallable, Category="Sevnce|GeometryDrawer|JSON")
    FString DrawLineJson(const FString& Json);

    UFUNCTION(BlueprintCallable, Category="Sevnce|GeometryDrawer|JSON")
    FString DrawCubeJson(const FString& Json);

    UFUNCTION(BlueprintCallable, Category="Sevnce|GeometryDrawer|JSON")
    FString RemoveJson(const FString& Json);

    UFUNCTION(BlueprintCallable, Category="Sevnce|GeometryDrawer|JSON")
    FString ClearJson(const FString& Json);

	/** 删除一个绘制对象 */
	UFUNCTION(BlueprintCallable, Category="Sevnce|GeometryDrawer")
	bool RemoveDrawObject(FString ObjectId);

	/** 清空指定类型或全部绘制对象 */
	UFUNCTION(BlueprintCallable, Category="Sevnce|GeometryDrawer")
	bool ClearDrawObjects(EGeometryDrawType Type = EGeometryDrawType::All);

protected:
	UPROPERTY()
	class USvcGeometryDrawerManager* m_DrawerManager;

	FDelegateHandle m_InitHandle;
};
