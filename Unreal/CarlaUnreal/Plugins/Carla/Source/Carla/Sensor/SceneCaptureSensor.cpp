// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include "Carla/Sensor/SceneCaptureSensor.h"
#include "Carla/Sensor/PostProcessConfig.h"
#include "Carla.h"
#include "Carla/Game/CarlaStatics.h"

// Geometry Drawer subsystem for debug primitives (points/lines/cubes)
#include "SvcGeometryDrawerSubsystem.h"
#include "SvcGeometryDrawerManager.h"

#include <util/ue-header-guard-begin.h>
#include "Actor/ActorBlueprintFunctionLibrary.h"
#include "Engine/PostProcessVolume.h"
#include "GameFramework/SpectatorPawn.h"
#include <util/ue-header-guard-end.h>

#include <mutex>
#include <atomic>
#include <thread>

static int SCENE_CAPTURE_COUNTER = 0u;

// =============================================================================
// -- Local static methods -----------------------------------------------------
// =============================================================================

// Local namespace to avoid name collisions on unit builds.
namespace SceneCaptureSensor_local_ns {

  static void SetCameraDefaultOverrides(USceneCaptureComponent2D &CaptureComponent2D);

  static auto GetQualitySettings(UWorld *World)
  {
    auto Settings = UCarlaStatics::GetCarlaSettings(World);
    check(Settings != nullptr);
    return Settings->GetQualityLevel();
  }

} // namespace SceneCaptureSensor_local_ns

// =============================================================================
// -- ASceneCaptureSensor ------------------------------------------------------
// =============================================================================

ASceneCaptureSensor::ASceneCaptureSensor(const FObjectInitializer &ObjectInitializer)
  : Super(ObjectInitializer)
{
  PrimaryActorTick.bCanEverTick = true;
  PrimaryActorTick.TickGroup = TG_PrePhysics;

  CaptureRenderTarget = CreateDefaultSubobject<UTextureRenderTarget2D>(
      FName(*FString::Printf(TEXT("CaptureRenderTarget_d%d"), SCENE_CAPTURE_COUNTER)));
  CaptureRenderTarget->CompressionSettings = TextureCompressionSettings::TC_Default;
  CaptureRenderTarget->SRGB = false;
  CaptureRenderTarget->bAutoGenerateMips = false;
  CaptureRenderTarget->bGPUSharedFlag = true;
  CaptureRenderTarget->AddressX = TextureAddress::TA_Clamp;
  CaptureRenderTarget->AddressY = TextureAddress::TA_Clamp;

  CaptureComponent2D = CreateDefaultSubobject<USceneCaptureComponent2D_CARLA>(
      FName(*FString::Printf(TEXT("USceneCaptureComponent2D%d"), SCENE_CAPTURE_COUNTER)));
  check(CaptureComponent2D != nullptr);
  CaptureComponent2D->ViewActor = this;
  CaptureComponent2D->SetupAttachment(RootComponent);
  CaptureComponent2D->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
  CaptureComponent2D->bCaptureOnMovement = false;
  CaptureComponent2D->bCaptureEveryFrame = false;
  CaptureComponent2D->bAlwaysPersistRenderingState = true;
  CaptureComponent2D->bUseRayTracingIfEnabled = true;

  SceneCaptureSensor_local_ns::SetCameraDefaultOverrides(*CaptureComponent2D);

  ++SCENE_CAPTURE_COUNTER;
}

void ASceneCaptureSensor::Set(const FActorDescription &Description)
{
  Super::Set(Description);
  UActorBlueprintFunctionLibrary::SetCamera(Description, this);
}

void ASceneCaptureSensor::SetImageSize(uint32 InWidth, uint32 InHeight)
{
  ImageWidth = InWidth;
  ImageHeight = InHeight;
}

void ASceneCaptureSensor::SetFOVAngle(const float FOVAngle)
{
  check(CaptureComponent2D != nullptr);
  CaptureComponent2D->FOVAngle = FOVAngle;
}

float ASceneCaptureSensor::GetFOVAngle() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->FOVAngle;
}

