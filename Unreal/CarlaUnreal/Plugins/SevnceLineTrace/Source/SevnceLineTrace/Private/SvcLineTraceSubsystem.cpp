#include "SvcLineTraceSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Slate/SceneViewport.h"

void USvcLineTraceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void USvcLineTraceSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

FLineTraceResult USvcLineTraceSubsystem::LineTraceSingle(
	const FVector& RayStart,
	const FVector& RayDirection,
	float MaxDistance
) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return FLineTraceResult();
	}

	// 转换为厘米
	FVector RayStartCm = RayStart * 100.0f;
	float MaxDistanceCm = MaxDistance * 100.0f;

	return ULineTraceUtils::LineTraceSingle(World, RayStartCm, RayDirection, MaxDistanceCm);
}

TArray<FLineTraceResult> USvcLineTraceSubsystem::LineTraceMultiple(
	const FVector& RayStart,
	const TArray<FVector>& RayDirections,
	float MaxDistance
) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return TArray<FLineTraceResult>();
	}

	// 转换为厘米
	FVector RayStartCm = RayStart * 100.0f;
	float MaxDistanceCm = MaxDistance * 100.0f;

	return ULineTraceUtils::LineTraceMultiple(World, RayStartCm, RayDirections, MaxDistanceCm);
}

FLineTraceResult USvcLineTraceSubsystem::LineTraceSingleFromCamera(
	const FVector& CameraLocation,
	const FRotator& CameraRotation,
	float FOV,
	int32 ImageWidth,
	int32 ImageHeight,
	float U,
	float V,
	float MaxDistance
) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return FLineTraceResult();
	}

	// 转换为厘米
	FVector CameraLocationCm = CameraLocation * 100.0f;

	return ULineTraceUtils::LineTraceSingleFromCamera(
		World,
		CameraLocationCm,
		CameraRotation,
		FOV,
		ImageWidth,
		ImageHeight,
		U,
		V,
		MaxDistance
	);
}

TArray<FLineTraceResult> USvcLineTraceSubsystem::LineTraceMultipleFromCamera(
	const FVector& CameraLocation,
	const FRotator& CameraRotation,
	float FOV,
	int32 ImageWidth,
	int32 ImageHeight,
	const TArray<FVector2D>& UVs,
	float MaxDistance
) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return TArray<FLineTraceResult>();
	}

	// 转换为厘米
	FVector CameraLocationCm = CameraLocation * 100.0f;

	return ULineTraceUtils::LineTraceMultipleFromCamera(
		World,
		CameraLocationCm,
		CameraRotation,
		FOV,
		ImageWidth,
		ImageHeight,
		UVs,
		MaxDistance
	);
}

FString USvcLineTraceSubsystem::LineTraceSingleJson(
	const FVector& RayStart,
	const FVector& RayDirection,
	float MaxDistance
) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return FString();
	}

	FVector RayStartCm = RayStart * 100.0f;
	return ULineTraceUtils::LineTraceSingleJson(World, RayStartCm, RayDirection, MaxDistance);
}

FString USvcLineTraceSubsystem::LineTraceMultipleJson(
	const FVector& RayStart,
	const TArray<FVector>& RayDirections,
	float MaxDistance
) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return FString();
	}

	FVector RayStartCm = RayStart * 100.0f;
	return ULineTraceUtils::LineTraceMultipleJson(World, RayStartCm, RayDirections, MaxDistance);
}

