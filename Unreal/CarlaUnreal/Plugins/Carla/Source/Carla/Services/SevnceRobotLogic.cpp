#include "SevnceRobotLogic.h"
#include "Carla/Actor/CarlaActor.h"
#include "Carla/Game/CarlaEpisode.h"
#include "Carla/Game/CarlaStatics.h"
#include "Carla/MapGen/LargeMapManager.h"
#include "Carla/Actor/ActorDefinition.h"
#include "Carla/Actor/ActorAttribute.h"
#include "Carla/Services/SevnceActorLogic.h"

#include "Carla/Vehicle/VehicleInputPriority.h"

FString SvcRobotLogic::CreateRobot(UCarlaEpisode* Episode, const FString& JsonString)
{
    TSharedPtr<FJsonObject> root;
    FString error;
    TArray<TTuple<FString,int32,FString>> sensorInfos; // name, id, type
    int32 vehicleId = 0;

    if (!ParseJson(JsonString, root, error))
	{
        return BuildResultJson(false, error, 0, sensorInfos);
	}

	// vehicle
    const TSharedPtr<FJsonObject>* vehicleObjPtr = nullptr;
    if (!root->TryGetObjectField(TEXT("robot"), vehicleObjPtr) || !vehicleObjPtr || !(*vehicleObjPtr).IsValid())
	{
        return BuildResultJson(false, TEXT("missing vehicle"), 0, sensorInfos);
	}

    FCarlaActor* vehicleCarlaActor = SpawnVehicle(Episode, *vehicleObjPtr);
    if (!vehicleCarlaActor)
	{
        return BuildResultJson(false, TEXT("spawn vehicle failed"), 0, sensorInfos);
	}
    vehicleId = vehicleCarlaActor->GetActorId();

	// map name->actor for chaining
    TMap<FString, FCarlaActor*> nameToActor;
    nameToActor.Add(TEXT("robot"), vehicleCarlaActor);

	// sensors
    const TArray<TSharedPtr<FJsonValue>>* sensorsArray = nullptr;
    if (root->TryGetArrayField(TEXT("sensors"), sensorsArray) && sensorsArray)
	{
        for (const TSharedPtr<FJsonValue>& sensorVal : *sensorsArray)
		{
            const TSharedPtr<FJsonObject>* sensorObjPtr = nullptr;
            if (!sensorVal.IsValid() || !sensorVal->TryGetObject(sensorObjPtr) || !sensorObjPtr || !(*sensorObjPtr).IsValid())
			{
                error = TEXT("invalid sensor entry");
                return BuildResultJson(false, error, vehicleId, sensorInfos);
			}
            FCarlaActor* parent = vehicleCarlaActor;
            FString parentName;
            if (const TSharedPtr<FJsonObject>* attachObj = nullptr; (*sensorObjPtr)->TryGetObjectField(TEXT("attach"), attachObj) && attachObj && (*attachObj).IsValid())
			{
                (*attachObj)->TryGetStringField(TEXT("parent"), parentName);
                if (!parentName.IsEmpty())
				{
                    if (FCarlaActor** found = nameToActor.Find(parentName))
					{
                        parent = *found;
					}
				}
			}

            FCarlaActor* sensor = SpawnAndAttachSensor(Episode, *sensorObjPtr, parent, nameToActor);
            if (!sensor)
			{
                return BuildResultJson(false, TEXT("spawn sensor failed"), vehicleId, sensorInfos);
			}

            FString sensorName; (*sensorObjPtr)->TryGetStringField(TEXT("name"), sensorName);
            FString bp; (*sensorObjPtr)->TryGetStringField(TEXT("blueprint"), bp);
            sensorInfos.Emplace(sensorName, sensor->GetActorId(), bp);
            if (!sensorName.IsEmpty())
			{
                nameToActor.Add(sensorName, sensor);
			}
		}
	}

    return BuildResultJson(true, TEXT(""), vehicleId, sensorInfos);
}

bool SvcRobotLogic::DestroyRobot(UCarlaEpisode* Episode, carla::rpc::ActorId RobotActorId)
{
    if (!Episode)
        return false;

    FCarlaActor* Robot = Episode->FindCarlaActor(RobotActorId);
    if (!Robot)
        return false;

    // 收集所有子传感器（深度一层：直接孩子）
    TArray<carla::rpc::ActorId> Children;
    const TArray<carla::rpc::ActorId>& Childs = Robot->GetChildren();
    for (auto Id : Childs)
        Children.Add(Id);

    // 先销毁子传感器
    for (auto ChildId : Children)
    {
        Episode->DestroyActor(ChildId);
    }

    // 再销毁车辆自身
    if (!Episode->DestroyActor(RobotActorId))
        return false;

    return true;
}

