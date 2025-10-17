#pragma once

#include "CoreMinimal.h"
#include "Carla/Actor/CarlaActor.h"
#include "Carla/Game/CarlaEpisode.h"
#include "Carla/Game/CarlaStatics.h"
#include "Carla/MapGen/LargeMapManager.h"
#include "Carla/Sensor/RayCastLidar.h"
#include "Components/SkeletalMeshComponent.h"
#include "Carla/Server/CarlaServerResponse.h"
#include "carla/rpc/Actor.h"
#include "carla/rpc/ActorDescription.h"
#include "carla/rpc/Transform.h"
#include "carla/rpc/AttachmentType.h"
#include "carla/rpc/ActorId.h"

/**
 * SvcActorLogic - Actor生成和管理相关的静态服务类
 * 提供Actor生成、挂载等功能的静态方法
 */
class CARLA_API SvcActorLogic
{
public:
    /**
     * 生成Actor
     * @param Episode 当前Episode
     * @param Description Actor描述
     * @param Transform 变换信息
     * @return 生成的FCarlaActor指针，失败返回nullptr
     */
    static FCarlaActor* SpawnActor(UCarlaEpisode* Episode, const carla::rpc::ActorDescription& Description, const carla::rpc::Transform& Transform);

    /**
     * 生成带父Actor的Actor
     * @param Episode 当前Episode
     * @param Description Actor描述
     * @param Transform 变换信息
     * @param ParentId 父Actor ID
     * @param InAttachmentType 挂载类型
     * @return 生成的FCarlaActor指针，失败返回nullptr
     */
    static FCarlaActor* SpawnActorWithParent(UCarlaEpisode* Episode, const carla::rpc::ActorDescription& Description, const carla::rpc::Transform& Transform, carla::rpc::ActorId ParentId, carla::rpc::AttachmentType InAttachmentType);

private:
    /**
     * 处理特殊挂载逻辑（骨骼网格插槽挂载）
     * @param CarlaActor 子Actor
     * @param ParentCarlaActor 父Actor
     * @return 是否成功挂载到插槽
     */
    static bool HandleSpecialAttachment(FCarlaActor* CarlaActor, FCarlaActor* ParentCarlaActor);

    /**
     * 处理ROS2相关的父Actor名称设置
     * @param Episode 当前Episode
     * @param CarlaActor 子Actor
     * @param ParentCarlaActor 父Actor
     */
    static void HandleROS2ParentName(UCarlaEpisode* Episode, FCarlaActor* CarlaActor, FCarlaActor* ParentCarlaActor);
};
