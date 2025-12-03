// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "Carla/Game/CarlaEpisode.h"
#include "Carla/Sensor/DataStream.h"
#include "Carla/Util/RandomEngine.h"
#include "Carla/Game/CarlaEngine.h"

#include <util/disable-ue4-macros.h>
#include <carla/Logging.h>
#include <carla/Buffer.h>
#include <carla/BufferView.h>
#include <carla/sensor/SensorRegistry.h>
#include <util/enable-ue4-macros.h>

#include <util/ue-header-guard-begin.h>
#include "GameFramework/Actor.h"
#include <util/ue-header-guard-end.h>

#include "Sensor.generated.h"

struct FActorDescription;



/*  @CARLA_UE5
    
    The FPixelReader class has been deprecated, as its functionality
    is now split between ImageUtil::ReadImageDataAsync (see Sensor/ImageUtil.h)
    and ASensor::SendDataToClient.
    Here's a brief example of how to use both:
    
    if (!AreClientsListening()) // Ideally, check whether there are any clients.
        return;

    auto FrameIndex = FCarlaEngine::GetFrameCounter();
    ImageUtil::ReadImageDataAsync(
        *GetCaptureRenderTarget(),
        [this](
            const void* MappedPtr,
            size_t RowPitch,
            size_t BufferHeight,
            EPixelFormat Format,
            FIntPoint Extent)
        {
            TArray<FColor> ImageData;
            // Parse the raw data into ImageData...
            SendDataToClient(
                *this,
                ImageData,
                FrameIndex);
            return true;
        });

    Alternatively, if you just want to retrieve the pixels as
    FColor/FLinearColor, you can just use ReadImageDataAsyncFColor
    or ReadImageDataAsyncFLinearColor.

*/



/// Base class for sensors.
UCLASS(Abstract, hidecategories = (Collision, Attachment, Actor))
class CARLA_API ASensor : public AActor
{
  GENERATED_BODY()

public:

  ASensor(const FObjectInitializer &ObjectInitializer);

  void SetEpisode(const UCarlaEpisode &InEpisode)
  {
    Episode = &InEpisode;
  }

  virtual void Set(const FActorDescription &Description);

  std::optional<FActorAttribute> GetAttribute(const FString Name);

  virtual void BeginPlay();

  /// Replace the FDataStream associated with this sensor.
  ///
  /// @warning Do not change the stream after BeginPlay. It is not thread-safe.
  void SetDataStream(FDataStream InStream)
  {
    Stream = std::move(InStream);
  }

  FDataStream MoveDataStream()
  {
    return std::move(Stream);
  }

  /// Return the token that allows subscribing to this sensor's stream.
  auto GetToken() const
  {
    return Stream.GetToken();
  }

  bool IsStreamReady()
  {
    return Stream.IsStreamReady();
  }

  bool AreClientsListening()
  { 
    return Stream.AreClientsListening();
  }

  void Tick(const float DeltaTime) final;

  virtual void PrePhysTick(float DeltaSeconds) {}
  virtual void PostPhysTick(UWorld *World, ELevelTick TickType, float DeltaSeconds) {}
  // Small interface to notify sensors when clients are listening
  virtual void OnFirstClientConnected() {};
  // Small interface to notify sensors when no clients are listening
  virtual void OnLastClientDisconnected() {};


  void PostPhysTickInternal(UWorld *World, ELevelTick TickType, float DeltaSeconds);

  UFUNCTION(BlueprintCallable)
  URandomEngine *GetRandomEngine()
  {
    return RandomEngine;
  }

  UFUNCTION(BlueprintCallable)
  int32 GetSeed() const
  {
    return Seed;
  }

  UFUNCTION(BlueprintCallable)
  void SetSeed(int32 InSeed);

  const UCarlaEpisode &GetEpisode() const
  {
    check(Episode != nullptr);
    return *Episode;
  }

  void SetSavingDataToDisk(bool bSavingData) { bSavingDataToDisk = bSavingData; }

protected:

  void PostActorCreated() override;

  void EndPlay(EEndPlayReason::Type EndPlayReason) override;

  /// Return the FDataStream associated with this sensor.
  ///
  /// You need to provide a reference to self, this is necessary for template
  /// deduction.
  template <typename SensorType>
  FAsyncDataStream GetDataStream(SensorType&& Self)
  {
    return Stream.MakeAsyncDataStream<std::remove_cvref_t<SensorType>>(
      std::forward<SensorType>(Self),
      GetEpisode().GetElapsedGameTime());
  }


