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
	float linear_velocity_mps = Source.throttle; // m/s
	float angular_velocity_radps = Source.steer; // rad/s

	// 直接设置线速度与角速度；接口在物理侧会保持目标速度
	FVector forward_direction = _Actor->GetActorForwardVector();
	FVector velocity_cmps = forward_direction * (linear_velocity_mps * 100.0f); // cm/s
	// 将线速度保存到车辆，由Tick每帧应用，保证低频消息也能持续
	Vehicle->SetRos2LinearVelocity(velocity_cmps);
	// 角速度同理
	Vehicle->SetRos2AngularVelocity({0,0,angular_velocity_radps});
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