namespace
{
  void LogPostProcessMutationBlocked(const TCHAR *FunctionName)
  {
    UE_LOG(LogCarla, Verbose, TEXT("%s: 后处理参数调整已被禁用"), FunctionName);
  }
}

void ASceneCaptureSensor::SetExposureMethod(EAutoExposureMethod Method)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetExposureMethod"));
}

EAutoExposureMethod ASceneCaptureSensor::GetExposureMethod() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.AutoExposureMethod;
}

void ASceneCaptureSensor::SetExposureCompensation(float Compensation)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetExposureCompensation"));
}

float ASceneCaptureSensor::GetExposureCompensation() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.AutoExposureBias;
}

void ASceneCaptureSensor::SetShutterSpeed(float Speed)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetShutterSpeed"));
}

float ASceneCaptureSensor::GetShutterSpeed() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.CameraShutterSpeed;
}

void ASceneCaptureSensor::SetISO(float ISO)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetISO"));
}

float ASceneCaptureSensor::GetISO() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.CameraISO;
}

void ASceneCaptureSensor::SetAperture(float Aperture)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetAperture"));
}

float ASceneCaptureSensor::GetAperture() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.DepthOfFieldFstop;
}

void ASceneCaptureSensor::SetSensorWidth(float Width)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetSensorWidth"));
}

float ASceneCaptureSensor::GetSensorWidth() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.DepthOfFieldSensorWidth;
}

void ASceneCaptureSensor::SetFocalDistance(float Distance)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetFocalDistance"));
}

float ASceneCaptureSensor::GetFocalDistance() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.DepthOfFieldFocalDistance;
}

void ASceneCaptureSensor::SetDepthBlurAmount(float Amount)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetDepthBlurAmount"));
}

float ASceneCaptureSensor::GetDepthBlurAmount() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.DepthOfFieldDepthBlurAmount;
}

void ASceneCaptureSensor::SetDepthBlurRadius(float Radius)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetDepthBlurRadius"));
}

float ASceneCaptureSensor::GetDepthBlurRadius() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.DepthOfFieldDepthBlurRadius;
}

void ASceneCaptureSensor::SetDepthOfFieldMinFstop(float MinFstop)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetDepthOfFieldMinFstop"));
}

float ASceneCaptureSensor::GetDepthOfFieldMinFstop() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.DepthOfFieldMinFstop;
}

void ASceneCaptureSensor::SetBladeCount(int Count)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetBladeCount"));
}

int ASceneCaptureSensor::GetBladeCount() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.DepthOfFieldBladeCount;
}

void ASceneCaptureSensor::SetFilmSlope(float Slope)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetFilmSlope"));
}

float ASceneCaptureSensor::GetFilmSlope() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.FilmSlope;
}

void ASceneCaptureSensor::SetFilmToe(float Toe)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetFilmToe"));
}

float ASceneCaptureSensor::GetFilmToe() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.FilmToe;
}

void ASceneCaptureSensor::SetFilmShoulder(float Shoulder)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetFilmShoulder"));
}

float ASceneCaptureSensor::GetFilmShoulder() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.FilmShoulder;
}

void ASceneCaptureSensor::SetFilmBlackClip(float BlackClip)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetFilmBlackClip"));
}

float ASceneCaptureSensor::GetFilmBlackClip() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.FilmBlackClip;
}

void ASceneCaptureSensor::SetFilmWhiteClip(float WhiteClip)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetFilmWhiteClip"));
}

float ASceneCaptureSensor::GetFilmWhiteClip() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.FilmWhiteClip;
}

void ASceneCaptureSensor::SetExposureMinBrightness(float Brightness)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetExposureMinBrightness"));
}

float ASceneCaptureSensor::GetExposureMinBrightness() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.AutoExposureMinBrightness;
}

void ASceneCaptureSensor::SetExposureMaxBrightness(float Brightness)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetExposureMaxBrightness"));
}

float ASceneCaptureSensor::GetExposureMaxBrightness() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.AutoExposureMaxBrightness;
}

void ASceneCaptureSensor::SetExposureSpeedDown(float Speed)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetExposureSpeedDown"));
}

