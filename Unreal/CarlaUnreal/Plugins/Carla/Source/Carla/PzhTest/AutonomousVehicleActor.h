#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Actor/ActorDescription.h"
#include "AutonomousVehicleActor.generated.h"

struct FActorDescription;
UCLASS()
class CARLA_API AAutonomousVehicleActor : public AActor
{
	GENERATED_BODY()

public:
	AAutonomousVehicleActor();

protected:
	virtual void BeginPlay() override;

private:
	void SpawnVehicleWithSensor();
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
	FActorDescription Description;
};