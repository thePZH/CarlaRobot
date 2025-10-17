#pragma once

#include "CoreMinimal.h"
#include "Carla/Game/CarlaEpisode.h"
#include "carla/rpc/Actor.h"
#include "carla/rpc/ActorDescription.h"
#include "carla/rpc/Transform.h"
#include "carla/rpc/AttachmentType.h"
#include "carla/rpc/ActorId.h"
#include "Carla/Actor/CarlaActor.h"
#include "Carla/Server/CarlaServerResponse.h"
#include "carla/rpc/VehicleControl.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"


class FCarlaActor;

class CARLA_API SvcRobotLogic
{
public:
	// 入口：根据Json描述创建Vehicle及其传感器，返回结果JSON字符串（ok/ids等）
	static FString CreateRobot(UCarlaEpisode* Episode, const FString& JsonString);
	static ECarlaServerResponse ApplyControlToRobot(UCarlaEpisode* Episode, carla::rpc::ActorId ActorId, const carla::rpc::VehicleControl& Control);
	// 销毁机器人（车辆及其所有子传感器）
	static bool DestroyRobot(UCarlaEpisode* Episode, carla::rpc::ActorId RobotActorId);
	
private:
	// JSON解析与校验
	static bool ParseJson(const FString& JsonString, TSharedPtr<FJsonObject>& OutRoot, FString& OutError);

	// 工具：米->厘米
	static inline float MetersToCentimeters(float meters) { return meters * 100.0f; }

	// 从Json生成carla::rpc::Transform（世界/相对）
	static carla::rpc::Transform BuildTransformFromJson(const TSharedPtr<FJsonObject>& JsonTransform);

	// 从Json构建ActorDescription（蓝图+属性）
	static bool BuildDescriptionFromJson(const FString& Blueprint, const TSharedPtr<FJsonObject>& AttrsObj, carla::rpc::ActorDescription& OutDesc);
	
	// 通过Episode查找完整的ActorDefinition信息
	static bool FindActorDefinitionByBlueprintId(UCarlaEpisode* Episode, const FString& BlueprintId, carla::rpc::ActorDescription& OutDesc);

	// 生成Vehicle（spawn_actor）
	static FCarlaActor* SpawnVehicle(UCarlaEpisode* Episode, const TSharedPtr<FJsonObject>& VehicleObj);

	// 生成并挂载传感器（spawn_actor_with_parent）
	static FCarlaActor* SpawnAndAttachSensor(UCarlaEpisode* Episode, const TSharedPtr<FJsonObject>& SensorObj, FCarlaActor* ParentCarlaActor, const TMap<FString, FCarlaActor*>& NameToActor);

	// 应用可选物理设置
	// static void ApplyVehiclePhysicsIfAny(UCarlaEpisode* Episode, FCarlaActor* Vehicle, const TSharedPtr<FJsonObject>& VehicleObj);

	// 构建返回结果JSON
	static FString BuildResultJson(bool bOk, const FString& Error, int32 VehicleId, const TArray<TTuple<FString,int32,FString>>& SensorInfos);
};
