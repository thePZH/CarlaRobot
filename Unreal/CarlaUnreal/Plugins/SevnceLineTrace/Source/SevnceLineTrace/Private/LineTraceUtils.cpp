#include "LineTraceUtils.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "DrawDebugHelpers.h"

bool ULineTraceUtils::CalculateRayDirectionFromUV(
	const FVector& CameraLocation,
	const FRotator& CameraRotation,
	float FOV,
	int32 ImageWidth,
	int32 ImageHeight,
	float U,
	float V,
	FVector& OutDirection
)
{
	ImageWidth = FMath::Max(1, ImageWidth);
	ImageHeight = FMath::Max(1, ImageHeight);

	float AspectRatio = static_cast<float>(ImageWidth) / static_cast<float>(ImageHeight);
	float HalfFOV = FMath::DegreesToRadians(FOV * 0.5f);
	float HalfWidth = FMath::Tan(HalfFOV);
	float HalfHeight = HalfWidth / AspectRatio;

	float NDC_X = (U / static_cast<float>(ImageWidth)) * 2.0f - 1.0f;
	float NDC_Y = 1.0f - (V / static_cast<float>(ImageHeight)) * 2.0f;

	FVector LocalDir = FVector(1.0f, NDC_X * HalfWidth, NDC_Y * HalfHeight);
	LocalDir = LocalDir.GetSafeNormal();

	OutDirection = CameraRotation.RotateVector(LocalDir).GetSafeNormal();
	return true;
}

FLineTraceResult ULineTraceUtils::LineTraceSingle(
	UWorld* World,
	const FVector& RayStart,
	const FVector& RayDirection,
	float MaxDistance,
	const FVector& ForwardDirection
)
{
	FLineTraceResult Result;

	if (!World)
	{
		return Result;
	}
	FVector RayStartOffseted = RayStart + RayDirection * 5;
	FVector RayEnd = RayStart + RayDirection * MaxDistance;

	FHitResult HitResult;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LineTraceSingle), true);
	Params.bReturnPhysicalMaterial = false;

	bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		RayStartOffseted,
		RayEnd,
		ECC_GameTraceChannel4,
		Params
	);
	// FCollisionObjectQueryParams ObjectQueryParams; 
	// ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel4);
	// bool bHit = World->LineTraceSingleByObjectType(
	// 	HitResult,                  
	// 	RayStartOffseted,           
	// 	RayEnd,                     
	// 	ObjectQueryParams,          
	// 	Params                 
	// );
	
	// Debug: draw ray and impact point
#if WITH_EDITOR
	const FVector DebugEnd = bHit ? HitResult.Location : RayEnd;
	DrawDebugLine(World, RayStartOffseted, DebugEnd, FColor::Red, false, 1.0f, 0, 2.0f);
	if (bHit)
	{
		DrawDebugPoint(World, HitResult.Location, 12.0f, FColor::Green, false, 5.0f);
	}
#endif

	Result.bHit = bHit;
	if (bHit)
	{
		// 转换为米
		Result.Location = HitResult.Location / 100.0f;
		
		// 使用前向方向计算深度
		FVector ToHitPoint = HitResult.Location - RayStart;
		Result.Depth = FVector::DotProduct(ToHitPoint, ForwardDirection.GetSafeNormal()) / 100.0f;
	}

	return Result;
}

