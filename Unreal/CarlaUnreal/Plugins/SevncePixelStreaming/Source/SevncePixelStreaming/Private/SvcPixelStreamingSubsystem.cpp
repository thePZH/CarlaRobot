#include "SvcPixelStreamingSubsystem.h"
#include "PixelStreamingInputComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Misc/CString.h"

void USvcPixelStreamingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UWorld* world = GetGameInstance()->GetWorld();
    if (!world)
        return;

    FActorSpawnParameters spawnParams;
    spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AActor* m_PixelStreamingActor = world->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, spawnParams);
	if (!m_PixelStreamingActor)
		return;

    m_PixelInputComponent = NewObject<UPixelStreamingInput>(m_PixelStreamingActor);
	if (!m_PixelInputComponent)
		return;
	
    m_PixelInputComponent->RegisterComponent();
	m_PixelStreamingActor->AddInstanceComponent(m_PixelInputComponent);

    BindInputComponent();
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

void USvcPixelStreamingSubsystem::SendResponse(const FString& requestId, const FString& name, bool result, const FString& payloadJson, const FString& errorMessage)
{
	FString payload = payloadJson.IsEmpty() ? TEXT("{}") : payloadJson;
	FString escapedError = errorMessage.ReplaceCharWithEscapedChar();
	
	FString jsonString = FString::Format(
		TEXT("{\"type\":\"response\",\"request_id\":{0},\"name\":\"{1}\",\"result\":{2},\"payload\":{3},\"error_message\":\"{4}\"}"),
		{ requestId, name, result ? TEXT("true") : TEXT("false"), payload, escapedError }
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