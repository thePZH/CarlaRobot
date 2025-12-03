// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include "ActorROS2Handler.h"
#include "Carla/Vehicle/CarlaWheeledVehicle.h"
#include "Carla/Vehicle/VehicleControl.h"
#include "PzhTest/WheeledRobotAnimationInstance.h"
#include "carla/ros2/ROS2CallbackData.h"
#include "Carla/Sensor/SceneCaptureSensor.h"
#include "Carla/Game/CarlaStatics.h"
#include "Carla/Actor/CarlaActor.h"
#include "Kismet/KismetMathLibrary.h"

void ActorROS2Handler::operator()(carla::ros2::VehicleControl &Source)
{
	if (!_Actor) return;

	ACarlaWheeledVehicle *Vehicle = Cast<ACarlaWheeledVehicle>(_Actor);
	if (!Vehicle) return;

	// 获取 UCarlaEpisode 和 FCarlaActor
	UCarlaEpisode* Episode = UCarlaStatics::GetCurrentEpisode(_Actor);
	if (!Episode) return;

	FCarlaActor* CarlaActor = Episode->FindCarlaActor(_Actor);
	if (!CarlaActor) return;

	// throttle 字段存储线速度 (m/s)
	// steer 字段存储角速度 (rad/s)
	const float linear_velocity_mps = Source.throttle; // m/s
	const float angular_velocity_radps = Source.steer; // rad/s

	// 设置线速度：将速度向量设置为车辆前进方向
	if (linear_velocity_mps > 0.01f)
	{
		// 获取车辆的前进方向（本地坐标系）
		FVector forward_direction = _Actor->GetActorForwardVector();
		// 将速度从 m/s 转换为 cm/s（UE使用厘米）
		FVector velocity_cmps = forward_direction * (linear_velocity_mps * 100.0f);
		CarlaActor->SetActorTargetVelocity(velocity_cmps);
	}
	else
	{
		// 速度为零时，停止车辆
		CarlaActor->SetActorTargetVelocity(FVector::ZeroVector);
	}

	// 设置角速度：将角速度从 rad/s 转换为 deg/s（UE使用度数）
	// 角速度在Z轴（垂直向上），正值表示逆时针旋转
	FVector angular_velocity_degps = FVector(0.0f, 0.0f, FMath::RadiansToDegrees(angular_velocity_radps));
	CarlaActor->SetActorTargetAngularVelocity(angular_velocity_degps);
}

void ActorROS2Handler::operator()(carla::ros2::GimbalControl& Msg)
{
	if (_Actor)
	{
		if (auto* carlaVehicle = Cast<ACarlaWheeledVehicle>(_Actor))
		{
			if (auto* skmComp = _Actor->GetComponentByClass<USkeletalMeshComponent>())
			{
				if (auto* animInstance = skmComp->GetAnimInstance())
				{
					if (auto anim = Cast<UWheeledRobotAnimationInstance>(animInstance))
					{
						anim->CameraPitch = Msg.pitch;
						anim->Gimbalyaw = Msg.yaw;
					}
				}
			}
		}
	}
	// fov设置，暂时用twist.linear.x吧
	TArray<AActor*> actors;
	_Actor->GetAttachedActors(actors);
	for (auto actor : actors)
	{
		ASceneCaptureSensor* Camera = Cast<ASceneCaptureSensor>(actor);
		if (!Camera)
			return;
		std::cout << "[ActorROS2Handler]ros2 fov:" << Msg.fov << std::endl;
		Camera->SetFOVAngle(Msg.fov);
	}
}

void ActorROS2Handler::operator()(carla::ros2::MessageControl Message)
{
  if (!_Actor) return;

  ACarlaWheeledVehicle *Vehicle = Cast<ACarlaWheeledVehicle>(_Actor);
  if (!Vehicle) return;

  Vehicle->PrintROS2Message(Message.message);

  // FString ROSMessage = Message.message;
  // UE_LOG(LogCarla, Warning, TEXT("ROS2 Message received: %s"), *ROSMessage);
}