FString ULineTraceUtils::LineTraceSingleJson(
	UWorld* World,
	const FVector& RayStart,
	const FVector& RayDirection,
	float MaxDistance,
	const FVector& ForwardDirection
)
{
	const FLineTraceResult Result = LineTraceSingle(World, RayStart, RayDirection, MaxDistance, ForwardDirection);
	TSharedPtr<FJsonObject> ResultObject = CreateSingleResultJsonObject(Result);
	if (!ResultObject.IsValid())
	{
		return FString();
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(ResultObject.ToSharedRef(), Writer);
	return OutputString;
}

TArray<FLineTraceResult> ULineTraceUtils::LineTraceMultiple(
	UWorld* World,
	const FVector& RayStart,
	const TArray<FVector>& RayDirections,
	float MaxDistance
)
{
	TArray<FLineTraceResult> Results;

	if (!World)
	{
		return Results;
	}

	for (const FVector& RayDirection : RayDirections)
	{
		const FLineTraceResult One = LineTraceSingle(World, RayStart, RayDirection, MaxDistance, FVector::ZeroVector);
		Results.Add(One);
	}

	return Results;
}

FString ULineTraceUtils::LineTraceMultipleJson(
	UWorld* World,
	const FVector& RayStart,
	const TArray<FVector>& RayDirections,
	float MaxDistance
)
{
	const TArray<FLineTraceResult> Results = LineTraceMultiple(World, RayStart, RayDirections, MaxDistance);
	TSharedPtr<FJsonObject> ResultObject = CreateMultipleResultJsonObject(Results);
	if (!ResultObject.IsValid())
	{
		return FString();
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(ResultObject.ToSharedRef(), Writer);
	return OutputString;
}

FLineTraceResult ULineTraceUtils::LineTraceSingleFromCamera(
	UWorld* World,
	const FVector& CameraLocation,
	const FRotator& CameraRotation,
	float FOV,
	int32 ImageWidth,
	int32 ImageHeight,
	float U,
	float V,
	float MaxDistance,
	bool bUseForwardDepth
)
{
	FVector RayDirection;
	if (!CalculateRayDirectionFromUV(CameraLocation, CameraRotation, FOV, ImageWidth, ImageHeight, U, V, RayDirection))
	{
		return FLineTraceResult();
	}

	// 始终沿像素射线方向作为投影前向；若不使用前向深度，则在返回前改为实际距离
	const FVector ForwardDirection = RayDirection;
	FLineTraceResult Result = LineTraceSingle(World, CameraLocation, RayDirection, MaxDistance, ForwardDirection);
	if (Result.bHit && !bUseForwardDepth)
	{
		const FVector HitCM = Result.Location * 100.0f; // Result.Location 为米
		Result.Depth = (HitCM - CameraLocation).Size() / 100.0f;
	}
	return Result;
}

FString ULineTraceUtils::LineTraceSingleFromCameraJson(
	UWorld* World,
	const FVector& CameraLocation,
	const FRotator& CameraRotation,
	float FOV,
	int32 ImageWidth,
	int32 ImageHeight,
	float U,
	float V,
	float MaxDistance,
	bool bUseForwardDepth
)
{
	const FLineTraceResult Result = LineTraceSingleFromCamera(World, CameraLocation, CameraRotation, FOV, ImageWidth, ImageHeight, U, V, MaxDistance, bUseForwardDepth);
	TSharedPtr<FJsonObject> ResultObject = CreateSingleResultJsonObject(Result);
	if (!ResultObject.IsValid())
	{
		return FString();
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(ResultObject.ToSharedRef(), Writer);
	return OutputString;
}

TArray<FLineTraceResult> ULineTraceUtils::LineTraceMultipleFromCamera(
	UWorld* World,
	const FVector& RayStart,
	const FRotator& CameraRotation,
	float FOV,
	int32 ImageWidth,
	int32 ImageHeight,
	const TArray<FVector2D>& UVs,
	float MaxDistance,
	bool bUseForwardDepth
)
{
	TArray<FLineTraceResult> Results;

	if (!World)
	{
		return Results;
	}

	for (const FVector2D& UV : UVs)
	{
		FVector RayDirection;
		if (!CalculateRayDirectionFromUV(RayStart, CameraRotation, FOV, ImageWidth, ImageHeight, UV.X, UV.Y, RayDirection))
		{
			// 如果计算失败，添加一个无效结果
			Results.Add(FLineTraceResult());
			continue;
		}
		// 始终沿像素射线方向作为投影前向
		FLineTraceResult One = LineTraceSingle(World, RayStart, RayDirection, MaxDistance, RayDirection);
		if (One.bHit && !bUseForwardDepth)
		{
			const FVector HitCM = One.Location * 100.0f; // One.Location 为米
			One.Depth = (HitCM - RayStart).Size() / 100.0f;
		}
		Results.Add(One);
	}

	return Results;
}

FString ULineTraceUtils::LineTraceMultipleFromCameraJson(
	UWorld* World,
	const FVector& CameraLocation,
	const FRotator& CameraRotation,
	float FOV,
	int32 ImageWidth,
	int32 ImageHeight,
	const TArray<FVector2D>& UVs,
	float MaxDistance,
	bool bUseForwardDepth
)
{
	const TArray<FLineTraceResult> Results = LineTraceMultipleFromCamera(World, CameraLocation, CameraRotation, FOV, ImageWidth, ImageHeight, UVs, MaxDistance, bUseForwardDepth);
	TSharedPtr<FJsonObject> ResultObject = CreateMultipleResultJsonObject(Results);
	if (!ResultObject.IsValid())
	{
		return FString();
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(ResultObject.ToSharedRef(), Writer);
	return OutputString;
}

TSharedPtr<FJsonObject> ULineTraceUtils::CreateSingleResultJsonObject(const FLineTraceResult& Result)
{
	TSharedPtr<FJsonObject> ResultObject = MakeShareable(new FJsonObject);
	ResultObject->SetBoolField(TEXT("hit"), Result.bHit);

	if (Result.bHit)
	{
		TSharedPtr<FJsonObject> LocationObject = MakeShareable(new FJsonObject);
		LocationObject->SetNumberField(TEXT("x"), Result.Location.X);
		LocationObject->SetNumberField(TEXT("y"), Result.Location.Y);
		LocationObject->SetNumberField(TEXT("z"), Result.Location.Z);
		ResultObject->SetObjectField(TEXT("location"), LocationObject);
		ResultObject->SetNumberField(TEXT("Depth"), Result.Depth);
	}

	return ResultObject;
}

TSharedPtr<FJsonObject> ULineTraceUtils::CreateMultipleResultJsonObject(const TArray<FLineTraceResult>& Results)
{
	TSharedPtr<FJsonObject> ResultObject = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	ResultsArray.Reserve(Results.Num());

	for (const FLineTraceResult& Result : Results)
	{
		TSharedPtr<FJsonObject> SingleObject = CreateSingleResultJsonObject(Result);
		ResultsArray.Add(MakeShareable(new FJsonValueObject(SingleObject)));
	}

	ResultObject->SetArrayField(TEXT("results"), ResultsArray);
	return ResultObject;
}