ECarlaServerResponse SvcRobotLogic::ApplyControlToRobot(UCarlaEpisode* Episode, carla::rpc::ActorId ActorId, const carla::rpc::VehicleControl& Control)
{
	if (!Episode)
	{
		UE_LOG(LogCarla, Error, TEXT("SvcVehicleLogic::ApplyControlToVehicle: Episode is null"));
		return ECarlaServerResponse::Failure;
	}

	FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
	if (!CarlaActor)
	{
		UE_LOG(LogCarla, Error, TEXT("SvcVehicleLogic::ApplyControlToVehicle: Actor not found - Actor Id: %d"), ActorId);
		return ECarlaServerResponse::ActorNotFound;
	}

	ECarlaServerResponse Response = CarlaActor->ApplyControlToVehicle(Control, EVehicleInputPriority::Client);
	if (Response != ECarlaServerResponse::Success)
	{
		UE_LOG(LogCarla, Error, TEXT("SvcVehicleLogic::ApplyControlToVehicle: Failed to apply control - Actor Id: %d"), ActorId);
	}

	return Response;
}

bool SvcRobotLogic::ParseJson(const FString& JsonString, TSharedPtr<FJsonObject>& OutRoot, FString& OutError)
{
    TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(reader, OutRoot) || !OutRoot.IsValid())
	{
        OutError = TEXT("invalid json");
		return false;
	}
	return true;
}

carla::rpc::Transform SvcRobotLogic::BuildTransformFromJson(const TSharedPtr<FJsonObject>& JsonTransform)
{
    carla::rpc::Transform t;
    FVector loc = FVector::ZeroVector;
    FRotator rot = FRotator::ZeroRotator;
    if (const TSharedPtr<FJsonObject>* locObj = nullptr; JsonTransform->TryGetObjectField(TEXT("location"), locObj) && locObj && (*locObj).IsValid())
	{
        double x=0,y=0,z=0; (*locObj)->TryGetNumberField(TEXT("x"), x); (*locObj)->TryGetNumberField(TEXT("y"), y); (*locObj)->TryGetNumberField(TEXT("z"), z);
        loc = FVector(MetersToCentimeters(x), MetersToCentimeters(y), MetersToCentimeters(z));
	}
    if (const TSharedPtr<FJsonObject>* rotObj = nullptr; JsonTransform->TryGetObjectField(TEXT("rotation"), rotObj) && rotObj && (*rotObj).IsValid())
	{
        double p=0,yaw=0,r=0; (*rotObj)->TryGetNumberField(TEXT("pitch"), p); (*rotObj)->TryGetNumberField(TEXT("yaw"), yaw); (*rotObj)->TryGetNumberField(TEXT("roll"), r);
        rot = FRotator(p, yaw, r);
	}
    t.location = loc; t.rotation = rot;
    return t;
}

bool SvcRobotLogic::BuildDescriptionFromJson(const FString& Blueprint, const TSharedPtr<FJsonObject>& AttrsObj, carla::rpc::ActorDescription& OutDesc)
{
    OutDesc = carla::rpc::ActorDescription();
    OutDesc.id = TCHAR_TO_UTF8(*Blueprint);

	if (AttrsObj.IsValid())
	{
        for (const auto& kvp : AttrsObj->Values)
		{
            FString key = kvp.Key;
            FString valueStr;
            if (kvp.Value->TryGetString(valueStr))
			{
				carla::rpc::ActorAttribute Attr;
                Attr.id = TCHAR_TO_UTF8(*key);
				Attr.type = carla::rpc::ActorAttributeType::String;
                Attr.value = TCHAR_TO_UTF8(*valueStr);
				OutDesc.attributes.emplace_back(Attr);
			}
		}
	}
	return true;
}