float ASceneCaptureSensor::GetExposureSpeedDown() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.AutoExposureSpeedDown;
}

void ASceneCaptureSensor::SetExposureSpeedUp(float Speed)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetExposureSpeedUp"));
}

float ASceneCaptureSensor::GetExposureSpeedUp() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.AutoExposureSpeedUp;
}

void ASceneCaptureSensor::SetExposureCalibrationConstant(float Constant)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetExposureCalibrationConstant"));
}

float ASceneCaptureSensor::GetExposureCalibrationConstant() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.AutoExposureCalibrationConstant_DEPRECATED;
}

void ASceneCaptureSensor::SetMotionBlurIntensity(float Intensity)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetMotionBlurIntensity"));
}

float ASceneCaptureSensor::GetMotionBlurIntensity() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.MotionBlurAmount;
}

void ASceneCaptureSensor::SetMotionBlurMaxDistortion(float MaxDistortion)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetMotionBlurMaxDistortion"));
}

float ASceneCaptureSensor::GetMotionBlurMaxDistortion() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.MotionBlurMax;
}

void ASceneCaptureSensor::SetMotionBlurMinObjectScreenSize(float ScreenSize)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetMotionBlurMinObjectScreenSize"));
}

float ASceneCaptureSensor::GetMotionBlurMinObjectScreenSize() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.MotionBlurPerObjectSize;
}

void ASceneCaptureSensor::SetLensFlareIntensity(float Intensity)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetLensFlareIntensity"));
}

float ASceneCaptureSensor::GetLensFlareIntensity() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.LensFlareIntensity;
}

void ASceneCaptureSensor::SetBloomIntensity(float Intensity)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetBloomIntensity"));
}

float ASceneCaptureSensor::GetBloomIntensity() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.BloomIntensity;
}

void ASceneCaptureSensor::SetWhiteTemp(float Temp)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetWhiteTemp"));
}

float ASceneCaptureSensor::GetWhiteTemp() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.WhiteTemp;
}

void ASceneCaptureSensor::SetWhiteTint(float Tint)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetWhiteTint"));
}

float ASceneCaptureSensor::GetWhiteTint() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.WhiteTint;
}

void ASceneCaptureSensor::SetChromAberrIntensity(float Intensity)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetChromAberrIntensity"));
}

float ASceneCaptureSensor::GetChromAberrIntensity() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.SceneFringeIntensity;
}

void ASceneCaptureSensor::SetChromAberrOffset(float ChromAberrOffset)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetChromAberrOffset"));
}

float ASceneCaptureSensor::GetChromAberrOffset() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.ChromaticAberrationStartOffset;
}

void ASceneCaptureSensor::SetColorSaturation(FVector4 ColorSaturation)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetColorSaturation"));
}

FVector4 ASceneCaptureSensor::GetColorSaturation() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.ColorSaturation;
}

void ASceneCaptureSensor::SetColorContrast(FVector4 ColorContrast)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetColorContrast"));
}

FVector4 ASceneCaptureSensor::GetColorContrast() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.ColorContrast;
}

void ASceneCaptureSensor::SetColorGamma(FVector4 ColorGamma)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetColorGamma"));
}

FVector4 ASceneCaptureSensor::GetColorGamma() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.ColorGamma;
}

void ASceneCaptureSensor::SetHighlightsGamma(FVector4 HighlightsGamma)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetHighlightsGamma"));
}

FVector4 ASceneCaptureSensor::GetHighlightsGamma() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.ColorGammaHighlights;
}

void ASceneCaptureSensor::SetToneCurveAmount(float ToneCurveAmount)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetToneCurveAmount"));
}

float ASceneCaptureSensor::GetToneCurveAmount() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.ToneCurveAmount;
}

void ASceneCaptureSensor::SetSceneColorTint(FLinearColor SceneColorTint)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetSceneColorTint"));
}

FLinearColor ASceneCaptureSensor::GetSceneColorTint() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.SceneColorTint;
}

void ASceneCaptureSensor::SetVignetteIntensity(float VignetteIntensity)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetVignetteIntensity"));
}

