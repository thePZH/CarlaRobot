#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "LineTraceResult.h"
#include "LineTraceUtils.generated.h"

/**
 * 射线检测工具类
 * 提供从任意点发射射线的功能
 */
UCLASS()
class SEVNCELINETRACE_API ULineTraceUtils : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 从相机参数和UV坐标计算射线方向
	 * @param CameraLocation 相机位置（世界坐标，单位：厘米）
	 * @param CameraRotation 相机旋转
	 * @param FOV 视场角（度）
	 * @param ImageWidth 图像宽度（像素）
	 * @param ImageHeight 图像高度（像素）
	 * @param U UV坐标U（像素）
	 * @param V UV坐标V（像素）
	 * @param OutDirection 输出的射线方向（世界空间，已归一化）
	 * @return 是否成功计算
	 */
	static bool CalculateRayDirectionFromUV(
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float FOV,
		int32 ImageWidth,
		int32 ImageHeight,
		float U,
		float V,
		FVector& OutDirection
	);

	/**
	 * 执行单次射线检测
	 * @param World 世界对象
	 * @param RayStart 射线起点（世界坐标，单位：厘米）
	 * @param RayDirection 射线方向（世界空间，已归一化）
	 * @param MaxDistance 最大距离（单位：厘米）
	 * @param ForwardDirection 前向方向（用于计算深度，如果为空则使用实际距离）
	 * @return 射线检测结果
	 */
	static FLineTraceResult LineTraceSingle(
		UWorld* World,
		const FVector& RayStart,
		const FVector& RayDirection,
		float MaxDistance = 100000.0f,
		const FVector& ForwardDirection = FVector::ZeroVector
	);

	static FString LineTraceSingleJson(
		UWorld* World,
		const FVector& RayStart,
		const FVector& RayDirection,
		float MaxDistance = 100000.0f,
		const FVector& ForwardDirection = FVector::ZeroVector
	);

	/**
	 * 执行多次射线检测
	 * @param World 世界对象
	 * @param RayStart 射线起点（世界坐标，单位：厘米）
	 * @param RayDirections 射线方向数组（世界空间，已归一化）
	 * @param MaxDistance 最大距离（单位：厘米）
	 * @return 射线检测结果数组
	 */
	static TArray<FLineTraceResult> LineTraceMultiple(
		UWorld* World,
		const FVector& RayStart,
		const TArray<FVector>& RayDirections,
		float MaxDistance = 100000.0f
	);

	static FString LineTraceMultipleJson(
		UWorld* World,
		const FVector& RayStart,
		const TArray<FVector>& RayDirections,
		float MaxDistance = 100000.0f
	);

	/**
	 * 从相机参数和UV坐标执行单次射线检测
	 * @param World 世界对象
	 * @param CameraLocation 相机位置（世界坐标，单位：厘米）
	 * @param CameraRotation 相机旋转
	 * @param FOV 视场角（度）
	 * @param ImageWidth 图像宽度（像素）
	 * @param ImageHeight 图像高度（像素）
	 * @param U UV坐标U（像素）
	 * @param V UV坐标V（像素）
	 * @param MaxDistance 最大距离（单位：厘米）
	 * @param bUseForwardDepth 是否使用前向方向计算深度（true：使用相机前向，false：使用实际距离）
	 * @return 射线检测结果
	 */
	static FLineTraceResult LineTraceSingleFromCamera(
		UWorld* World,
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float FOV,
		int32 ImageWidth,
		int32 ImageHeight,
		float U,
		float V,
		float MaxDistance = 100000.0f,
		bool bUseForwardDepth = true
	);

	static FString LineTraceSingleFromCameraJson(
		UWorld* World,
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float FOV,
		int32 ImageWidth,
		int32 ImageHeight,
		float U,
		float V,
		float MaxDistance = 100000.0f,
		bool bUseForwardDepth = true
	);

	/**
	 * 从相机参数和UV坐标数组执行多次射线检测
	 * @param World 世界对象
	 * @param RayStart 相机位置（世界坐标，单位：厘米）
	 * @param CameraRotation 相机旋转
	 * @param FOV 视场角（度）
	 * @param ImageWidth 图像宽度（像素）
	 * @param ImageHeight 图像高度（像素）
	 * @param UVs UV坐标数组，每个元素包含U和V（像素）
	 * @param MaxDistance 最大距离（单位：厘米）
	 * @param bUseForwardDepth 是否使用前向方向计算深度（true：使用相机前向，false：使用实际距离）
	 * @return 射线检测结果数组
	 */
	static TArray<FLineTraceResult> LineTraceMultipleFromCamera(
		UWorld* World,
		const FVector& RayStart,
		const FRotator& CameraRotation,
		float FOV,
		int32 ImageWidth,
		int32 ImageHeight,
		const TArray<FVector2D>& UVs,
		float MaxDistance = 100000.0f,
		bool bUseForwardDepth = true
	);

	static FString LineTraceMultipleFromCameraJson(
		UWorld* World,
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float FOV,
		int32 ImageWidth,
		int32 ImageHeight,
		const TArray<FVector2D>& UVs,
		float MaxDistance = 100000.0f,
		bool bUseForwardDepth = true
	);

private:
	static TSharedPtr<class FJsonObject> CreateSingleResultJsonObject(const FLineTraceResult& Result);
	static TSharedPtr<class FJsonObject> CreateMultipleResultJsonObject(const TArray<FLineTraceResult>& Results);

};

