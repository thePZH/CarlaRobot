// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include "Carla/Sensor/RayCastLidar.h"
#include "Carla.h"
#include "Carla/Actor/ActorBlueprintFunctionLibrary.h"

#include <util/disable-ue4-macros.h>
#include "carla/geom/Math.h"
#include "carla/ros2/ROS2.h"
#include "carla/geom/Location.h"
#include <util/enable-ue4-macros.h>

#include <util/ue-header-guard-begin.h>
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Kismet/KismetMathLibrary.h"
#include <util/ue-header-guard-end.h>
// #include "LidarPointCloudShared.h"

#include <cmath>

FActorDefinition ARayCastLidar::GetSensorDefinition()
{
  return UActorBlueprintFunctionLibrary::MakeLidarDefinition(TEXT("ray_cast"));
}


ARayCastLidar::ARayCastLidar(const FObjectInitializer& ObjectInitializer)
  : Super(ObjectInitializer) {

  RandomEngine = CreateDefaultSubobject<URandomEngine>(TEXT("RandomEngine"));
  SetSeed(Description.RandomSeed);
}

void ARayCastLidar::Set(const FActorDescription &ActorDescription)
{
  ASensor::Set(ActorDescription);
  FLidarDescription LidarDescription;
  UActorBlueprintFunctionLibrary::SetLidar(ActorDescription, LidarDescription);
  Set(LidarDescription);
}

void ARayCastLidar::Set(const FLidarDescription &LidarDescription)
{
  Description = LidarDescription;
  LidarData = FLidarData(Description.Channels);
  CreateLasers();
  PointsPerChannel.resize(Description.Channels);

  // Compute drop off model parameters
  DropOffBeta = 1.0f - Description.DropOffAtZeroIntensity;
  DropOffAlpha = Description.DropOffAtZeroIntensity / Description.DropOffIntensityLimit;
  DropOffGenActive = Description.DropOffGenRate > std::numeric_limits<float>::epsilon();
}

void ARayCastLidar::PostPhysTick(UWorld *World, ELevelTick TickType, float DeltaTime)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(ARayCastLidar::PostPhysTick);
  SimulateLidar(DeltaTime);
// 	int MaxPoiontsNum = FMath::RoundHalfFromZero(Description.PointsPerSecond * DeltaTime);
// 	FRandomStream RandomStream(FMath::Rand());
// #if WITH_EDITOR
// 	PointsCloudPos.Empty();
// 	PointsCloudPos.Reserve(MaxPoiontsNum);
// 	
// 	for (const auto& channelhits : RecordedHits)
// 	{
// 		for	(const auto& hit : channelhits)
// 		{
//
// 			FLidarPointCloudPoint point;
// 			point.Location = {static_cast<float>(hit.Location.X), static_cast<float>(hit.Location.Y), static_cast<float>(hit.Location.Z)};
// 			point.Color = {0, 100, 255, 255};
// 			PointsCloudPos.Emplace(point);
// 			
// 			if (!bDrawDebugLine && !bDrawDebugPlane)
// 				continue;
// 			if (FMath::FRand() > Threshold)
// 				continue;
// 			
// 			double dist = FVector::Dist(hit.Location, GetActorLocation());
// 			float clampedDist = FMath::Clamp(static_cast<float>(dist), 0.0f, MaxDist);
// 			float hue = FMath::Loge(1.0f + clampedDist * LogFactor) / FMath::Loge(1.0f + MaxDist * LogFactor) * MaxHue;
// 			FLinearColor hsvColor(hue, 1.0f, 1.0f, 1.0f);
// 			FLinearColor linearColor = hsvColor.HSVToLinearRGB();
//
// 			if (bDrawDebugLine)
// 			{
// 				DrawDebugLine(GetWorld(), GetActorLocation(), hit.Location, linearColor.ToFColor(false), false, -1, 0, 0);
// 			}
// 			if (bDrawDebugPlane)
// 			{
// 				FPlane plane(hit.Location, hit.Normal);
// 				DrawDebugSolidPlane(
// 					GetWorld(),
// 					plane,				
// 					hit.Location,                               
// 					FVector2D(DebugPointSize, DebugPointSize),
// 					 linearColor.ToFColor(false),
// 					false,                                      
// 					-1,
// 					0
// 				);
// 			}
// 		}
// 	}
// #endif
  auto DataStream = GetDataStream(*this);
  auto SensorTransform = DataStream.GetSensorTransform();

  {
    TRACE_CPUPROFILER_EVENT_SCOPE_STR("Send Stream");
    DataStream.SerializeAndSend(*this, LidarData, DataStream.PopBufferFromPool());
  }
  // ROS2
  #if defined(WITH_ROS2)
  auto ROS2 = carla::ros2::ROS2::GetInstance();
  if (ROS2->IsEnabled())
  {
    TRACE_CPUPROFILER_EVENT_SCOPE_STR("ROS2 Send");
    auto StreamId = carla::streaming::detail::token_type(GetToken()).get_stream_id();
    AActor* ParentActor = GetAttachParentActor();
    if (ParentActor)
    {
      FTransform LocalTransformRelativeToParent = GetActorTransform().GetRelativeTransform(ParentActor->GetActorTransform());
      ROS2->ProcessDataFromLidar(DataStream.GetSensorType(), StreamId, LocalTransformRelativeToParent, LidarData, this);
    }
    else
    {
      ROS2->ProcessDataFromLidar(DataStream.GetSensorType(), StreamId, SensorTransform, LidarData, this);
    }
  }
  #endif


}