float ASceneCaptureSensor::GetVignetteIntensity() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.VignetteIntensity;
}

void ASceneCaptureSensor::SetHighlightContrastScale(float HighlightContrastScale)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetHighlightContrastScale"));
}

float ASceneCaptureSensor::GetHighlightContrastScale() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.LocalExposureHighlightContrastScale;
}

void ASceneCaptureSensor::SetShadowContrastScale(float ShadowContrastScale)
{
  check(CaptureComponent2D != nullptr);
  LogPostProcessMutationBlocked(TEXT("SetShadowContrastScale"));
}

float ASceneCaptureSensor::GetShadowContrastScale() const
{
  check(CaptureComponent2D != nullptr);
  return CaptureComponent2D->PostProcessSettings.LocalExposureShadowContrastScale;
}

void ASceneCaptureSensor::UpdatePostProcessConfig(
    FPostProcessConfig& InOutPostProcessConfig)
{
}

bool ASceneCaptureSensor::ApplyPostProcessVolumeToSensor(APostProcessVolume* Origin, ASceneCaptureSensor* Dest, bool bOverrideCurrentCamera)
{
  UE_LOG(LogCarla, Warning, TEXT("ApplyPostProcessVolumeToSensor: 后处理拷贝已禁用"));
  return false;
}

void ASceneCaptureSensor::EnqueueRenderSceneImmediate() {
  TRACE_CPUPROFILER_EVENT_SCOPE(ASceneCaptureSensor::EnqueueRenderSceneImmediate);
  // Creates an snapshot of the scene, requieres bCaptureEveryFrame = false.
#ifdef CARLA_HAS_GBUFFER_API
  CaptureSceneExtended();
#else
  GetCaptureComponent2D()->CaptureScene();
#endif
}

void ASceneCaptureSensor::BeginPlay()
{
  using namespace SceneCaptureSensor_local_ns;

  bEnablePostProcessingEffects = true;
  // Determine the gamma of the player.
  const bool bInForceLinearGamma = false;

  CaptureRenderTarget->InitCustomFormat(
      ImageWidth,
      ImageHeight,
      bEnable16BitFormat ? PF_FloatRGBA : PF_B8G8R8A8,
      bInForceLinearGamma);

  CaptureRenderTarget->TargetGamma = TargetGamma;
  UE_LOG(LogTemp, Log, TEXT("ASceneCaptureSensor::BeginPlay()::CaptureRenderTarget->TargetGamma: %f"), CaptureRenderTarget->TargetGamma);

  check(IsValid(CaptureComponent2D) && IsValidChecked(CaptureComponent2D));

  CaptureComponent2D->Deactivate();
  CaptureComponent2D->TextureTarget = CaptureRenderTarget;

  // Call derived classes to set up their things.
  SetUpSceneCaptureComponent(*CaptureComponent2D);

  CaptureComponent2D->CaptureSource = ESceneCaptureSource::SCS_FinalToneCurveHDR;

  CaptureComponent2D->UpdateContent();
  CaptureComponent2D->Activate();

  // 忽略 GeometryDrawer 子系统的 HookActor，从 RGB 等 SceneCapture 中排除调试几何
  {
    UCarlaGameInstance* GameInstance = UCarlaStatics::GetGameInstance(GetWorld());
    if (GameInstance)
    {
      if (USvcGeometryDrawerSubsystem* DrawerSubsystem = GameInstance->GetSubsystem<USvcGeometryDrawerSubsystem>())
      {
        if (USvcGeometryDrawerManager* DrawerManager = DrawerSubsystem->GetManager())
        {
          if (AActor* HookActor = DrawerManager->GetHookActor())
          {
            CaptureComponent2D->HiddenActors.AddUnique(HookActor);
          }
        }
      }
    }
  }

  // Make sure that there is enough time in the render queue.
  UKismetSystemLibrary::ExecuteConsoleCommand(
      GetWorld(),
      FString("g.TimeoutForBlockOnRenderFence 300000"));

  auto PostProcessConfig = FPostProcessConfig(
      CaptureComponent2D->PostProcessSettings,
      CaptureComponent2D->ShowFlags);
  PostProcessConfig.UpdateFromSceneCaptureComponent2D(*CaptureComponent2D);
  PostProcessConfig.EnablePostProcessingEffects(ArePostProcessingEffectsEnabled());
  UpdatePostProcessConfig(PostProcessConfig);
  CaptureComponent2D->ShowFlags = PostProcessConfig.EngineShowFlags;
  CaptureComponent2D->PostProcessSettings = PostProcessConfig.PostProcessSettings;

  if (ImageWidth < 1920 || ImageHeight < 1080)
    CaptureComponent2D->ShowFlags.SetMotionBlur(false);
  
  // This ensures the camera is always spawning the raindrops in case the
  // weather was previously set to have rain.
  auto Weather = GetEpisode().GetWeather();
  if (Weather != nullptr)
    Weather->NotifyWeather(this);
  
  Super::BeginPlay();
}

