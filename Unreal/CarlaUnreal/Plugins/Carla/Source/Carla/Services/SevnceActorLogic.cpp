#include "SevnceActorLogic.h"

FCarlaActor* SvcActorLogic::SpawnActor(UCarlaEpisode* Episode, const carla::rpc::ActorDescription& Description, const carla::rpc::Transform& Transform)
{
    if (!Episode)
    {
        UE_LOG(LogCarla, Error, TEXT("SvcActorLogic::SpawnActor: Episode is null"));
        return nullptr;
    }

    auto result = Episode->SpawnActorWithInfo(Transform, const_cast<carla::rpc::ActorDescription&>(Description));

    if (result.Key != EActorSpawnResultStatus::Success)
    {
        UE_LOG(LogCarla, Error, TEXT("SvcActorLogic::SpawnActor: Actor not Spawned - %s"), *FActorSpawnResult::StatusToString(result.Key));
        return nullptr;
    }

    // 处理大地图管理器
    ALargeMapManager* largeMap = UCarlaStatics::GetLargeMapManager(Episode->GetWorld());
    if (largeMap)
    {
        largeMap->OnActorSpawned(*result.Value);
    }

    return result.Value;
}

FCarlaActor* SvcActorLogic::SpawnActorWithParent(UCarlaEpisode* Episode, const carla::rpc::ActorDescription& Description, const carla::rpc::Transform& Transform, carla::rpc::ActorId ParentId, carla::rpc::AttachmentType InAttachmentType)
{
    if (!Episode)
    {
        UE_LOG(LogCarla, Error, TEXT("SvcActorLogic::SpawnActorWithParent: Episode is null"));
        return nullptr;
    }

    // 先生成Actor
    auto result = Episode->SpawnActorWithInfo(Transform, const_cast<carla::rpc::ActorDescription&>(Description));
    if (result.Key != EActorSpawnResultStatus::Success)
    {
        UE_LOG(LogCarla, Error, TEXT("SvcActorLogic::SpawnActorWithParent: Actor not Spawned - %s"), *FActorSpawnResult::StatusToString(result.Key));
        return nullptr;
    }

    // 获取生成的Actor
    FCarlaActor* carlaActor = Episode->FindCarlaActor(result.Value->GetActorId());
    if (!carlaActor)
    {
        UE_LOG(LogCarla, Error, TEXT("SvcActorLogic::SpawnActorWithParent: internal error: actor could not be spawned"));
        return nullptr;
    }

    // 获取父Actor
    FCarlaActor* parentCarlaActor = Episode->FindCarlaActor(ParentId);
    if (!parentCarlaActor)
    {
        UE_LOG(LogCarla, Error, TEXT("SvcActorLogic::SpawnActorWithParent: unable to attach actor: parent actor not found"));
        return nullptr;
    }

    // 设置父子关系
    carlaActor->SetParentActor(parentCarlaActor->GetActor());
    carlaActor->SetParent(ParentId);
    carlaActor->SetAttachmentType(InAttachmentType);
    parentCarlaActor->AddChildren(carlaActor->GetActorId());

    // 处理ROS2相关逻辑
    HandleROS2ParentName(Episode, carlaActor, parentCarlaActor);

    // 处理挂载逻辑
    if (!parentCarlaActor->IsDormant())
    {
        // 尝试特殊挂载（骨骼网格插槽）
        bool bDidAttachToSocket = HandleSpecialAttachment(carlaActor, parentCarlaActor);
        
        // 如果没有成功挂载到插槽，使用默认挂载方式
        if (!bDidAttachToSocket)
        {
            Episode->AttachActors(
                carlaActor->GetActor(),
                parentCarlaActor->GetActor(),
                static_cast<EAttachmentType>(InAttachmentType));
        }
    }
    else
    {
        // 如果父Actor处于休眠状态，将子Actor也设为休眠
        Episode->PutActorToSleep(carlaActor->GetActorId());
    }

    return carlaActor;
}

bool SvcActorLogic::HandleSpecialAttachment(FCarlaActor* CarlaActor, FCarlaActor* ParentCarlaActor)
{
    if (!CarlaActor || !ParentCarlaActor)
    {
        return false;
    }

    // 检查父Actor是否有骨骼网格组件
    if (auto skeletalMeshComp = ParentCarlaActor->GetActor()->FindComponentByClass<USkeletalMeshComponent>())
    {
        if (USkeletalMesh* skMesh = skeletalMeshComp->GetSkeletalMeshAsset())
        {
            // 区分sensor和lidar
            if (auto lidar = Cast<ARayCastLidar>(CarlaActor->GetActor()))
            {
                // Lidar挂载到SlotLD插槽
                if (skMesh->FindSocket(TEXT("SlotLD")))
                {
                    CarlaActor->GetActor()->AttachToComponent(
                        skeletalMeshComp,
                        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                        FName(TEXT("SlotLD")));
                    return true;
                }
            }
            else
            {
                // 其他传感器挂载到SlotBL插槽
                if (skMesh->FindSocket(TEXT("SlotBL")))
                {
                    CarlaActor->GetActor()->AttachToComponent(
                        skeletalMeshComp,
                        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                        FName(TEXT("SlotBL")));
                    return true;
                }
            }
        }
    }

    return false;
}

void SvcActorLogic::HandleROS2ParentName(UCarlaEpisode* Episode, FCarlaActor* CarlaActor, FCarlaActor* ParentCarlaActor)
{
#if defined(WITH_ROS2)
    auto ros2 = carla::ros2::ROS2::GetInstance();
    if (ros2->IsEnabled())
    {
        FCarlaActor* currentActor = ParentCarlaActor;
        while (currentActor)
        {
            for (const auto& attr : currentActor->GetActorInfo()->Description.Variations)
            {
                if (attr.Key == "ros_name")
                {
                    const std::string value = std::string(TCHAR_TO_UTF8(*attr.Value.Value));
                    ros2->AddActorParentRosName(static_cast<void*>(CarlaActor->GetActor()), static_cast<void*>(currentActor->GetActor()));
                }
            }
            currentActor = Episode->FindCarlaActor(currentActor->GetParent());
        }
    }
#endif
}