bool SvcRobotLogic::FindActorDefinitionByBlueprintId(UCarlaEpisode* Episode, const FString& BlueprintId, carla::rpc::ActorDescription& OutDesc)
{
	if (!Episode)
	{
		return false;
	}
	
	// 获取所有ActorDefinitions
	const TArray<FActorDefinition>& ActorDefinitions = Episode->GetActorDefinitions();
	
	// 查找匹配的蓝图ID
	for (const FActorDefinition& ActorDef : ActorDefinitions)
	{
		if (ActorDef.Id == BlueprintId)
		{
			// 找到匹配的蓝图，构建完整的ActorDescription
			OutDesc.uid = ActorDef.UId;
			OutDesc.id = TCHAR_TO_UTF8(*ActorDef.Id);
			
			// 清空并合并 Variations + Attributes，保证完整属性（含推荐值）
			OutDesc.attributes.clear();
			OutDesc.attributes.reserve(ActorDef.Variations.Num() + ActorDef.Attributes.Num());
			for (const FActorVariation& Var : ActorDef.Variations)
			{
				carla::rpc::ActorAttribute RpcVar(Var);
				OutDesc.attributes.emplace_back(RpcVar);
			}
			for (const FActorAttribute& Attr : ActorDef.Attributes)
			{
				carla::rpc::ActorAttribute RpcAttr(Attr);
				OutDesc.attributes.emplace_back(RpcAttr);
			}
			
			return true;
		}
	}
	
	return false;
}

FCarlaActor* SvcRobotLogic::SpawnVehicle(UCarlaEpisode* Episode, const TSharedPtr<FJsonObject>& VehicleObj)
{
    FString blueprint; if (!VehicleObj->TryGetStringField(TEXT("blueprint"), blueprint)) return nullptr;
    TSharedPtr<FJsonObject> attrs; if (const TSharedPtr<FJsonObject>* aPtr = nullptr; VehicleObj->TryGetObjectField(TEXT("attributes"), aPtr) && aPtr) attrs = *aPtr;
    TSharedPtr<FJsonObject> transObj; if (const TSharedPtr<FJsonObject>* tPtr = nullptr; VehicleObj->TryGetObjectField(TEXT("transform"), tPtr) && tPtr) transObj = *tPtr;
    if (!transObj.IsValid()) return nullptr;

    // 首先尝试通过蓝图库查找完整的ActorDefinition
    carla::rpc::ActorDescription desc;
    if (!FindActorDefinitionByBlueprintId(Episode, blueprint, desc))
    {
        // 如果找不到，回退到原来的方法
        BuildDescriptionFromJson(blueprint, attrs, desc);
    }
    else
    {
        // 如果找到了完整的定义，还需要合并JSON中提供的属性
        if (attrs.IsValid())
        {
            for (const auto& kvp : attrs->Values)
            {
                FString key = kvp.Key;
                FString valueStr;
                if (kvp.Value->TryGetString(valueStr))
                {
                    // 查找是否已存在该属性，如果存在则更新，否则添加
                    bool bFound = false;
                    for (auto& existingAttr : desc.attributes)
                    {
                        if (existingAttr.id == TCHAR_TO_UTF8(*key))
                        {
                            existingAttr.value = TCHAR_TO_UTF8(*valueStr);
                            bFound = true;
                            break;
                        }
                    }
                    if (!bFound)
                    {
                        carla::rpc::ActorAttribute Attr;
                        Attr.id = TCHAR_TO_UTF8(*key);
                        Attr.type = carla::rpc::ActorAttributeType::String;
                        Attr.value = TCHAR_TO_UTF8(*valueStr);
                        desc.attributes.emplace_back(Attr);
                    }
                }
            }
        }
    }
    
    carla::rpc::Transform t = BuildTransformFromJson(transObj);
    auto spawnResult = Episode->SpawnActorWithInfo(t, desc);
    if (spawnResult.Key != EActorSpawnResultStatus::Success) return nullptr;

    FCarlaActor* vehicle = spawnResult.Value;
    // ApplyVehiclePhysicsIfAny(Episode, vehicle, VehicleObj);
    return vehicle;
}

