#include "SevnceRobotServiceActor.h"

#include "Carla/Game/CarlaStatics.h"
#include "Carla/Game/CarlaEpisode.h"
#include "Carla/Services/SevnceRobotLogic.h"

ASevnceRobotServiceActor::ASevnceRobotServiceActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

FString ASevnceRobotServiceActor::CreateRobot(const FString& JsonSpec)
{
    UWorld* World = GetWorld();
    if (!World) return FString();
    UCarlaEpisode* Episode = UCarlaStatics::GetCurrentEpisode(World);
    if (!Episode) return FString();
    return SvcRobotLogic::CreateRobot(Episode, JsonSpec);
}

bool ASevnceRobotServiceActor::DestroyRobot(int32 RobotActorId)
{
    UWorld* World = GetWorld();
    if (!World) return false;
    UCarlaEpisode* Episode = UCarlaStatics::GetCurrentEpisode(World);
    if (!Episode) return false;
    return SvcRobotLogic::DestroyRobot(Episode, static_cast<carla::rpc::ActorId>(RobotActorId));
}


