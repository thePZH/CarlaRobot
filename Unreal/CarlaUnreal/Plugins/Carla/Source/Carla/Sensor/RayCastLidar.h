// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once


#include "Carla/Actor/ActorDefinition.h"
#include "Carla/Sensor/LidarDescription.h"
#include "Carla/Sensor/Sensor.h"
#include "Carla/Sensor/RayCastSemanticLidar.h"
#include "Carla/Actor/ActorBlueprintFunctionLibrary.h"

#include <util/disable-ue4-macros.h>
#include <carla/sensor/data/LidarData.h>
#include <util/enable-ue4-macros.h>
// #include "LidarPointCloudShared.h"
#include "RayCastLidar.generated.h"

/// A ray-cast based Lidar sensor.
UCLASS()
class CARLA_API ARayCastLidar : public ARayCastSemanticLidar
{
  GENERATED_BODY()

  using FLidarData = carla::sensor::data::LidarData;
  using FDetection = carla::sensor::data::LidarDetection;

public:
  static FActorDefinition GetSensorDefinition();

  ARayCastLidar(const FObjectInitializer &ObjectInitializer);
  virtual void Set(const FActorDescription &Description) override;
  virtual void Set(const FLidarDescription &LidarDescription) override;

  virtual void PostPhysTick(UWorld *World, ELevelTick TickType, float DeltaTime);

  const TArray<float>& GetTestPointCloud() const { return PointCloudLidarData; };
	
	// UPROPERTY(BlueprintReadOnly) // TODO:Delete ,只作为可视化点云数据用
	// TArray<FLidarPointCloudPoint> PointsCloudPos;
	// UPROPERTY(EditAnywhere, BlueprintReadWrite)
	// float MaxDist = 3500.f;
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "360"))
	// float MaxHue = 350.f;
	// UPROPERTY(EditAnywhere, BlueprintReadWrite)
	// float MaxDistReflectionLine = 1.f;
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
	// float Threshold = 0.001;
	// UPROPERTY(EditAnywhere, BlueprintReadWrite)
	// float LogFactor = 0.001f; 
	// UPROPERTY(EditAnywhere, BlueprintReadWrite)
	// float DebugPointSize = 10.0f;
	//
	// UPROPERTY(EditAnywhere, BlueprintReadWrite)
	// bool bDrawDebugLine = false;
	// UPROPERTY(EditAnywhere, BlueprintReadWrite)
	// bool bDrawDebugPlane = false;
	
private:
  /// Compute received intensity of point
  float ComputeIntensity(const FSemanticDetection& RawDetection) const;
  FDetection ComputeDetection(const FHitResult& HitInfo, const FTransform& SensorTransf) const;

  void PreprocessRays(uint32_t Channels, uint32_t MaxPointsPerChannel) override;
  bool PostprocessDetection(FDetection& Detection) const;

  void ComputeAndSaveDetections(const FTransform& SensorTransform) override;

  // 处理和发布累积的数据
  void ProcessAndPublishAccumulatedData();

  // 累积相关成员变量
  float AccumulatedAngleDistance {0.0f};
  float PreviousHorizontalAngle {0.0f};
  bool HasCompletedFullScan {false};
  std::vector<std::vector<FHitResult>> AccumulatedHits;
  std::vector<std::vector<uint32_t>> AccumulatedHitSampleIndices;

  FLidarData LidarData;

  /// Enable/Disable general dropoff of lidar points
  bool DropOffGenActive;

  /// Slope for the intensity dropoff of lidar points, it is calculated
  /// throught the dropoff limit and the dropoff at zero intensity
  /// The points is kept with a probality alpha*Intensity + beta where
  /// alpha = (1 - dropoff_zero_intensity) / droppoff_limit
  /// beta = (1 - dropoff_zero_intensity)
  float DropOffAlpha;
  float DropOffBeta;

  // Way to access PointCloud data from the server.
  TArray<float> PointCloudLidarData;

  void PointCloudResetMemory();
  void PointCloudWritePointSync(const FDetection& Detection);
};
