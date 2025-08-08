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

void ActorROS2Handler::operator()(carla::ros2::VehicleControl &Source)
{
  if (!_Actor) return;

  ACarlaWheeledVehicle *Vehicle = Cast<ACarlaWheeledVehicle>(_Actor);
  if (!Vehicle) return;

  // setup control values
  FVehicleControl NewControl;
  NewControl.Throttle = Source.throttle;
  NewControl.Brake = Source.brake;
  NewControl.bHandBrake = Source.hand_brake;
  NewControl.bReverse = Source.reverse;
  NewControl.bManualGearShift = Source.manual_gear_shift;
  NewControl.Gear = Source.gear;
	FVector v = _Actor->GetVelocity();
	float speed = FMath::Sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);	
	if (speed < 0.01)
	{
		float clampedRot = FMath::Clamp(Source.steer, 0.f, 1.f);
		_Actor->AddActorWorldRotation({0, 0, Source.steer * 2});
	}
	else
	{
		NewControl.Steer = Source.steer;
	}

  Vehicle->ApplyVehicleControl(NewControl, EVehicleInputPriority::User);
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