  // Send sensor data to the client.
  template <
    typename SensorType,
    typename ElementType>
  static void SendDataToClient(
    SensorType&& Sensor,                  // The data's owning sensor.
    TArrayView<ElementType> SensorData,   // Data to send to the client.
    uint64_t FrameIndex,                   // Current frame index.
    bool bSendToROS2 = true
    )
  {
    using carla::sensor::SensorRegistry;
    using SensorT = std::remove_const_t<std::remove_reference_t<SensorType>>;
    constexpr size_t HeaderOffset = SensorRegistry::get<SensorT*>::type::header_offset;

    if (!Sensor.AreClientsListening())
        return;

    auto Stream = Sensor.GetDataStream(Sensor);
    Stream.SetFrameNumber(FrameIndex);
    
    auto Buffer = Stream.PopBufferFromPool();
    Buffer.copy_from(
      HeaderOffset,
      boost::asio::buffer(
        SensorData.GetData(),
        SensorData.Num() * sizeof(ElementType)));

    if (!Buffer.data())
      return;

    auto Serialized = SensorRegistry::Serialize(Sensor, std::move(Buffer));
    auto SerializedBuffer = carla::Buffer(std::move(Serialized));
    auto BufferView = carla::BufferView::CreateFrom(std::move(SerializedBuffer));

#if defined(WITH_ROS2)
    auto ROS2 = carla::ros2::ROS2::GetInstance();
    if (ROS2->IsEnabled() && bSendToROS2)
    {
      TRACE_CPUPROFILER_EVENT_SCOPE_STR("ROS2 SendDataToClient");
      auto StreamId = carla::streaming::detail::token_type(Sensor.GetToken()).get_stream_id();
      auto Res = std::async(std::launch::async, [&Sensor, ROS2, &Stream, StreamId, BufferView]()
      {
        // get resolution of camera
        int W = -1, H = -1;
        float Fov = -1.0f;
        auto WidthOpt = Sensor.GetAttribute("image_size_x");
        if (WidthOpt.has_value())
          W = FCString::Atoi(*WidthOpt->Value);
        auto HeightOpt = Sensor.GetAttribute("image_size_y");
        if (HeightOpt.has_value())
          H = FCString::Atoi(*HeightOpt->Value);
        auto FovOpt = Sensor.GetAttribute("fov");
        if (FovOpt.has_value())
          Fov = FCString::Atof(*FovOpt->Value);
        // send data to ROS2
        auto ParentActor = Sensor.GetAttachParentActor();
        auto Transform =
          ParentActor ?
          Sensor.GetActorTransform().GetRelativeTransform(ParentActor->GetActorTransform()) :
          Stream.GetSensorTransform();
        ROS2->ProcessDataFromCamera(
          Stream.GetSensorType(),
          StreamId,
          Transform,
          W, H,
          Fov,
          BufferView,
          &Sensor);
      });
    }
#endif

    if (Sensor.AreClientsListening())
      Stream.Send(Sensor, BufferView);
  }
//
// 	  template <
//     typename SensorType,
//     typename ElementType>
//   static void SendDataToClient(
//     SensorType&& Sensor,                  // 调用这个函数的传感器对象
//     TArrayView<ElementType> SensorData,   // 传感器采样到的原始帧数据（如图像字节数组）
//     uint64_t FrameIndex                   // 这帧数据在世界里的帧编号，用来标记时间线。
//     )
//   {
//     using carla::sensor::SensorRegistry;	// 用于在 Carla 内部查找某传感器类型对应的序列化、偏移、配置等信息
//     using SensorT = std::remove_const_t<std::remove_reference_t<SensorType>>;// 去掉引用和 const 的裸类型，后面模板里做映射会用
//   	// 查出 SensorRegistry 中这个传感器类型在数据包里需要预留的头部偏移量，这是 Carla 序列化设计的一部分（不同传感器有不同包头）。
//     constexpr size_t HeaderOffset = SensorRegistry::get<SensorT*>::type::header_offset;
//
//   	// 如果启用了 ROS2，就算没有客户端，也要强制发布话题。
//     bool bForceSend = false;
// #if defined(WITH_ROS2)
//     auto ROS2 = carla::ros2::ROS2::GetInstance();
//   	ROS2->Enable(true);
//     if (ROS2->IsEnabled())
//       bForceSend = true;
// #endif
//   	
//     // 如果既没有客户端监听，又没有启用 ROS2，直接 return
//     if (!Sensor.AreClientsListening() && !bForceSend)
//       return;
//
//
//     // 不管是给客户端还是给 ROS2，都需要准备数据流
//     auto Stream = Sensor.GetDataStream(Sensor);
//     Stream.SetFrameNumber(FrameIndex);
//
//   	// 把原始的传感器数据（SensorData）从偏移 HeaderOffset 位置开始写进 Buffer，保证前面空出来留给头部信息。
//   	// 用 boost::asio::buffer 是为了和 Boost 网络流统一接口。
//     auto Buffer = Stream.PopBufferFromPool();
//     Buffer.copy_from(
//       HeaderOffset,
//       boost::asio::buffer(
//         SensorData.GetData(),
//         SensorData.Num() * sizeof(ElementType)));
//
//     if (!Buffer.data())
//       return;
// 	// 调用 Carla 的序列化工具，把当前传感器 + 原始 Buffer 变成 Carla 自己的统一格式（带头 + 数据），方便网络传输或话题打包。
//   	// 包到 SerializedBuffer 中生成 BufferView 用于真正的传输
//     auto Serialized = SensorRegistry::Serialize(Sensor, std::move(Buffer));
//     auto SerializedBuffer = carla::Buffer(std::move(Serialized));
//     auto BufferView = carla::BufferView::CreateFrom(std::move(SerializedBuffer));
//
// #if defined(WITH_ROS2)
//     if (ROS2->IsEnabled())
//     {
//       TRACE_CPUPROFILER_EVENT_SCOPE_STR("ROS2 SendDataToClient");
//       // 解析当前传感器流对应的 StreamId，后续 ROS2 需要用这个 ID 区分消息源。
//       auto StreamId = carla::streaming::detail::token_type(Sensor.GetToken()).get_stream_id();
//
//       // 启动一个异步任务，将当前帧打包给 ROS2 发布
//       auto Res = std::async(std::launch::async, [&Sensor, ROS2, &Stream, StreamId, BufferView]()
//       {
//         // 从传感器获取相机参数
//         int W = -1, H = -1;
//         float Fov = -1.0f;
//
//         auto WidthOpt = Sensor.GetAttribute("image_size_x");
//         if (WidthOpt.has_value())
//           W = FCString::Atoi(*WidthOpt->Value);
//
//         auto HeightOpt = Sensor.GetAttribute("image_size_y");
//         if (HeightOpt.has_value())
//           H = FCString::Atoi(*HeightOpt->Value);
//
//         auto FovOpt = Sensor.GetAttribute("fov");
//         if (FovOpt.has_value())
//           Fov = FCString::Atof(*FovOpt->Value);
//
// 		// 计算相机或传感器相对于父物体的局部坐标变换，如果没父物体就用自己和场景的默认姿态。
//         auto ParentActor = Sensor.GetAttachParentActor();
//         auto Transform =
//           ParentActor ?
//           Sensor.GetActorTransform().GetRelativeTransform(ParentActor->GetActorTransform()) :
//           Stream.GetSensorTransform();
//
//         // 调用 ROS2 封装接口发送帧数据
//         ROS2->ProcessDataFromCamera(
//           Stream.GetSensorType(),
//           StreamId,
//           Transform,
//           W, H,
//           Fov,
//           BufferView,
//           &Sensor);
//       });
//     }
// #endif
//   	
//     // 如果没启用 ROS2 且有客户端监听，再发给 Python 客户端
// #if defined(WITH_ROS2)
//     if (!ROS2->IsEnabled() && Sensor.AreClientsListening())
//     {
//       Stream.Send(Sensor, BufferView);
//     }
// #else
//     if (Sensor.AreClientsListening())
//     {
//       Stream.Send(Sensor, BufferView);
//     }
// #endif
//   }

  /// Seed of the pseudo-random engine.
  UPROPERTY(Category = "Random Engine", EditAnywhere)
  int32 Seed = 123456789;

  /// Random Engine used to provide noise for sensor output.
  UPROPERTY()
  URandomEngine *RandomEngine = nullptr;

  UPROPERTY()
  bool bIsActive = false;

  // Property used when testing with SensorSpawnerActor in editor.
  bool bSavingDataToDisk = false;

private:

  FDataStream Stream;

  FDelegateHandle OnPostTickDelegate;

  FActorDescription SensorDescription;

  const UCarlaEpisode *Episode = nullptr;

  /// Allows the sensor to tick with the tick rate from UE4.
  bool ReadyToTick = false;

  bool bClientsListening = false;

};