void ASceneCaptureSensor::PrePhysTick(float DeltaSeconds)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(ASceneCaptureSensor::PrePhysTick);
  Super::PrePhysTick(DeltaSeconds);

  // Add the view information every tick. It's only used for one tick and then
  // removed by the streamer.
  IStreamingManager::Get().AddViewInformation(
      CaptureComponent2D->GetComponentLocation(),
      ImageWidth,
      ImageWidth / FMath::Tan(CaptureComponent2D->FOVAngle));
}

void ASceneCaptureSensor::PostPhysTick(UWorld *World, ELevelTick TickType, float DeltaTime)
{
  TRACE_CPUPROFILER_EVENT_SCOPE(ASceneCaptureSensor::PostPhysTick);
  Super::PostPhysTick(World, TickType, DeltaTime);
  EnqueueRenderSceneImmediate();
}

void ASceneCaptureSensor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  Super::EndPlay(EndPlayReason);
  FlushRenderingCommands();
  SCENE_CAPTURE_COUNTER = 0u;
}

#ifdef CARLA_HAS_GBUFFER_API

constexpr const TCHAR* GBufferNames[] =
{
  TEXT("SceneColor"),
  TEXT("SceneDepth"),
  TEXT("SceneStencil"),
  TEXT("GBufferA"),
  TEXT("GBufferB"),
  TEXT("GBufferC"),
  TEXT("GBufferD"),
  TEXT("GBufferE"),
  TEXT("GBufferF"),
  TEXT("Velocity"),
  TEXT("SSAO"),
  TEXT("CustomDepth"),
  TEXT("CustomStencil"),
};

template <EGBufferTextureID ID, typename T>
static void CheckGBufferStream(T& GBufferStream, FGBufferRequest& GBuffer)
{
    GBufferStream.bIsUsed = GBufferStream.Stream.AreClientsListening();
    if (GBufferStream.bIsUsed)
        GBuffer.MarkAsRequested(ID);
}

static uint64 Prior = 0;

