// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SevnceCameraSubsystem.generated.h"

class ASevnceMainCameraPawn;
class AActor;

/**
 * 主相机控制子系统
 * 提供蓝图和 C++ 接口来控制主相机
 */
UCLASS()
class SEVNCECAMERASYSTEM_API USevnceCameraSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * 通过 JSON 字符串控制主相机
	 * @param JsonString JSON 格式的控制参数
	 * @param TargetActor 可选的 Actor 指针，用于 fixed/orbit 模式。如果为 nullptr 且模式需要 Actor，则返回错误
	 * @return JSON 格式的响应字符串
	 */
	UFUNCTION(BlueprintCallable, Category = "Sevnce Camera")
	FString ControlMainCameraFromJson(const FString& JsonString, AActor* TargetActor = nullptr);

	/**
	 * 查找主相机 Pawn
	 * @return 主相机 Pawn 指针，如果未找到则返回 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "Sevnce Camera")
	ASevnceMainCameraPawn* FindMainCameraPawn() const;

private:
	// JSON 解析辅助函数
	bool ParseVectorField(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, FVector& OutVector, float UnitScale);
	bool ParseRotationField(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, FRotator& OutRotation);
	bool ParseTransformObject(const TSharedPtr<FJsonObject>& JsonObject, FTransform& OutTransform);
	
	// 响应生成函数
	FString MakeJsonResponse(bool bOk, const FString& ErrorMessage = FString(), const TFunction<void(TSharedPtr<FJsonObject>)>& OnSuccess = TFunction<void(TSharedPtr<FJsonObject>)>());
};

