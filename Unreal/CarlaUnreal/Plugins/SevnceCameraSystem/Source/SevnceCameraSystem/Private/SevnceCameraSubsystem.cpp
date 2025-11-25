// Copyright Epic Games, Inc. All Rights Reserved.

#include "SevnceCameraSubsystem.h"
#include "SevnceMainCameraPawn.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "EngineUtils.h"
#include "Engine/World.h"

void USevnceCameraSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void USevnceCameraSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

ASevnceMainCameraPawn* USevnceCameraSubsystem::FindMainCameraPawn() const
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<ASevnceMainCameraPawn> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

bool USevnceCameraSubsystem::ParseVectorField(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, FVector& OutVector, float UnitScale)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* FieldObject = nullptr;
	if (!JsonObject->TryGetObjectField(FieldName, FieldObject))
	{
		return false;
	}

	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
	if (!(*FieldObject)->TryGetNumberField(TEXT("x"), x) ||
		!(*FieldObject)->TryGetNumberField(TEXT("y"), y) ||
		!(*FieldObject)->TryGetNumberField(TEXT("z"), z))
	{
		return false;
	}

	OutVector = FVector(x * UnitScale, y * UnitScale, z * UnitScale);
	return true;
}

bool USevnceCameraSubsystem::ParseRotationField(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, FRotator& OutRotation)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* FieldObject = nullptr;
	if (!JsonObject->TryGetObjectField(FieldName, FieldObject))
	{
		return false;
	}

	double pitch = 0.0;
	double yaw = 0.0;
	double roll = 0.0;
	if (!(*FieldObject)->TryGetNumberField(TEXT("pitch"), pitch) ||
		!(*FieldObject)->TryGetNumberField(TEXT("yaw"), yaw) ||
		!(*FieldObject)->TryGetNumberField(TEXT("roll"), roll))
	{
		return false;
	}

	OutRotation = FRotator(pitch, yaw, roll);
	return true;
}

bool USevnceCameraSubsystem::ParseTransformObject(const TSharedPtr<FJsonObject>& JsonObject, FTransform& OutTransform)
{
	if (!JsonObject.IsValid())
	{
		return false;
	}

	FVector Location;
	FRotator Rotation;
	if (!ParseVectorField(JsonObject, TEXT("location"), Location, 100.0f))
	{
		return false;
	}
	if (!ParseRotationField(JsonObject, TEXT("rotation"), Rotation))
	{
		return false;
	}

	OutTransform = FTransform(Rotation, Location, FVector::OneVector);
	return true;
}

FString USevnceCameraSubsystem::MakeJsonResponse(bool bOk, const FString& ErrorMessage, const TFunction<void(TSharedPtr<FJsonObject>)>& OnSuccess)
{
	TSharedPtr<FJsonObject> ResponseObject = MakeShareable(new FJsonObject);
	ResponseObject->SetBoolField(TEXT("ok"), bOk);
	if (bOk)
	{
		if (OnSuccess)
		{
			OnSuccess(ResponseObject);
		}
	}
	else
	{
		ResponseObject->SetStringField(TEXT("error"), ErrorMessage);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(ResponseObject.ToSharedRef(), Writer);
	return OutputString;
}

FString USevnceCameraSubsystem::ControlMainCameraFromJson(const FString& JsonString, AActor* TargetActor)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return MakeJsonResponse(false, TEXT("World is null"));
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return MakeJsonResponse(false, TEXT("Invalid JSON payload"));
	}

	FString ModeString;
	if (!RootObject->TryGetStringField(TEXT("mode"), ModeString))
	{
		return MakeJsonResponse(false, TEXT("Missing mode"));
	}

	ASevnceMainCameraPawn* CameraPawn = FindMainCameraPawn();
	if (CameraPawn == nullptr)
	{
		return MakeJsonResponse(false, TEXT("SevnceMainCameraPawn not found"));
	}

	auto MakeSuccess = [&ModeString](TSharedPtr<FJsonObject> JsonObject)
	{
		JsonObject->SetStringField(TEXT("mode"), ModeString);
	};

	if (ModeString.Equals(TEXT("free"), ESearchCase::IgnoreCase))
	{
		const TSharedPtr<FJsonObject>* TransformObject = nullptr;
		CameraPawn->DetachFromActorTarget();

		if (RootObject->TryGetObjectField(TEXT("transform"), TransformObject))
		{
			FTransform TargetTransform;
			if (!ParseTransformObject(*TransformObject, TargetTransform))
			{
				return MakeJsonResponse(false, TEXT("Invalid transform data"));
			}

			CameraPawn->SetCameraTransform(TargetTransform);
		}

		return MakeJsonResponse(true, FString(), MakeSuccess);
	}

	if (!ModeString.Equals(TEXT("fixed"), ESearchCase::IgnoreCase) &&
		!ModeString.Equals(TEXT("orbit"), ESearchCase::IgnoreCase))
	{
		return MakeJsonResponse(false, TEXT("Unsupported mode"));
	}

	if (TargetActor == nullptr)
	{
		return MakeJsonResponse(false, TEXT("TargetActor is null"));
	}

	const TSharedPtr<FJsonObject>* RelativeTransformObject = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("relative_transform"), RelativeTransformObject))
	{
		return MakeJsonResponse(false, TEXT("Missing relative_transform"));
	}

	FTransform RelativeTransform;
	if (!ParseTransformObject(*RelativeTransformObject, RelativeTransform))
	{
		return MakeJsonResponse(false, TEXT("Invalid relative_transform"));
	}

	if (ModeString.Equals(TEXT("fixed"), ESearchCase::IgnoreCase))
	{
		CameraPawn->SetFixedModeTransform(RelativeTransform.GetLocation(), RelativeTransform.Rotator());
		CameraPawn->AttachToActorTarget(TargetActor, ECameraAttachmentMode::Fixed);
		return MakeJsonResponse(true, FString(), MakeSuccess);
	}

	// Orbit (spring arm) mode.
	double SpringArmLengthMeters = 0.0;
	if (!RootObject->TryGetNumberField(TEXT("spring_arm_length"), SpringArmLengthMeters))
	{
		return MakeJsonResponse(false, TEXT("Missing spring_arm_length"));
	}
	const float SpringArmLength = SpringArmLengthMeters * 100.0f;

	FVector SpringArmOffset = FVector::ZeroVector;
	ParseVectorField(RootObject, TEXT("spring_arm_offset"), SpringArmOffset, 100.0f);

	bool bEnableCollision = true;
	RootObject->TryGetBoolField(TEXT("enable_collision"), bEnableCollision);

	CameraPawn->SetSpringArmAnchorOffset(RelativeTransform.GetLocation());
	CameraPawn->SetSpringArmParameters(SpringArmLength, SpringArmOffset, bEnableCollision);
	CameraPawn->AttachToActorTarget(TargetActor, ECameraAttachmentMode::SpringArm);
	CameraPawn->SetSpringArmRotation(RelativeTransform.Rotator());
	return MakeJsonResponse(true, FString(), MakeSuccess);
}

