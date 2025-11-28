#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LineTraceResult.h"
#include "LineTraceUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "SvcLineTraceSubsystem.generated.h"

/**
 * 射线检测子系统
 * 提供蓝图接口用于射线检测功能
 */
UCLASS()
class SEVNCELINETRACE_API USvcLineTraceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * 执行单次射线检测（从指定位置和方向）
	 * @param RayStart 射线起点（世界坐标）
	 * @param RayDirection 射线方向（世界空间，已归一化）
	 * @param MaxDistance 最大距离
	 * @return 射线检测结果
	 */
	UFUNCTION(BlueprintCallable, Category = "Sevnce|LineTrace")
	FLineTraceResult LineTraceSingle(
		const FVector& RayStart,
		const FVector& RayDirection,
		float MaxDistance = 10000.0f
	) const;

	/**
	 * 执行多次射线检测（从指定位置和方向数组）
	 * @param RayStart 射线起点（世界坐标）
	 * @param RayDirections 射线方向数组（世界空间，已归一化）
	 * @param MaxDistance 最大距离
	 * @return 射线检测结果数组
	 */
	UFUNCTION(BlueprintCallable, Category = "Sevnce|LineTrace")
	TArray<FLineTraceResult> LineTraceMultiple(
		const FVector& RayStart,
		const TArray<FVector>& RayDirections,
		float MaxDistance = 10000.0f
	) const;

	/**
	 * 从相机参数和UV坐标执行单次射线检测
	 * @param CameraLocation 相机位置（世界坐标）
	 * @param CameraRotation 相机旋转
	 * @param FOV 视场角（度）
	 * @param ImageWidth 图像宽度（像素）
	 * @param ImageHeight 图像高度（像素）
	 * @param U UV坐标U（像素）
	 * @param V UV坐标V（像素）
	 * @param MaxDistance 最大距离
	 * @return 射线检测结果
	 */
	UFUNCTION(BlueprintCallable, Category = "Sevnce|LineTrace")
	FLineTraceResult LineTraceSingleFromCamera(
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float FOV,
		int32 ImageWidth,
		int32 ImageHeight,
		float U,
		float V,
		float MaxDistance = 10000.0f
	) const;

	/**
	 * 从相机参数和UV坐标数组执行多次射线检测
	 * @param CameraLocation 相机位置（世界坐标）
	 * @param CameraRotation 相机旋转
	 * @param FOV 视场角（度）
	 * @param ImageWidth 图像宽度（像素）
	 * @param ImageHeight 图像高度（像素）
	 * @param UVs UV坐标数组，每个元素包含U和V（像素）
	 * @param MaxDistance 最大距离
	 * @return 射线检测结果数组
	 */
	UFUNCTION(BlueprintCallable, Category = "Sevnce|LineTrace")
	TArray<FLineTraceResult> LineTraceMultipleFromCamera(
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float FOV,
		int32 ImageWidth,
		int32 ImageHeight,
		const TArray<FVector2D>& UVs,
		float MaxDistance = 10000.0f
	) const;

	UFUNCTION(BlueprintCallable, Category = "Sevnce|LineTrace|JSON")
	FString LineTraceSingleJson(
		const FVector& RayStart,
		const FVector& RayDirection,
		float MaxDistance = 10000.0f
	) const;

	UFUNCTION(BlueprintCallable, Category = "Sevnce|LineTrace|JSON")
	FString LineTraceMultipleJson(
		const FVector& RayStart,
		const TArray<FVector>& RayDirections,
		float MaxDistance = 10000.0f
	) const;

	UFUNCTION(BlueprintCallable, Category = "Sevnce|LineTrace|JSON")
	FString LineTraceSingleFromCameraJson(
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float FOV,
		int32 ImageWidth,
		int32 ImageHeight,
		float U,
		float V,
		float MaxDistance = 10000.0f
	) const;

	UFUNCTION(BlueprintCallable, Category = "Sevnce|LineTrace|JSON")
	FString LineTraceMultipleFromCameraJson(
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float FOV,
		int32 ImageWidth,
		int32 ImageHeight,
		const TArray<FVector2D>& UVs,
		float MaxDistance = 10000.0f
	) const;

	/**
	 * 从玩家控制器的主相机执行单次射线检测（使用屏幕坐标）
	 * @param PlayerController 玩家控制器（如果为null，则自动获取本地玩家控制器）
	 * @param ScreenX 屏幕X坐标（像素）
	 * @param ScreenY 屏幕Y坐标（像素）
	 * @param MaxDistance 最大距离
	 * @return 射线检测结果
	 */
	UFUNCTION(BlueprintCallable, Category = "Sevnce|LineTrace|PlayerCamera")
	FLineTraceResult LineTraceSingleFromPlayerCamera(
		class APlayerController* PlayerController,
		float ScreenX,
		float ScreenY,
		float MaxDistance = 10000.0f
	) const;

	/**
	 * 从玩家控制器的主相机执行多次射线检测（使用屏幕坐标数组）
	 * @param PlayerController 玩家控制器（如果为null，则自动获取本地玩家控制器）
	 * @param ScreenPositions 屏幕坐标数组，每个元素包含X和Y（像素）
	 * @param MaxDistance 最大距离（单位：米）
	 * @return 射线检测结果数组
	 */
	UFUNCTION(BlueprintCallable, Category = "Sevnce|LineTrace|PlayerCamera")
	TArray<FLineTraceResult> LineTraceMultipleFromPlayerCamera(
		class APlayerController* PlayerController,
		const TArray<FVector2D>& ScreenPositions,
		float MaxDistance = 10000.0f
	) const;

	/**
	 * 从玩家控制器的主相机执行单次射线检测（使用屏幕坐标），返回JSON字符串
	 * @param PlayerController 玩家控制器（如果为null，则自动获取本地玩家控制器）
	 * @param ScreenX 屏幕X坐标（像素）
	 * @param ScreenY 屏幕Y坐标（像素）
	 * @param MaxDistance 最大距离
	 * @return 射线检测结果JSON字符串
	 */
	UFUNCTION(BlueprintCallable, Category = "Sevnce|LineTrace|PlayerCamera|JSON")
	FString LineTraceSingleFromPlayerCameraJson(
		class APlayerController* PlayerController,
		float ScreenX,
		float ScreenY,
		float MaxDistance = 10000.0f
	) const;

	/**
	 * 从玩家控制器的主相机执行多次射线检测（使用屏幕坐标数组），返回JSON字符串
	 * @param PlayerController 玩家控制器（如果为null，则自动获取本地玩家控制器）
	 * @param ScreenPositions 屏幕坐标数组，每个元素包含X和Y（像素）
	 * @param MaxDistance 最大距离（单位：米）
	 * @return 射线检测结果JSON字符串
	 */
	UFUNCTION(BlueprintCallable, Category = "Sevnce|LineTrace|PlayerCamera|JSON")
	FString LineTraceMultipleFromPlayerCameraJson(
		class APlayerController* PlayerController,
		const TArray<FVector2D>& ScreenPositions,
		float MaxDistance = 10000.0f
	) const;
};

