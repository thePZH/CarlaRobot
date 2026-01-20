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
  
  // 初始化固定步长参数
  InitializeFixedStepParameters();
}

void ARayCastLidar::InitializeFixedStepParameters()
{
  // 基于每秒点数和传感器tick间隔计算固定步长
  float TotalPointsPerSecond = Description.PointsPerSecond;
  float SensorTickInterval = GetActorTickInterval() > 0.0f ? GetActorTickInterval() : 0.1f;
  
  // 计算每帧每条线的点数
  float PointsPerChannelPerFrame = TotalPointsPerSecond * SensorTickInterval / Description.Channels;
  
  // 计算固定步长：FOV / 每帧每条线的点数
  FixedAngleStep = Description.HorizontalFov / PointsPerChannelPerFrame;
  IsInitialized = true;
}

void ARayCastLidar::PostPhysTick(UWorld *World, ELevelTick TickType, float DeltaTime)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(ARayCastLidar::PostPhysTick);
  
  // 确保已初始化
  if (!IsInitialized)
  {
    InitializeFixedStepParameters();
  }
  
  // 1. 计算每帧需要扫描的点数（基于固定的sensor_tick频率）
  // 获取当前的tick间隔，如果设置了sensor_tick，DeltaTime应该等于sensor_tick
  const float SensorTickInterval = GetActorTickInterval() > 0.0f ? GetActorTickInterval() : 0.1f;
  const uint32 PointsToScanWithOneLaser = FMath::RoundHalfFromZero(
      Description.PointsPerSecond * SensorTickInterval / float(Description.Channels));
  
  // 2. 执行完整FOV扫描：从 -FOV/2 到 +FOV/2
  SimulateFullFOVScan(SensorTickInterval, PointsToScanWithOneLaser);
  
  // 3. 处理和发布数据
  ComputeAndSaveDetections(GetTransform());
  
  // 4. 发送数据流
  auto DataStream = GetDataStream(*this);
  auto SensorTransform = DataStream.GetSensorTransform();
  {
    TRACE_CPUPROFILER_EVENT_SCOPE_STR("Send Stream");
    DataStream.SerializeAndSend(*this, LidarData, DataStream.PopBufferFromPool());
  }
  
  // 5. ROS2发送
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

void ARayCastLidar::SimulateFullFOVScan(const float DeltaTime, const uint32 PointsToScanWithOneLaser)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(ARayCastLidar::SimulateFullFOVScan);
  const uint32 ChannelCount = Description.Channels;

  if (PointsToScanWithOneLaser <= 0)
  {
    UE_LOG(
        LogCarla,
        Warning,
        TEXT("%s: no points requested this frame, try increasing the number of points per second."),
        *GetName());
    return;
  }

  check(ChannelCount == LaserAngles.Num());

  // 计算时间相关参数
  SamplesPerChannelThisFrame = PointsToScanWithOneLaser;
  SecondsPerSample = PointsToScanWithOneLaser > 0u ? DeltaTime / static_cast<float>(PointsToScanWithOneLaser) : 0.0f;

  ResetRecordedHits(ChannelCount, PointsToScanWithOneLaser);
  PreprocessRays(ChannelCount, PointsToScanWithOneLaser);

  auto LockedPhysObject = FPhysicsObjectExternalInterface::LockRead(GetWorld()->GetPhysicsScene());
  {
    TRACE_CPUPROFILER_EVENT_SCOPE(ParallelFor);
    ParallelFor(ChannelCount, [&](int32 idxChannel) {
      TRACE_CPUPROFILER_EVENT_SCOPE(ParallelForTask);

      FCollisionQueryParams TraceParams = FCollisionQueryParams(FName(TEXT("Laser_Trace")), true, this);
      TraceParams.bTraceComplex = true;
      TraceParams.bReturnPhysicalMaterial = false;

      for (auto idxPtsOneLaser = 0u; idxPtsOneLaser < PointsToScanWithOneLaser; idxPtsOneLaser++) {
        FHitResult HitResult;
      	// 当前激光的垂直方向上的角度，天顶角
        const float VertAngle = LaserAngles[idxChannel];
      	// 完整FOV扫描：从 -FOV/2 开始，使用固定步长
        const float HorizAngle = -Description.HorizontalFov / 2.0f + FixedAngleStep * idxPtsOneLaser;
        const bool PreprocessResult = RayPreprocessCondition[idxChannel][idxPtsOneLaser];

        if (PreprocessResult && ShootLaser(VertAngle, HorizAngle, HitResult, TraceParams)) {
          WritePointAsync(idxChannel, idxPtsOneLaser, HitResult);
        }
      };
    });
  }
  LockedPhysObject.Release();

  // 更新基类的角度状态（保持兼容性）
  SemanticLidarData.SetHorizontalAngle(carla::geom::Math::ToRadians(Description.HorizontalFov / 2.0f));
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
  const FVector LocalHitPoint = SensorTransf.Inverse().TransformPosition(HitPoint);
  // 使用 Location(const FVector &) 构造函数进行单位转换（厘米 -> 米）
  Detection.SetPoint(carla::geom::Location(LocalHitPoint));

  const float Distance = Detection.GetPoint().Length();

  const float AttenAtm = Description.AtmospAttenRate;
  const float AbsAtm = exp(-AttenAtm * Distance);

  const float IntRec = AbsAtm;

  Detection.intensity = IntRec * 255.0f;

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
      auto point = Detection.GetPoint();
      const auto ForwardVector = point.MakeUnitVector();
      const auto Noise = ForwardVector * RandomEngine->GetNormalDistribution(0.0f, Description.NoiseStdDev);
      point += Noise;
      Detection.SetPoint(point);
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

	for (auto idxChannel = 0u; idxChannel < Description.Channels; ++idxChannel)
	{
		auto &channelHits = RecordedHits[idxChannel];
		auto &channelSamples = RecordedHitSampleIndices[idxChannel];
		for (auto idxHit = 0u; idxHit < channelHits.size(); ++idxHit)
		{
			FDetection Detection = ComputeDetection(channelHits[idxHit], SensorTransform);
			Detection.ring = static_cast<uint16_t>(idxChannel);
			const uint32_t sampleIndex = idxHit < channelSamples.size() ? channelSamples[idxHit] : 0u;
			Detection.time = SecondsPerSample > 0.0f ? static_cast<float>(sampleIndex) * SecondsPerSample : 0.0f;
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
  PointCloudLidarData.Emplace(Detection.x);
  PointCloudLidarData.Emplace(Detection.y);
  PointCloudLidarData.Emplace(Detection.z);
  PointCloudLidarData.Emplace(Detection.intensity);
}

