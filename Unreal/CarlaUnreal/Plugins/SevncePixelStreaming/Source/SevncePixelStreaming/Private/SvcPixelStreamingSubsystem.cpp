#include "SvcPixelStreamingSubsystem.h"
#include "PixelStreamingInputComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Misc/CString.h"
#include "Engine/Engine.h"

void USvcPixelStreamingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	m_bHasInitialized = false;
}

void USvcPixelStreamingSubsystem::Deinitialize()
{
	m_bHasInitialized = false;

    if (m_PixelStreamingActor)
    {
        m_PixelStreamingActor->Destroy();
        m_PixelStreamingActor = nullptr;
        m_PixelInputComponent = nullptr;
    }
    Super::Deinitialize();
}

void USvcPixelStreamingSubsystem::PostInitialize()
{
	if (m_bHasInitialized)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE))
	{
		// 世界还不可用时，不做初始化，等待下次使用再尝试
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	m_PixelStreamingActor = World->SpawnActor<AActor>(
		AActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!m_PixelStreamingActor)
	{
		return;
	}

	m_PixelInputComponent = NewObject<UPixelStreamingInput>(m_PixelStreamingActor);
	if (!m_PixelInputComponent)
	{
		m_PixelStreamingActor->Destroy();
		m_PixelStreamingActor = nullptr;
		return;
	}

	m_PixelInputComponent->RegisterComponent();
	m_PixelStreamingActor->AddInstanceComponent(m_PixelInputComponent);

	BindInputComponent();

	m_bHasInitialized = true;
}

void USvcPixelStreamingSubsystem::SendNotify(const FString& name, const FString& payloadJson)
{
	PostInitialize();

	FString payload = payloadJson.IsEmpty() ? TEXT("{}") : payloadJson;

	FString jsonString = FString::Format(
		TEXT("{\"type\":\"notify\",\"name\":\"{0}\",\"payload\":{1}}"),
		{ name, payload }
	);

	SendMessageToWeb(jsonString);
}

void USvcPixelStreamingSubsystem::SendResponse(const FString& name, const FString& payloadJson)
{
	PostInitialize();

	FString payload = payloadJson.IsEmpty() ? TEXT("{}") : payloadJson;
	FString jsonString = FString::Format(
		TEXT("{\"type\":\"response\",\"name\":\"{0}\",\"payload\":{1}}"),
		{ name, payload }
	);

	SendMessageToWeb(jsonString);
}

void USvcPixelStreamingSubsystem::BindInputComponent()
{
    if (!m_PixelInputComponent)
        return;

    // 正确绑定委托
    m_PixelInputComponent->OnInputEvent.AddDynamic(this, &USvcPixelStreamingSubsystem::HandleWebInputEvent);
}

void USvcPixelStreamingSubsystem::HandleWebInputEvent(const FString& Descriptor)
{
    OnWebMessageReceived.Broadcast(Descriptor);
}

void USvcPixelStreamingSubsystem::SendMessageToWeb(const FString& JsonString)
{
    if (!m_PixelInputComponent)
        return;
	
    m_PixelInputComponent->SendPixelStreamingResponse(JsonString);
}