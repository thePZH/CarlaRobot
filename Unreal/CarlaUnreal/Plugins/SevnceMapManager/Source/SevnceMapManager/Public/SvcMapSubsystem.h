#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Map.h"
#include "Containers/Array.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "SvcMapSubsystem.generated.h"

class ALCCActor;

UCLASS()
class SEVNCEMAPMANAGER_API USvcMapSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Sevnce|MapManager")
	bool LoadMap(const FString& Path);

    UFUNCTION(BlueprintCallable, Category = "Sevnce|MapManager")
    bool Unload();

private:
	ALCCActor* GetOrCreateLCCActor();
	TWeakObjectPtr<ALCCActor> m_LccActor;
};


