#include "SvcMapSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"
#include "UObject/UObjectGlobals.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "LCCActor.h"
#include "LCCComponent.h"

void USvcMapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void USvcMapSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

bool USvcMapSubsystem::LoadMap(const FString& Path)
{
	UWorld* world = GetWorld();
	if (!world)
	{
		return false;
	}

	ALCCActor* lccActor = GetOrCreateLCCActor();
	if (!IsValid(lccActor))
	{
		return false;
	}

	ULCCComponent* lccComp = lccActor->GetLCCComponent();
	if (!IsValid(lccComp))
	{
		return false;
	}

	// 卸载当前地图
	if (lccComp->CheckIfLoaded())
		lccComp->UnLoad();
	

	// 加载新地图
	const bool loaded = lccComp->Load(Path);
	if (!loaded)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load dataset from: %s"), *Path);
		return false;
	}
	return true;
}

bool USvcMapSubsystem::Unload()
{
    ALCCActor* actor = m_LccActor.Get();
    if (!IsValid(actor))
    {
        return false;
    }
    if (ULCCComponent* comp = actor->GetLCCComponent())
    {
        comp->UnLoad();
    }
    return true;
}

ALCCActor* USvcMapSubsystem::GetOrCreateLCCActor()
{
	ALCCActor* lccActor = m_LccActor.Get();
    
	if (!IsValid(lccActor))
	{
		UWorld* world = GetWorld();
		if (world)
		{
			lccActor = world->SpawnActor<ALCCActor>({0,0,0}, {0,0,0});
			lccActor->GetLCCComponent()->SetLCCCollisionEnable(true);
			m_LccActor = lccActor;
			
		}
	}
    
	return lccActor;
}