void ASceneCaptureSensor::CaptureSceneExtended()
{
    auto GBufferPtr = MakeUnique<FGBufferRequest>();
    auto& GBuffer = *GBufferPtr;

    CheckGBufferStream<EGBufferTextureID::SceneColor>(CameraGBuffers.SceneColor, GBuffer);
    CheckGBufferStream<EGBufferTextureID::SceneDepth>(CameraGBuffers.SceneDepth, GBuffer);
    CheckGBufferStream<EGBufferTextureID::SceneStencil>(CameraGBuffers.SceneStencil, GBuffer);
    CheckGBufferStream<EGBufferTextureID::GBufferA>(CameraGBuffers.GBufferA, GBuffer);
    CheckGBufferStream<EGBufferTextureID::GBufferB>(CameraGBuffers.GBufferB, GBuffer);
    CheckGBufferStream<EGBufferTextureID::GBufferC>(CameraGBuffers.GBufferC, GBuffer);
    CheckGBufferStream<EGBufferTextureID::GBufferD>(CameraGBuffers.GBufferD, GBuffer);
    CheckGBufferStream<EGBufferTextureID::GBufferE>(CameraGBuffers.GBufferE, GBuffer);
    CheckGBufferStream<EGBufferTextureID::GBufferF>(CameraGBuffers.GBufferF, GBuffer);
    CheckGBufferStream<EGBufferTextureID::Velocity>(CameraGBuffers.Velocity, GBuffer);
    CheckGBufferStream<EGBufferTextureID::SSAO>(CameraGBuffers.SSAO, GBuffer);
    CheckGBufferStream<EGBufferTextureID::CustomDepth>(CameraGBuffers.CustomDepth, GBuffer);
    CheckGBufferStream<EGBufferTextureID::CustomStencil>(CameraGBuffers.CustomStencil, GBuffer);

    if (GBufferPtr->DesiredTexturesMask == 0)
    {
        // Creates an snapshot of the scene, requieres bCaptureEveryFrame = false.
        CaptureComponent2D->CaptureScene();
        return;
    }

    if (Prior != GBufferPtr->DesiredTexturesMask)
        UE_LOG(LogCarla, Verbose, TEXT("GBuffer selection changed (%llu)."), GBufferPtr->DesiredTexturesMask);

    Prior = GBufferPtr->DesiredTexturesMask;
    GBufferPtr->OwningActor = CaptureComponent2D->GetViewOwner();

#define CARLA_GBUFFER_DISABLE_TAA // Temporarily disable TAA to avoid jitter.

#ifdef CARLA_GBUFFER_DISABLE_TAA
    bool bTAA = CaptureComponent2D->ShowFlags.TemporalAA;
    if (bTAA) {
        CaptureComponent2D->ShowFlags.TemporalAA = false;
    }
#endif

    CaptureComponent2D->CaptureSceneWithGBuffer(GBuffer);

#ifdef CARLA_GBUFFER_DISABLE_TAA
    if (bTAA) {
        CaptureComponent2D->ShowFlags.TemporalAA = true;
    }
#undef CARLA_GBUFFER_DISABLE_TAA
#endif

    AsyncTask(ENamedThreads::AnyHiPriThreadNormalTask, [this, GBuffer = MoveTemp(GBufferPtr)]() mutable
        {
            SendGBufferTextures(*GBuffer);
        });
}

void ASceneCaptureSensor::SendGBufferTextures(FGBufferRequest& GBuffer)
{
    SendGBufferTexturesInternal(*this, GBuffer);
}

#endif



// =============================================================================
// -- Local static functions implementations -----------------------------------
// =============================================================================

namespace SceneCaptureSensor_local_ns {

