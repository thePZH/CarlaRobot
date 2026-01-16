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
  
  // 1. 执行仿真，获取当前帧的数据
  SimulateLidar(DeltaTime);

  // 2. 获取当前角度（从基类的 SemanticLidarData）
  const float CurrentHorizontalAngle = carla::geom::Math::ToDegrees(SemanticLidarData.GetHorizontalAngle());
  
  // 3. 计算角度增量
  float AngleDelta = 0.0f;
  if (HasCompletedFullScan)
  {
    AngleDelta = CurrentHorizontalAngle - PreviousHorizontalAngle;
    // 处理回绕
    if (AngleDelta < 0.0f)
    {
      AngleDelta += Description.HorizontalFov;
    }
  }
  else
  {
    // 第一次扫描
    AngleDelta = CurrentHorizontalAngle;
    // 初始化累积缓冲区
    if (AccumulatedHits.empty())
    {
      AccumulatedHits.resize(Description.Channels);
      AccumulatedHitSampleIndices.resize(Description.Channels);
      for (uint32_t i = 0; i < Description.Channels; ++i)
      {
        AccumulatedHits[i].reserve(2000);
        AccumulatedHitSampleIndices[i].reserve(2000);
      }
    }
  }
  
  // 4. 累积当前帧的数据
  for (uint32_t channel = 0; channel < Description.Channels; ++channel)
  {
    AccumulatedHits[channel].insert(AccumulatedHits[channel].end(), 
                                   RecordedHits[channel].begin(), 
                                   RecordedHits[channel].end());
    AccumulatedHitSampleIndices[channel].insert(AccumulatedHitSampleIndices[channel].end(), 
                                               RecordedHitSampleIndices[channel].begin(), 
                                               RecordedHitSampleIndices[channel].end());
  }
  
  // 5. 累积角度距离
  AccumulatedAngleDistance += AngleDelta;
  
  // 6. 检查是否完成了一个完整的HorizontalFov扫描
  if (AccumulatedAngleDistance >= Description.HorizontalFov)
  {
    // 7. 处理累积的数据
    ProcessAndPublishAccumulatedData();
    
    // 8. 重置累积距离，为下一轮完整扫描做准备
    AccumulatedAngleDistance = 0.0f;
    
    // 9. 清空累积缓冲区
    for (uint32_t channel = 0; channel < Description.Channels; ++channel)
    {
      AccumulatedHits[channel].clear();
      AccumulatedHitSampleIndices[channel].clear();
    }
  }
  
  // 10. 更新状态
  PreviousHorizontalAngle = CurrentHorizontalAngle;
  HasCompletedFullScan = true;
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

void ARayCastLidar::ProcessAndPublishAccumulatedData()
{
  // 1. 计算每通道的点数
  std::vector<uint32_t> AccumulatedPointsPerChannel;
  AccumulatedPointsPerChannel.reserve(Description.Channels);
  for (uint32_t channel = 0; channel < Description.Channels; ++channel)
  {
    AccumulatedPointsPerChannel.push_back(AccumulatedHits[channel].size());
  }

  // 2. 重置LidarData
  LidarData.ResetMemory(AccumulatedPointsPerChannel);

  // 3. 处理累积的检测数据
  FTransform ActorTransf = GetTransform();
  for (uint32_t channel = 0; channel < Description.Channels; ++channel)
  {
    for (size_t i = 0; i < AccumulatedHits[channel].size(); ++i)
    {
      // 从FHitResult计算FDetection
      FDetection Detection = ComputeDetection(AccumulatedHits[channel][i], ActorTransf);
      Detection.ring = static_cast<uint16_t>(channel);
      
      // 设置时间信息
      const uint32_t sampleIndex = i < AccumulatedHitSampleIndices[channel].size() ? 
                                  AccumulatedHitSampleIndices[channel][i] : 0u;
      Detection.time = SecondsPerSample > 0.0f ? 
                     static_cast<float>(sampleIndex) * SecondsPerSample : 0.0f;
      
      // 后处理
      if (PostprocessDetection(Detection))
      {
        LidarData.WritePointSync(Detection);
      }
      else
      {
        AccumulatedPointsPerChannel[channel]--;
      }
    }
  }

  // 4. 写入通道计数
  LidarData.WriteChannelCount(AccumulatedPointsPerChannel);

  // 5. 设置水平角度
  LidarData.SetHorizontalAngle(SemanticLidarData.GetHorizontalAngle());

  // 6. 发送数据流
  auto DataStream = GetDataStream(*this);
  auto SensorTransform = DataStream.GetSensorTransform();
  {
    TRACE_CPUPROFILER_EVENT_SCOPE_STR("Send Stream");
    DataStream.SerializeAndSend(*this, LidarData, DataStream.PopBufferFromPool());
  }

  // 7. ROS2发送
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
