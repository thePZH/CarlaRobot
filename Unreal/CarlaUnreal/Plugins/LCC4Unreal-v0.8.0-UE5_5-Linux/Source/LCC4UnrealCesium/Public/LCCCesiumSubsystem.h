/*********************************************
 * Copyright: XGrids Corporation
 * Author   : Feng HaiLiang
 * ******************************************/

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LCCCesiumSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnCesiumUpdate);

/**
 * Receive Cesium Georeference update event
 */
UCLASS()
class LCC4UNREALCESIUM_API ULCCCesiumSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	bool RequestCesiumOrigin(double& Latitude, double& Longitude, double& Altitude);
	UFUNCTION()
	void OnCesiumGeoreferenceUpdate();
	FOnCesiumUpdate OnCesiumUpdate;
};