  static void SetCameraDefaultOverrides(USceneCaptureComponent2D &CaptureComponent2D)
  {
    auto &PostProcessSettings = CaptureComponent2D.PostProcessSettings;

    PostProcessSettings.bOverride_AutoExposureMethod = true;
    PostProcessSettings.bOverride_AutoExposureBias = true;
    PostProcessSettings.bOverride_CameraShutterSpeed = true;
    PostProcessSettings.bOverride_CameraISO = true;
    PostProcessSettings.bOverride_DepthOfFieldFstop = true;
    PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
    PostProcessSettings.bOverride_DepthOfFieldDepthBlurAmount = true;
    PostProcessSettings.bOverride_DepthOfFieldDepthBlurRadius = true;
    PostProcessSettings.bOverride_DepthOfFieldMinFstop = true;
    PostProcessSettings.bOverride_DepthOfFieldBladeCount = true;
    PostProcessSettings.bOverride_DepthOfFieldSensorWidth = true;
    PostProcessSettings.bOverride_FilmSlope = true;
    PostProcessSettings.bOverride_FilmToe = true;
    PostProcessSettings.bOverride_FilmShoulder = true;
    PostProcessSettings.bOverride_FilmBlackClip = true;
    PostProcessSettings.bOverride_FilmWhiteClip = true;
    PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
    PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
    PostProcessSettings.bOverride_AutoExposureSpeedDown = true;
    PostProcessSettings.bOverride_AutoExposureSpeedUp = true;
    PostProcessSettings.bOverride_AutoExposureCalibrationConstant_DEPRECATED = true;
    PostProcessSettings.bOverride_TemperatureType = true;
    PostProcessSettings.bOverride_MotionBlurAmount = true;
    PostProcessSettings.bOverride_MotionBlurMax = true;
    PostProcessSettings.bOverride_MotionBlurPerObjectSize = true;
    PostProcessSettings.bOverride_LensFlareIntensity = true;
    PostProcessSettings.bOverride_BloomIntensity = true;
    PostProcessSettings.bOverride_WhiteTemp = true;
    PostProcessSettings.bOverride_WhiteTint = true;
    PostProcessSettings.bOverride_SceneFringeIntensity = true;
    PostProcessSettings.bOverride_ChromaticAberrationStartOffset = true;
    PostProcessSettings.bOverride_ColorSaturation = true;
    PostProcessSettings.bOverride_ColorContrast = true;
    PostProcessSettings.bOverride_ColorGamma = true;
    PostProcessSettings.bOverride_ColorGammaHighlights = true;
    PostProcessSettings.bOverride_ToneCurveAmount = true;
    PostProcessSettings.bOverride_SceneColorTint = true;
    PostProcessSettings.bOverride_VignetteIntensity = true;
    PostProcessSettings.bOverride_LocalExposureHighlightContrastScale = true;
    PostProcessSettings.bOverride_LocalExposureShadowContrastScale = true;

    CaptureComponent2D.bUseRayTracingIfEnabled = true;
    PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
    PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
    PostProcessSettings.bOverride_LumenSceneLightingQuality = true;
    PostProcessSettings.LumenSceneLightingQuality = 1.0f;
    PostProcessSettings.bOverride_LumenSceneDetail = true;
    PostProcessSettings.LumenSceneDetail = 1.0f;
    PostProcessSettings.bOverride_LumenSceneViewDistance = true;
    PostProcessSettings.LumenSceneViewDistance = 20000.0f;
    PostProcessSettings.bOverride_LumenFinalGatherQuality = true;
    PostProcessSettings.LumenFinalGatherQuality = 1.0f;
    PostProcessSettings.bOverride_LumenMaxTraceDistance = true;
    PostProcessSettings.LumenMaxTraceDistance = 20000.0f;
    PostProcessSettings.bOverride_LumenSurfaceCacheResolution = true;
    PostProcessSettings.LumenSurfaceCacheResolution = 1.0f;
    PostProcessSettings.bOverride_LumenSceneLightingUpdateSpeed = true;
    PostProcessSettings.LumenSceneLightingUpdateSpeed = 1.0f;
    PostProcessSettings.bOverride_LumenFinalGatherLightingUpdateSpeed = true;
    PostProcessSettings.LumenFinalGatherLightingUpdateSpeed = 1.0f;
    PostProcessSettings.bOverride_LumenDiffuseColorBoost = true;
    PostProcessSettings.LumenDiffuseColorBoost = 1.0f;
    PostProcessSettings.bOverride_LumenSkylightLeaking = true;
    PostProcessSettings.LumenSkylightLeaking = 0.1f;
    PostProcessSettings.bOverride_LumenFullSkylightLeakingDistance = true;
    PostProcessSettings.LumenFullSkylightLeakingDistance = 1000.0f;
    PostProcessSettings.bOverride_ReflectionMethod = true;
    PostProcessSettings.ReflectionMethod = EReflectionMethod::Lumen;
    PostProcessSettings.bOverride_LumenReflectionQuality = true;
    PostProcessSettings.LumenReflectionQuality = 1.0f;
    PostProcessSettings.bOverride_LumenRayLightingMode = true;
    PostProcessSettings.LumenRayLightingMode = ELumenRayLightingModeOverride::Default;
    PostProcessSettings.bOverride_LumenFrontLayerTranslucencyReflections = true;
    PostProcessSettings.LumenFrontLayerTranslucencyReflections = true;
    PostProcessSettings.bOverride_LumenMaxReflectionBounces = true;
    PostProcessSettings.LumenMaxReflectionBounces = 3;
  }
} // namespace SceneCaptureSensor_local_ns
