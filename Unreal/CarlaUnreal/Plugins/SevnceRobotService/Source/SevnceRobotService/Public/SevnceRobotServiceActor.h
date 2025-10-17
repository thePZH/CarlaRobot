#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SevnceRobotServiceActor.generated.h"

UCLASS(Blueprintable)
class SEVNCEROBOTSERVICE_API ASevnceRobotServiceActor : public AActor
{
    GENERATED_BODY()

public:
    ASevnceRobotServiceActor();

    UFUNCTION(BlueprintCallable, Category="SevnceRobotService")
    FString CreateRobot(const FString& JsonSpec);

    UFUNCTION(BlueprintCallable, Category="SevnceRobotService")
    bool DestroyRobot(int32 RobotActorId);
};