float ARayCastLidar::ComputeIntensity(const FSemanticDetection& RawDetection) const
{
  const carla::geom::Location HitPoint = RawDetection.point;
  const float Distance = HitPoint.Length();

  const float AttenAtm = Description.AtmospAttenRate;
  const float AbsAtm = exp(-AttenAtm * Distance);

  const float IntRec = AbsAtm;

  return IntRec;
}

ARayCastLidar::FDetection ARayCastLidar::ComputeDetection(const FHitResult& HitInfo, const FTransform& SensorTransf) const
{
  FDetection Detection;
  const FVector HitPoint = HitInfo.ImpactPoint;
  Detection.point = SensorTransf.Inverse().TransformPosition(HitPoint);

  const float Distance = Detection.point.Length();

  const float AttenAtm = Description.AtmospAttenRate;
  const float AbsAtm = exp(-AttenAtm * Distance);

  const float IntRec = AbsAtm;

  Detection.intensity = IntRec;

  return Detection;
}

  void ARayCastLidar::PreprocessRays(uint32_t Channels, uint32_t MaxPointsPerChannel) {
    Super::PreprocessRays(Channels, MaxPointsPerChannel);

    for (auto ch = 0u; ch < Channels; ch++) {
      for (auto p = 0u; p < MaxPointsPerChannel; p++) {
        RayPreprocessCondition[ch][p] = !(DropOffGenActive && RandomEngine->GetUniformFloat() < Description.DropOffGenRate);
      }
    }
  }

  bool ARayCastLidar::PostprocessDetection(FDetection& Detection) const
  {
    if (Description.NoiseStdDev > std::numeric_limits<float>::epsilon()) {
      const auto ForwardVector = Detection.point.MakeUnitVector();
      const auto Noise = ForwardVector * RandomEngine->GetNormalDistribution(0.0f, Description.NoiseStdDev);
      Detection.point += Noise;
    }

    const float Intensity = Detection.intensity;
    if(Intensity > Description.DropOffIntensityLimit)
      return true;
    else
      return RandomEngine->GetUniformFloat() < DropOffAlpha * Intensity + DropOffBeta;
  }

  void ARayCastLidar::ComputeAndSaveDetections(const FTransform& SensorTransform) {
    for (auto idxChannel = 0u; idxChannel < Description.Channels; ++idxChannel)
      PointsPerChannel[idxChannel] = RecordedHits[idxChannel].size();

    LidarData.ResetMemory(PointsPerChannel);
#if WITH_EDITOR
    if(bSavingDataToDisk)
    {
      PointCloudResetMemory();
    }
#endif

    // 简化版本：仅保存点的空间位置和强度，ring/time 交给 ROS2 侧在接收后按 header 信息估算
	for (auto idxChannel = 0u; idxChannel < Description.Channels; ++idxChannel)
	{
		for (auto& hit : RecordedHits[idxChannel])
		{
			FDetection Detection = ComputeDetection(hit, SensorTransform);
			if (PostprocessDetection(Detection))
			{
				LidarData.WritePointSync(Detection);
#if WITH_EDITOR
				if(bSavingDataToDisk)
				{
					PointCloudWritePointSync(Detection);
				}
#endif
			}
			else
			{
				PointsPerChannel[idxChannel]--;
			}
		}
	}

    LidarData.WriteChannelCount(PointsPerChannel);
  }

void ARayCastLidar::PointCloudResetMemory()
{
  PointCloudLidarData.Empty();
  PointCloudLidarData.Reserve(static_cast<uint32_t>(std::accumulate(PointsPerChannel.begin(), PointsPerChannel.end(), 0)) * 4);
}

void ARayCastLidar::PointCloudWritePointSync(const FDetection& Detection)
{
  PointCloudLidarData.Emplace(Detection.point.x);
  PointCloudLidarData.Emplace(Detection.point.y);
  PointCloudLidarData.Emplace(Detection.point.z);
  PointCloudLidarData.Emplace(Detection.intensity);
}