FCarlaActor* SvcRobotLogic::SpawnAndAttachSensor(UCarlaEpisode* Episode, const TSharedPtr<FJsonObject>& SensorObj, FCarlaActor* ParentCarlaActor, const TMap<FString, FCarlaActor*>& NameToActor)
{
    if (!ParentCarlaActor) 
        return nullptr;
    FString blueprint;
	if (!SensorObj->TryGetStringField(TEXT("blueprint"), blueprint))
		return nullptr;
	
    TSharedPtr<FJsonObject> attrs;
	if (const TSharedPtr<FJsonObject>* aPtr = nullptr; SensorObj->TryGetObjectField(TEXT("attributes"), aPtr) && aPtr)
		attrs = *aPtr;

    // 首先尝试通过蓝图库查找完整的ActorDefinition
    carla::rpc::ActorDescription desc;
    if (!FindActorDefinitionByBlueprintId(Episode, blueprint, desc))
    {
        // 如果找不到，回退到原来的方法
        BuildDescriptionFromJson(blueprint, attrs, desc);
    }
    else
    {
        // 如果找到了完整的定义，还需要合并JSON中提供的属性
        if (attrs.IsValid())
        {
            for (const auto& kvp : attrs->Values)
            {
                FString key = kvp.Key;
                FString valueStr;
                if (kvp.Value->TryGetString(valueStr))
                {
                    // 查找是否已存在该属性，如果存在则更新，否则添加
                    bool bFound = false;
                    for (auto& existingAttr : desc.attributes)
                    {
                        if (existingAttr.id == TCHAR_TO_UTF8(*key))
                        {
                            existingAttr.value = TCHAR_TO_UTF8(*valueStr);
                            bFound = true;
                            break;
                        }
                    }
                    if (!bFound)
                    {
                        carla::rpc::ActorAttribute Attr;
                        Attr.id = TCHAR_TO_UTF8(*key);
                        Attr.type = carla::rpc::ActorAttributeType::String;
                        Attr.value = TCHAR_TO_UTF8(*valueStr);
                        desc.attributes.emplace_back(Attr);
                    }
                }
            }
        }
    }
    
    // 传感器使用零变换，因为会绑定到父actor
    carla::rpc::Transform t;
    t.location = carla::rpc::Location(0, 0, 0);
    t.rotation = carla::rpc::Rotation(0, 0, 0);

    // 选择挂载类型
    carla::rpc::AttachmentType attachType = carla::rpc::AttachmentType::Rigid;
    if (const TSharedPtr<FJsonObject>* attachObj = nullptr; SensorObj->TryGetObjectField(TEXT("attach"), attachObj) && attachObj && (*attachObj).IsValid())
	{
        FString typeStr; (*attachObj)->TryGetStringField(TEXT("type"), typeStr);
        if (typeStr.Equals(TEXT("SpringArmGhost"), ESearchCase::IgnoreCase))
		{
            attachType = carla::rpc::AttachmentType::SpringArmGhost;
		}
	}

    // 使用SpawnActorWithParent直接创建并绑定传感器
    return SvcActorLogic::SpawnActorWithParent(Episode, desc, t, ParentCarlaActor->GetActorId(), attachType);
}

// void SvcRobotLogic::ApplyVehiclePhysicsIfAny(UCarlaEpisode* Episode, FCarlaActor* Vehicle, const TSharedPtr<FJsonObject>& VehicleObj)
// {
// 	if (!Vehicle) return;
//     const TSharedPtr<FJsonObject>* physObj = nullptr;
//     if (!VehicleObj->TryGetObjectField(TEXT("physics"), physObj) || !physObj || !(*physObj).IsValid()) return;
//     bool useSweep = false; (*physObj)->TryGetBoolField(TEXT("use_sweep_wheel_collision"), useSweep);
//     if (useSweep)
// 	{
// 		Vehicle->EnableSweepWheelCollision(true);
// 	}
// }

FString SvcRobotLogic::BuildResultJson(bool bOk, const FString& Error, int32 VehicleId, const TArray<TTuple<FString,int32,FString>>& SensorInfos)
{
    TSharedPtr<FJsonObject> out = MakeShared<FJsonObject>();
    out->SetBoolField(TEXT("ok"), bOk);
    if (!bOk)
	{
        out->SetStringField(TEXT("error"), TEXT("spawn_failed"));
        out->SetStringField(TEXT("message"), Error);
	}
    if (VehicleId > 0)
    	out->SetNumberField(TEXT("robot_id"), VehicleId);
    TArray<TSharedPtr<FJsonValue>> arr;
    for (const auto& it : SensorInfos)
	{
        TSharedPtr<FJsonObject> s = MakeShared<FJsonObject>();
        s->SetStringField(TEXT("name"), it.Get<0>());
        s->SetNumberField(TEXT("id"), it.Get<1>());
        s->SetStringField(TEXT("type"), it.Get<2>());
        arr.Add(MakeShared<FJsonValueObject>(s));
	}
    out->SetArrayField(TEXT("sensors"), arr);
    FString output; TSharedRef<TJsonWriter<>> w = TJsonWriterFactory<>::Create(&output);
    FJsonSerializer::Serialize(out.ToSharedRef(), w);
    return output;
}