FString USvcLineTraceSubsystem::LineTraceSingleFromCameraJson(
	const FVector& CameraLocation,
	const FRotator& CameraRotation,
	float FOV,
	int32 ImageWidth,
	int32 ImageHeight,
	float U,
	float V,
	float MaxDistance
) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return FString();
	}

	FVector CameraLocationCm = CameraLocation * 100.0f;
	return ULineTraceUtils::LineTraceSingleFromCameraJson(
		World,
		CameraLocationCm,
		CameraRotation,
		FOV,
		ImageWidth,
		ImageHeight,
		U,
		V,
		MaxDistance
	);
}

	FString USvcLineTraceSubsystem::LineTraceMultipleFromCameraJson(
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float FOV,
		int32 ImageWidth,
		int32 ImageHeight,
		const TArray<FVector2D>& UVs,
		float MaxDistance
	) const
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return FString();
		}

		FVector CameraLocationCm = CameraLocation * 100.0f;
		return ULineTraceUtils::LineTraceMultipleFromCameraJson(
			World,
			CameraLocationCm,
			CameraRotation,
			FOV,
			ImageWidth,
			ImageHeight,
			UVs,
			MaxDistance
		);
	}

	FLineTraceResult USvcLineTraceSubsystem::LineTraceSingleFromPlayerCamera(
		APlayerController* PlayerController,
		float ScreenX,
		float ScreenY,
		float MaxDistance
	) const
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return FLineTraceResult();
		}

		// 如果没有提供PlayerController，尝试获取本地玩家控制器
		if (!PlayerController)
		{
			PlayerController = UGameplayStatics::GetPlayerController(World, 0);
		}

		if (!PlayerController)
		{
			return FLineTraceResult();
		}

		// 获取相机管理器
		APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
		if (!CameraManager)
		{
			return FLineTraceResult();
		}

		// 获取相机位置和旋转
		FVector CameraLocation = CameraManager->GetCameraLocation();
		FRotator CameraRotation = CameraManager->GetCameraRotation();

		// 获取FOV
		float FOV = CameraManager->GetFOVAngle();

		// 获取视口大小
		int32 ViewportSizeX = 0;
		int32 ViewportSizeY = 0;
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

		// 如果无法获取视口大小，使用默认值
		if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
		{
			if (GEngine && GEngine->GameViewport)
			{
				FViewport* Viewport = GEngine->GameViewport->GetGameViewport();
				if (Viewport)
				{
					FIntPoint ViewportSize = Viewport->GetSizeXY();
					ViewportSizeX = ViewportSize.X;
					ViewportSizeY = ViewportSize.Y;
				}
			}

			// 如果还是获取不到，使用默认值
			if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
			{
				ViewportSizeX = 1920;
				ViewportSizeY = 1080;
			}
		}

		// 转换为米（相机位置从厘米转米）
		FVector CameraLocationM = CameraLocation / 100.0f;

		// 调用射线检测
		return LineTraceSingleFromCamera(
			CameraLocationM,
			CameraRotation,
			FOV,
			ViewportSizeX,
			ViewportSizeY,
			ScreenX,
			ScreenY,
			MaxDistance
		);
	}

	TArray<FLineTraceResult> USvcLineTraceSubsystem::LineTraceMultipleFromPlayerCamera(
		APlayerController* PlayerController,
		const TArray<FVector2D>& ScreenPositions,
		float MaxDistance
	) const
	{
		TArray<FLineTraceResult> Results;

		UWorld* World = GetWorld();
		if (!World)
		{
			return Results;
		}

		// 如果没有提供PlayerController，尝试获取本地玩家控制器
		if (!PlayerController)
		{
			PlayerController = UGameplayStatics::GetPlayerController(World, 0);
		}

		if (!PlayerController)
		{
			return Results;
		}

		// 获取相机管理器
		APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
		if (!CameraManager)
		{
			return Results;
		}

		// 获取相机位置和旋转
		FVector CameraLocation = CameraManager->GetCameraLocation();
		FRotator CameraRotation = CameraManager->GetCameraRotation();

		// 获取FOV
		float FOV = CameraManager->GetFOVAngle();

		// 获取视口大小
		int32 ViewportSizeX = 0;
		int32 ViewportSizeY = 0;
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

		// 如果无法获取视口大小，使用默认值
		if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
		{
			if (GEngine && GEngine->GameViewport)
			{
				FSceneViewport* Viewport = GEngine->GameViewport->GetGameViewport();
				if (Viewport)
				{
					FIntPoint ViewportSize = Viewport->GetSizeXY();
					ViewportSizeX = ViewportSize.X;
					ViewportSizeY = ViewportSize.Y;
				}
			}

			// 如果还是获取不到，使用默认值
			if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
			{
				ViewportSizeX = 1920;
				ViewportSizeY = 1080;
			}
		}

		// 转换为米（相机位置从厘米转米）
		FVector CameraLocationM = CameraLocation / 100.0f;

		// 调用射线检测
		return LineTraceMultipleFromCamera(
			CameraLocationM,
			CameraRotation,
			FOV,
			ViewportSizeX,
			ViewportSizeY,
			ScreenPositions,
			MaxDistance
		);
	}

	FString USvcLineTraceSubsystem::LineTraceSingleFromPlayerCameraJson(
		APlayerController* PlayerController,
		float ScreenX,
		float ScreenY,
		float MaxDistance
	) const
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return FString();
		}

		// 如果没有提供PlayerController，尝试获取本地玩家控制器
		if (!PlayerController)
		{
			PlayerController = UGameplayStatics::GetPlayerController(World, 0);
		}

		if (!PlayerController)
		{
			return FString();
		}

		// 获取相机管理器
		APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
		if (!CameraManager)
		{
			return FString();
		}

		// 获取相机位置和旋转
		FVector CameraLocation = CameraManager->GetCameraLocation();
		FRotator CameraRotation = CameraManager->GetCameraRotation();

		// 获取FOV
		float FOV = CameraManager->GetFOVAngle();

		// 获取视口大小
		int32 ViewportSizeX = 0;
		int32 ViewportSizeY = 0;
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

		// 如果无法获取视口大小，使用默认值
		if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
		{
			if (GEngine && GEngine->GameViewport)
			{
				FSceneViewport* Viewport = GEngine->GameViewport->GetGameViewport();
				if (Viewport)
				{
					FIntPoint ViewportSize = Viewport->GetSizeXY();
					ViewportSizeX = ViewportSize.X;
					ViewportSizeY = ViewportSize.Y;
				}
			}

			// 如果还是获取不到，使用默认值
			if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
			{
				ViewportSizeX = 1920;
				ViewportSizeY = 1080;
			}
		}

		// 调用JSON版本的射线检测（CameraLocation已经是厘米单位）
		return ULineTraceUtils::LineTraceSingleFromCameraJson(
			World,
			CameraLocation,
			CameraRotation,
			FOV,
			ViewportSizeX,
			ViewportSizeY,
			ScreenX,
			ScreenY,
			MaxDistance,
			true
		);
	}

	FString USvcLineTraceSubsystem::LineTraceMultipleFromPlayerCameraJson(
		APlayerController* PlayerController,
		const TArray<FVector2D>& ScreenPositions,
		float MaxDistance
	) const
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return FString();
		}

		// 如果没有提供PlayerController，尝试获取本地玩家控制器
		if (!PlayerController)
		{
			PlayerController = UGameplayStatics::GetPlayerController(World, 0);
		}

		if (!PlayerController)
		{
			return FString();
		}

		// 获取相机管理器
		APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
		if (!CameraManager)
		{
			return FString();
		}

		// 获取相机位置和旋转
		FVector CameraLocation = CameraManager->GetCameraLocation();
		FRotator CameraRotation = CameraManager->GetCameraRotation();

		// 获取FOV
		float FOV = CameraManager->GetFOVAngle();

		// 获取视口大小
		int32 ViewportSizeX = 0;
		int32 ViewportSizeY = 0;
		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);

		// 如果无法获取视口大小，使用默认值
		if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
		{
			if (GEngine && GEngine->GameViewport)
			{
				FSceneViewport* Viewport = GEngine->GameViewport->GetGameViewport();
				if (Viewport)
				{
					FIntPoint ViewportSize = Viewport->GetSizeXY();
					ViewportSizeX = ViewportSize.X;
					ViewportSizeY = ViewportSize.Y;
				}
			}

			// 如果还是获取不到，使用默认值
			if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
			{
				ViewportSizeX = 1920;
				ViewportSizeY = 1080;
			}
		}

		// 调用JSON版本的射线检测（CameraLocation已经是厘米单位）
		return ULineTraceUtils::LineTraceMultipleFromCameraJson(
			World,
			CameraLocation,
			CameraRotation,
			FOV,
			ViewportSizeX,
			ViewportSizeY,
			ScreenPositions,
			MaxDistance,
			true
		);
	}

