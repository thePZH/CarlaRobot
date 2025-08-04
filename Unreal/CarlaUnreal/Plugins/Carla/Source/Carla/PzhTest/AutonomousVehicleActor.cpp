#include "AutonomousVehicleActor.h"
#include "Carla/Game/CarlaEpisode.h"
#include "Carla/Game/CarlaStatics.h"
#include "Carla/Actor/ActorBlueprintFunctionLibrary.h"
#include "Carla/Actor/VehicleParameters.h"
#include "Carla/Actor/ActorDescription.h"
#include "Carla/Actor/ActorAttribute.h"
#include "Carla/Actor/CarlaActor.h"
#include "Engine/World.h"

AAutonomousVehicleActor::AAutonomousVehicleActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAutonomousVehicleActor::BeginPlay()
{
	Super::BeginPlay();
	SpawnVehicleWithSensor();
}

void AAutonomousVehicleActor::SpawnVehicleWithSensor()
{
    // 1. 获取当前 CARLA Episode
    UCarlaEpisode* CarlaEpisode = UCarlaStatics::GetCurrentEpisode(GetWorld());
    if (!CarlaEpisode) 
    {
        UE_LOG(LogCarla, Error, TEXT("Failed to get CarlaEpisode"));
        return;
    }

    // 2. 准备车辆生成参数
    FActorDescription VehicleDesc;
    VehicleDesc.Id = TEXT("vehicle.robot.01");
	bool found = false;
	for	(const auto& def : CarlaEpisode->GetActorDefinitions())
	{
		if (def.Id.Equals(VehicleDesc.Id, ESearchCase::IgnoreCase))
		{
			VehicleDesc.UId = def.UId;
			found = true;
			break;
		}
	}
    if (!found)
    {
    	UE_LOG(LogCarla, Error, TEXT("can not find %s"), *VehicleDesc.Id);
    	return;
    }
    // VehicleDesc.Class = LoadClass<AActor>(nullptr, TEXT(
    // 	"/Script/Engine.Blueprint'/Game/Carla/Blueprints/Vehicles/1RobotWheeled/BP_4wheeleRobot.BP_4wheeleRobot_C'"
    // 	));
    // 属性
    FActorAttribute RoleAttr;
    RoleAttr.Id = TEXT("role_name");
    RoleAttr.Type = EActorAttributeType::String;
    RoleAttr.Value = TEXT("hero");
    VehicleDesc.Variations.Add(RoleAttr.Id, RoleAttr);

	RoleAttr.Id = TEXT("ros_name");
	RoleAttr.Value = TEXT("ego");
	VehicleDesc.Variations.Add(RoleAttr.Id, RoleAttr);

	// 3. 生成车辆
    FTransform VehicleSpawnPoint;
    VehicleSpawnPoint.SetLocation(FVector(0.f, 0.0f, 50.0f));
    
    auto VehicleSpawnResult = CarlaEpisode->SpawnActorWithInfo(VehicleSpawnPoint, VehicleDesc);
    
    if (VehicleSpawnResult.Key != EActorSpawnResultStatus::Success)
    {	
        UE_LOG(LogCarla, Error, TEXT("Failed to spawn vehicle: %s"), 
            *FActorSpawnResult::StatusToString(VehicleSpawnResult.Key));
        return;
    }
    
    ACarlaWheeledVehicle* Vehicle = Cast<ACarlaWheeledVehicle>(VehicleSpawnResult.Value->GetActor());
    if (!Vehicle) 
    {
        UE_LOG(LogCarla, Error, TEXT("Spawned actor is not a vehicle"));
        return;
    }

    // 4. 获取车辆的 CarlaActor 表示
    FCarlaActor* VehicleCarlaActor = CarlaEpisode->FindCarlaActor(VehicleSpawnResult.Value->GetActorId());
    if (!VehicleCarlaActor) 
    {
        UE_LOG(LogCarla, Error, TEXT("Failed to find CarlaActor for vehicle"));
        return;
    }

    // 5. 准备传感器生成参数
    FActorDescription SensorDesc;
    SensorDesc.Id = TEXT("sensor.camera.rgb");
	found = false;
	for	(const auto& def : CarlaEpisode->GetActorDefinitions())
	{
		if (def.Id.Equals(SensorDesc.Id, ESearchCase::IgnoreCase))
		{
			SensorDesc.UId = def.UId;
			found = true;
			break;
		}
	}
	if (!found)
	{
		UE_LOG(LogCarla, Error, TEXT("can not find %s relavent blueprint"), *SensorDesc.Id);
		return;
	}
	
    // 传感器属性
    TArray<FActorAttribute> SensorAttributes;
    
    FActorAttribute Attr;
    Attr.Id = TEXT("image_size_x");
    Attr.Type = EActorAttributeType::Int;
    Attr.Value = TEXT("800");
    SensorAttributes.Add(Attr);
    
    Attr.Id = TEXT("image_size_y");
    Attr.Type = EActorAttributeType::Int;
    Attr.Value = TEXT("600");
    SensorAttributes.Add(Attr);
    
    Attr.Id = TEXT("fov");
    Attr.Type = EActorAttributeType::Float;
    Attr.Value = TEXT("90.0");
    SensorAttributes.Add(Attr);

    // 添加 role_name 属性
    Attr.Id = TEXT("ros_name");
    Attr.Type = EActorAttributeType::String;
    Attr.Value = TEXT("front_camera");
    SensorAttributes.Add(Attr);
	
    for (const FActorAttribute& Attribute : SensorAttributes)
    {
        SensorDesc.Variations.Add(Attribute.Id, Attribute);
    }

    // 6. 生成传感器
    FTransform SensorTransform;
    SensorTransform.SetLocation(FVector(200.0f, 0.0f, 140.0f)); // 车顶位置
    
    auto SensorSpawnResult = CarlaEpisode->SpawnActorWithInfo(SensorTransform, SensorDesc);
    
    if (SensorSpawnResult.Key != EActorSpawnResultStatus::Success)
    {
        UE_LOG(LogCarla, Error, TEXT("Failed to spawn sensor: %s"), 
            *FActorSpawnResult::StatusToString(SensorSpawnResult.Key));
        return;
    }
    
    ASensor* Sensor = Cast<ASensor>(SensorSpawnResult.Value->GetActor());
    if (!Sensor) 
    {
        UE_LOG(LogCarla, Error, TEXT("Spawned actor is not a sensor"));
        return;
    }

	// ============== 手动设置父子关系和 处理ROS2 ============== 
	// 7.获取传感器的 CarlaActor 表示
    FCarlaActor* SensorCarlaActor = CarlaEpisode->FindCarlaActor(SensorSpawnResult.Value->GetActorId());
    if (!SensorCarlaActor) 
    {
        UE_LOG(LogCarla, Error, TEXT("Failed to find CarlaActor for sensor"));
        return;
    }

    // 8. 设置父子关系
    uint32 VehicleActorID = VehicleCarlaActor->GetActorId();
    SensorCarlaActor->SetParent(VehicleActorID);
    SensorCarlaActor->SetAttachmentType(carla::rpc::AttachmentType::Rigid);
    VehicleCarlaActor->AddChildren(SensorCarlaActor->GetActorId());

    // 9. ROS2 处理 (如果启用)
#if defined(WITH_ROS2)
    auto ROS2 = carla::ros2::ROS2::GetInstance();
    if (ROS2->IsEnabled()) 
    {
        FCarlaActor* CurrentActor = VehicleCarlaActor;
        while(CurrentActor) 
        {
            for (const auto &Attr : CurrentActor->GetActorInfo()->Description.Variations) 
            {
                if (Attr.Key == "ros_name") 
                {
                    const std::string value = std::string(TCHAR_TO_UTF8(*Attr.Value.Value));
                    ROS2->AddActorParentRosName(
                        static_cast<void*>(SensorCarlaActor->GetActor()), 
                        static_cast<void*>(CurrentActor->GetActor())
                    );
                }
            }
            CurrentActor = CarlaEpisode->FindCarlaActor(CurrentActor->GetParent());
        }
    }
#endif

    // 10. 处理休眠状态
    if (!VehicleCarlaActor->IsDormant()) 
    {
        // 立即附加传感器到车辆
        CarlaEpisode->AttachActors(
            SensorCarlaActor->GetActor(),
            VehicleCarlaActor->GetActor(),
            EAttachmentType::Rigid
        );
    } 
    else 
    {
        // 车辆休眠，传感器也进入休眠
        CarlaEpisode->PutActorToSleep(SensorCarlaActor->GetActorId());
    }
    
    // 11. 设置数据回调（如果需要）
    // Sensor->Listen([](const carla::sensor::SensorData& data) {
    //     // 处理传感器数据
    // });
}