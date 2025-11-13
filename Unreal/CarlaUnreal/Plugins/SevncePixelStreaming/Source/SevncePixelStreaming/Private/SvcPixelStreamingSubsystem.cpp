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
	
	TWeakObjectPtr<USvcPixelStreamingSubsystem> WeakThis(this);
	
    m_InitHandle = FWorldDelegates::OnPostWorldInitialization.AddLambda(
        [WeakThis](UWorld* World, const UWorld::InitializationValues)
        {
        	if (!WeakThis.IsValid() || !World || (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE))
				return;
        	
            FWorldDelegates::OnPostWorldInitialization.Remove(WeakThis->m_InitHandle);

            FActorSpawnParameters spawnParams;
            spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            WeakThis->m_PixelStreamingActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, spawnParams);
            if (!WeakThis->m_PixelStreamingActor)
                return;

            WeakThis->m_PixelInputComponent = NewObject<UPixelStreamingInput>(WeakThis->m_PixelStreamingActor);
            if (!WeakThis->m_PixelInputComponent)
                return;

            WeakThis->m_PixelInputComponent->RegisterComponent();
            WeakThis->m_PixelStreamingActor->AddInstanceComponent(WeakThis->m_PixelInputComponent);
        	
            WeakThis->BindInputComponent();
        }
    );
}

void USvcPixelStreamingSubsystem::Deinitialize()
{
    if (m_PixelStreamingActor)
    {
        m_PixelStreamingActor->Destroy();
        m_PixelStreamingActor = nullptr;
        m_PixelInputComponent = nullptr;
    }
    Super::Deinitialize();
}

void USvcPixelStreamingSubsystem::SendNotify(const FString& name, const FString& payloadJson)
{
	FString payload = payloadJson.IsEmpty() ? TEXT("{}") : payloadJson;

	FString jsonString = FString::Format(
		TEXT("{\"type\":\"notify\",\"name\":\"{0}\",\"payload\":{1}}"),
		{ name, payload }
	);

	SendMessageToWeb(jsonString);
}

void USvcPixelStreamingSubsystem::SendResponse(const FString& name, const FString& payloadJson)
{
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