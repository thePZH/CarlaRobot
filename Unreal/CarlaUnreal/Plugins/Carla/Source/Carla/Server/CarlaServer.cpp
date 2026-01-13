// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include "Carla/Server/CarlaServer.h"
#include "Carla.h"
#include "Carla/PzhTest/WheeledRobotAnimationInstance.h"
#include "Robot/RobotBoneControlIn.h"
#include "Carla/Server/CarlaServerResponse.h"
#include "Carla/Traffic/TrafficLightGroup.h"
#include "Carla/OpenDrive/OpenDrive.h"
#include "Carla/Util/DebugShapeDrawer.h"
#include "Carla/Util/NavigationMesh.h"
#include "Carla/Util/RayTracer.h"
#include "Carla/Vehicle/CarlaWheeledVehicle.h"
#include "Carla/Walker/WalkerController.h"
#include "Carla/Walker/WalkerBase.h"
#include "Carla/Game/Tagger.h"
#include "Carla/Game/CarlaStatics.h"
#include "Carla/Gauges/GaugesManagerActor.h"
#include "Carla/Vehicle/MovementComponents/CarSimManagerComponent.h"
#include "Carla/Vehicle/MovementComponents/ChronoMovementComponent.h"
#include "Carla/Lights/CarlaLightSubsystem.h"
#include "Carla/Actor/ActorData.h"
#include "CarlaServerResponse.h"
#include "Carla/Util/BoundingBoxCalculator.h"
#include "Carla/Services/SevnceRobotLogic.h"
#include "Carla/Services/SevnceActorLogic.h"

#include <util/disable-ue4-macros.h>
#include <carla/Functional.h>
#include <carla/multigpu/router.h>
#include <carla/Version.h>
#include <carla/rpc/AckermannControllerSettings.h>
#include <carla/rpc/Actor.h>
#include <carla/rpc/ActorDefinition.h>
#include <carla/rpc/ActorDescription.h>
#include <carla/rpc/BoneTransformDataIn.h>
#include <carla/rpc/Command.h>
#include <carla/rpc/CommandResponse.h>
#include <carla/rpc/DebugShape.h>
#include <carla/rpc/EnvironmentObject.h>
#include <carla/rpc/EpisodeInfo.h>
#include <carla/rpc/EpisodeSettings.h>
#include <carla/rpc/LabelledPoint.h>
#include <carla/rpc/LightState.h>
#include <carla/rpc/MapInfo.h>
#include <carla/rpc/MapLayer.h>
#include <carla/rpc/Response.h>
#include <carla/rpc/Server.h>
#include <carla/rpc/String.h>
#include <carla/rpc/Transform.h>
#include <carla/rpc/Vector2D.h>
#include <carla/rpc/Vector3D.h>
#include <carla/rpc/VehicleDoor.h>
#include <carla/rpc/VehicleAckermannControl.h>
#include <carla/rpc/VehicleControl.h>
#include <carla/rpc/VehiclePhysicsControl.h>
#include <carla/rpc/VehicleLightState.h>
#include <carla/rpc/VehicleLightStateList.h>
#include <carla/rpc/WalkerBoneControlIn.h>
#include <carla/rpc/WalkerBoneControlOut.h>
#include <carla/rpc/RobotBoneControlIn.h>
#include <carla/rpc/RobotBoneControlOut.h>
#include <carla/rpc/WalkerControl.h>
#include <carla/rpc/VehicleWheels.h>
#include <carla/rpc/WeatherParameters.h>
#include <carla/streaming/detail/Types.h>
#include <carla/rpc/Texture.h>
#include <carla/rpc/MaterialParameter.h>
#include <util/enable-ue4-macros.h>
#include "Engine/Engine.h"

#include <util/ue-header-guard-begin.h>
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Animation/PoseSnapshot.h"
#include "Animation/AnimInstance.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/CriticalSection.h"

#include <util/ue-header-guard-end.h>

#include <vector>
#include <atomic>
#include <map>
#include <tuple>
#include <random>

#include "NavigationSystem.h"
#include "Sensor/RayCastLidar.h"
#include "NavMesh/RecastNavMesh.h"
#include "Services/SevnceRobotLogic.h"
#include "SevnceCameraSubsystem.h"

// Geometry drawer subsystem
#include "SvcGeometryDrawerSubsystem.h"

// Line trace subsystem
#include "LineTraceUtils.h"
#include "SvcLineTraceSubsystem.h"

// Map manager subsystem
#include "SvcMapSubsystem.h"

template <typename T>
using R = carla::rpc::Response<T>;

// =============================================================================
// -- Static local functions ---------------------------------------------------
// =============================================================================

template <typename T, typename Other>
static std::vector<T> MakeVectorFromTArray(const TArray<Other> &Array)
{
  return {Array.GetData(), Array.GetData() + Array.Num()};
}

static bool ParseJsonString(const std::string &InJson, TSharedPtr<FJsonObject> &OutObject, FString &OutError)
{
  if (InJson.empty())
  {
    OutError = TEXT("Empty JSON payload");
    return false;
  }

  const FString JsonString = UTF8_TO_TCHAR(InJson.c_str());
  TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
  if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
  {
    OutError = TEXT("Invalid JSON payload");
    return false;
  }
  return true;
}

static std::string MakeJsonResponse(bool bOk, const FString& ErrorMessage = FString(), const TFunction<void(TSharedPtr<FJsonObject>)>& OnSuccess = TFunction<void(TSharedPtr<FJsonObject>)>())
{
  TSharedPtr<FJsonObject> ResponseObject = MakeShareable(new FJsonObject);
  ResponseObject->SetBoolField(TEXT("ok"), bOk);
  if (bOk)
  {
    if (OnSuccess)
    {
      OnSuccess(ResponseObject);
    }
  }
  else
  {
    ResponseObject->SetStringField(TEXT("error"), ErrorMessage);
  }

  FString OutputString;
  TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
  FJsonSerializer::Serialize(ResponseObject.ToSharedRef(), Writer);
  return std::string(TCHAR_TO_UTF8(*OutputString));
}

static TMap<FString, FString>& GetResourcePathMap()
{
    // 使用双重检查锁定模式
    static TMap<FString, FString>* ResourcePathMap = nullptr;
    static FCriticalSection InitCriticalSection;
    
    if (!ResourcePathMap)
    {
        FScopeLock Lock(&InitCriticalSection);
        if (!ResourcePathMap)
        {
            static TMap<FString, FString> LocalResourcePathMap = []()
            {
                TMap<FString, FString> Map;
                
                // --- 特效 ---
                Map.Add(TEXT("Effect.Fire"), TEXT("/Game/Sevnce/CarVFX/BP_Fire.BP_Fire_C"));
                Map.Add(TEXT("Effect.Smoke01"), TEXT("/Game/Sevnce/CarVFX/BP_Smoke01.BP_Smoke01_C"));
                Map.Add(TEXT("Effect.Smoke02"), TEXT("/Game/Sevnce/CarVFX/BP_Smoke02.BP_Smoke02_C"));
                Map.Add(TEXT("Effect.Smoke03"), TEXT("/Game/Sevnce/CarVFX/BP_Smoke03.BP_Smoke03_C"));
                
                // --- 角色与网格体 ---
                Map.Add(TEXT("Human.Worker01"), TEXT("/Game/Sevnce/Worker/BP_Worker01.BP_Worker01_C"));
                
                // --- 道具
                Map.Add(TEXT("Prop.Gauge01"), TEXT("/Game/Sevnce/Props/BP_Gauge01.BP_Gauge01_C"));
                Map.Add(TEXT("Prop.Barrel01"), TEXT("/Game/Sevnce/Props/BP_Barrel01.BP_Barrel01_C"));
                Map.Add(TEXT("Prop.KoreanFireExtinguisher"), TEXT("/Game/Sevnce/Props/BP_KoreanFireExtinguisher.BP_KoreanFireExtinguisher_C"));
                Map.Add(TEXT("Prop.RoadBarrier"), TEXT("/Game/Sevnce/Props/BP_RoadBarrier.BP_RoadBarrier_C"));
                Map.Add(TEXT("Prop.WetFloorSign"), TEXT("/Game/Sevnce/Props/BP_WetFloorSign.BP_WetFloorSign_C"));

                return Map;
            }();
            ResourcePathMap = &LocalResourcePathMap;
        }
    }
    return *ResourcePathMap;
}

// 辅助函数：根据key加载对应的Actor类
static TSubclassOf<AActor> LoadActorClass(const FString& Key)
{
    const TMap<FString, FString>& ResourcePathMap = GetResourcePathMap();
    const FString* AssetPathPtr = ResourcePathMap.Find(Key);
    
    if (!AssetPathPtr || AssetPathPtr->IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Asset path not found for key: %s"), *Key);
        return nullptr;
    }
    
    // 每次都重新加载，避免野指针问题
    TSubclassOf<AActor> ActorClass = LoadClass<AActor>(nullptr, **AssetPathPtr);
    if (!ActorClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load actor class for key: %s, path: %s"), *Key, **AssetPathPtr);
    }
    
    return ActorClass;
}

// =============================================================================
// -- FCarlaServer::FPimpl -----------------------------------------------
// =============================================================================

class FCarlaServer::FPimpl
{
public:

  FPimpl(uint16_t RPCPort, uint16_t StreamingPort, uint16_t SecondaryPort)
    : Server(RPCPort),
      StreamingServer(StreamingPort),
      BroadcastStream(StreamingServer.MakeStream())
  {
    // we need to create shared_ptr from the router for some handlers to live
    SecondaryServer = std::make_shared<carla::multigpu::Router>(SecondaryPort);
    SecondaryServer->SetCallbacks();
    BindActions();
  }

  std::shared_ptr<carla::multigpu::Router> GetSecondaryServer() {
    return SecondaryServer;
  }

  /// Map of pairs < port , ip > with all the Traffic Managers active in the simulation
  std::map<uint16_t, std::string> TrafficManagerInfo;

  carla::rpc::Server Server;

  carla::streaming::Server StreamingServer;

  carla::streaming::Stream BroadcastStream;

  std::shared_ptr<carla::multigpu::Router> SecondaryServer;

  UCarlaEpisode *Episode = nullptr;

  std::atomic_size_t TickCuesReceived { 0u };


	FCriticalSection ActorMapCriticalSection;  // 用于保护 CreatedActorMap 的访问
	FCriticalSection ResourceMapCriticalSection; // 用于保护 ResourceMap 的访问（如果需要）
private:

  void BindActions();
};

// =============================================================================
// -- Define helper macros -----------------------------------------------------
// =============================================================================

#if WITH_EDITOR
#  define CARLA_ENSURE_GAME_THREAD() check(IsInGameThread());
#else
#  define CARLA_ENSURE_GAME_THREAD()
#endif // WITH_EDITOR

#define RESPOND_ERROR(str) {                                              \
    UE_LOG(LogCarlaServer, Log, TEXT("Responding error: %s"), TEXT(str)); \
    return carla::rpc::ResponseError(str); }

#define RESPOND_ERROR_FSTRING(fstr) {                                 \
    UE_LOG(LogCarlaServer, Log, TEXT("Responding error: %s"), *fstr); \
    return carla::rpc::ResponseError(carla::rpc::FromFString(fstr)); }

#define REQUIRE_CARLA_EPISODE() \
    CARLA_ENSURE_GAME_THREAD();   \
    if (Episode == nullptr) { RESPOND_ERROR("episode not ready"); }

#define REQUIRE_CARLA_GAME_MODE() \
    ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld()); \
    if (!GameMode) \
    { \
      RESPOND_ERROR("unable to find CARLA game mode"); \
    }

carla::rpc::ResponseError RespondError(
    const FString& FuncName,
    const FString& ErrorMessage,
    const FString& ExtraInfo = "")
{
  FString TotalMessage = "Responding error from function " + FuncName + ": " +
      ErrorMessage + ". " + ExtraInfo;
  UE_LOG(LogCarlaServer, Log, TEXT("%s"), *TotalMessage);
  return carla::rpc::ResponseError(carla::rpc::FromFString(TotalMessage));
}

carla::rpc::ResponseError RespondError(
    const FString& FuncName,
    const ECarlaServerResponse& Error,
    const FString& ExtraInfo = "")
{
  return RespondError(FuncName, CarlaGetStringError(Error), ExtraInfo);
}

template <typename T>
static R<void> ApplyTextureToActor(
  UCarlaEpisode* Episode,
  carla::rpc::ActorId ActorId,
  const carla::rpc::MaterialParameter& MaterialParameter,
  T&& Texture)
{
  auto CarlaActor = Episode->FindCarlaActor(ActorId);
  if (CarlaActor == nullptr)
  {
    RESPOND_ERROR("Could not find CARLA Actor.");
  }
  auto Actor = CarlaActor->GetActor();
  check(Actor);
  REQUIRE_CARLA_GAME_MODE()
  auto TextureUE = GameMode->CreateUETexture(Texture);
  if (TextureUE == nullptr)
  {
    RESPOND_ERROR("CreateUETexture failed.");
  }
  GameMode->ApplyTextureToActor(Actor, TextureUE, MaterialParameter);
  return R<void>::Success();
}

class ServerBinder
{
public:

  constexpr ServerBinder(const char *name, carla::rpc::Server &srv, bool sync)
    : _name(name),
      _server(srv),
      _sync(sync) {}

  template <typename FuncT>
  auto operator<<(FuncT func)
  {
    if (_sync)
    {
      _server.BindSync(_name, func);
    }
    else
    {
      _server.BindAsync(_name, func);
    }
    return func;
  }

private:

  const char *_name;

  carla::rpc::Server &_server;

  bool _sync;
};

#define BIND_SYNC(name)   auto name = ServerBinder(# name, Server, true)
#define BIND_ASYNC(name)  auto name = ServerBinder(# name, Server, false)

// =============================================================================
// -- Bind Actions -------------------------------------------------------------
// =============================================================================

void FCarlaServer::FPimpl::BindActions()
{
	namespace cr = carla::rpc;
	namespace cg = carla::geom;

	/// Looks for a Traffic Manager running on port
	BIND_SYNC(is_traffic_manager_running) << [this] (uint16_t port) ->R<bool>
	{
		return (TrafficManagerInfo.find(port) != TrafficManagerInfo.end());
	};

	/// Gets a pair filled with the <IP, port> of the Trafic Manager running on port.
	/// If there is no Traffic Manager running the pair will be ("", 0)
	BIND_SYNC(get_traffic_manager_running) << [this] (uint16_t port) ->R<std::pair<std::string, uint16_t>>
	{
		auto it = TrafficManagerInfo.find(port);
		if(it != TrafficManagerInfo.end()) {
			return std::pair<std::string, uint16_t>(it->second, it->first);
		}
		return std::pair<std::string, uint16_t>("",0);
	};

	/// Add a new Traffic Manager running on <IP, port>
	BIND_SYNC(add_traffic_manager_running) << [this] (std::pair<std::string, uint16_t> trafficManagerInfo) ->R<bool>
	{
		uint16_t port = trafficManagerInfo.second;
		auto it = TrafficManagerInfo.find(port);
		if(it == TrafficManagerInfo.end()) {
			TrafficManagerInfo.insert(
			  std::pair<uint16_t, std::string>(port, trafficManagerInfo.first));
			return true;
		}
		return false;

	};

	BIND_SYNC(destroy_traffic_manager) << [this] (uint16_t port) ->R<bool>
	{
		auto it = TrafficManagerInfo.find(port);
		if(it != TrafficManagerInfo.end()) {
			TrafficManagerInfo.erase(it);
			return true;
		}
		return false;
	};

	BIND_ASYNC(version) << [] () -> R<std::string>
	{
		return carla::version();
	};

	// ~~ Tick ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	BIND_SYNC(tick_cue) << [this]() -> R<uint64_t>
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TickCueReceived);
		auto Current = FCarlaEngine::GetFrameCounter();
		(void)TickCuesReceived.fetch_add(1, std::memory_order_release);
		return Current + 1;
	};

	// ~~ Load new episode ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	BIND_ASYNC(get_available_maps) << [this]() -> R<std::vector<std::string>>
	{
		const auto MapNames = UCarlaStatics::GetAllMapNames();
		std::vector<std::string> result;
		result.reserve(MapNames.Num());
		for (const auto &MapName : MapNames)
		{
			if (MapName.Contains("/Sublevels/"))
				continue;
			if (MapName.Contains("/BaseMap/"))
				continue;
			if (MapName.Contains("/BaseLargeMap/"))
				continue;
			if (MapName.Contains("_Tile_"))
				continue;

			result.emplace_back(cr::FromFString(MapName));
		}
		return result;
	};

	BIND_SYNC(load_new_episode) << [this](const std::string &map_name, const bool reset_settings, cr::MapLayer MapLayers) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();

		UCarlaGameInstance* GameInstance = UCarlaStatics::GetGameInstance(Episode->GetWorld());
		if (!GameInstance)
		{
			RESPOND_ERROR("unable to find CARLA game instance");
		}
		GameInstance->SetMapLayer(static_cast<int32>(MapLayers));

		if(!Episode->LoadNewEpisode(cr::ToFString(map_name), reset_settings))
		{
			FString Str(TEXT("Map '"));
			Str += cr::ToFString(map_name);
			Str += TEXT("' not found");
			RESPOND_ERROR_FSTRING(Str);
		}

		return R<void>::Success();
	};

	BIND_SYNC(load_map_layer) << [this](cr::MapLayer MapLayers) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();

		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		if (!GameMode)
		{
			RESPOND_ERROR("unable to find CARLA game mode");
		}
		GameMode->LoadMapLayer(static_cast<int32>(MapLayers));

		return R<void>::Success();
	};

	BIND_SYNC(unload_map_layer) << [this](cr::MapLayer MapLayers) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();

		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		if (!GameMode)
		{
			RESPOND_ERROR("unable to find CARLA game mode");
		}
		GameMode->UnLoadMapLayer(static_cast<int32>(MapLayers));

		return R<void>::Success();
	};

	BIND_SYNC(copy_opendrive_to_file) << [this](const std::string &opendrive, cr::OpendriveGenerationParameters Params) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		if (!Episode->LoadNewOpendriveEpisode(cr::ToLongFString(opendrive), Params))
		{
			RESPOND_ERROR("opendrive could not be correctly parsed");
		}
		return R<void>::Success();
	};

	BIND_SYNC(apply_texture_to_actor) << [this](
	  cr::ActorId ActorId,
	  const cr::MaterialParameter& MaterialParameter,
	  const cr::TextureColor& Texture) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		::ApplyTextureToActor(Episode, ActorId, MaterialParameter, Texture);
		return R<void>::Success();
	};

	BIND_SYNC(apply_texture_to_actor_float) << [this](
	  cr::ActorId ActorId,
	  const cr::MaterialParameter& MaterialParameter,
	  const cr::TextureFloatColor& Texture) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		::ApplyTextureToActor(Episode, ActorId, MaterialParameter, Texture);
		return R<void>::Success();
	};

	BIND_SYNC(apply_color_texture_to_objects) << [this](
		const std::vector<std::string> &actors_name,
		const cr::MaterialParameter& parameter,
		const cr::TextureColor& Texture) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		if (!GameMode)
		{
			RESPOND_ERROR("unable to find CARLA game mode");
		}
		TArray<AActor*> ActorsToPaint;
		for(const std::string& actor_name : actors_name)
		{
			AActor* ActorToPaint = GameMode->FindActorByName(cr::ToFString(actor_name));
			if (ActorToPaint)
			{
				ActorsToPaint.Add(ActorToPaint);
			}
		}

		if(!ActorsToPaint.Num())
		{
			RESPOND_ERROR("unable to find Actor to apply the texture");
		}

		UTexture2D* UETexture = GameMode->CreateUETexture(Texture);

		for(AActor* ActorToPaint : ActorsToPaint)
		{
			GameMode->ApplyTextureToActor(
				ActorToPaint,
				UETexture,
				parameter);
		}
		return R<void>::Success();
	};

	BIND_SYNC(apply_float_color_texture_to_objects) << [this](
		const std::vector<std::string> &actors_name,
		const cr::MaterialParameter& parameter,
		const cr::TextureFloatColor& Texture) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		if (!GameMode)
		{
			RESPOND_ERROR("unable to find CARLA game mode");
		}
		TArray<AActor*> ActorsToPaint;
		for(const std::string& actor_name : actors_name)
		{
			AActor* ActorToPaint = GameMode->FindActorByName(cr::ToFString(actor_name));
			if (ActorToPaint)
			{
				ActorsToPaint.Add(ActorToPaint);
			}
		}

		if(!ActorsToPaint.Num())
		{
			RESPOND_ERROR("unable to find Actor to apply the texture");
		}

		UTexture2D* UETexture = GameMode->CreateUETexture(Texture);

		for(AActor* ActorToPaint : ActorsToPaint)
		{
			GameMode->ApplyTextureToActor(
				ActorToPaint,
				UETexture,
				parameter);
		}
		return R<void>::Success();
	};

	BIND_SYNC(get_names_of_all_objects) << [this]() -> R<std::vector<std::string>>
	{
		REQUIRE_CARLA_EPISODE();
		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		if (!GameMode)
		{
			RESPOND_ERROR("unable to find CARLA game mode");
		}
		TArray<FString> NamesFString = GameMode->GetNamesOfAllActors();
		std::vector<std::string> NamesStd;
		for (const FString &Name : NamesFString)
		{
			NamesStd.emplace_back(cr::FromFString(Name));
		}
		return NamesStd;
	};

	// ~~ Episode settings and info ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	BIND_SYNC(get_episode_info) << [this]() -> R<cr::EpisodeInfo>
	{
		REQUIRE_CARLA_EPISODE();
		return cr::EpisodeInfo{Episode->GetId(), BroadcastStream.token()};
	};

	BIND_SYNC(get_map_info) << [this]() -> R<cr::MapInfo>
	{
		REQUIRE_CARLA_EPISODE();
		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		const auto &SpawnPoints = Episode->GetRecommendedSpawnPoints();
		FString FullMapPath = GameMode->GetFullMapPath();
		FString MapDir = FullMapPath.RightChop(FullMapPath.Find("Content/", ESearchCase::CaseSensitive) + 8);
		MapDir += "/" + Episode->GetMapName();
		return cr::MapInfo{
			cr::FromFString(MapDir),
			MakeVectorFromTArray<cg::Transform>(SpawnPoints)};
	};

	BIND_SYNC(get_map_data) << [this]() -> R<std::string>
	{
		REQUIRE_CARLA_EPISODE();
		return cr::FromLongFString(UOpenDrive::GetXODR(Episode->GetWorld()));
	};

	BIND_SYNC(get_navigation_mesh) << [this]() -> R<std::vector<uint8_t>>
	{
		REQUIRE_CARLA_EPISODE();
		auto FileContents = FNavigationMesh::Load(Episode->GetMapName());
		// make a mem copy (from TArray to std::vector)
		std::vector<uint8_t> Result(FileContents.Num());
		memcpy(&Result[0], FileContents.GetData(), FileContents.Num());
		return Result;
	};

	BIND_SYNC(get_required_files) << [this](std::string folder = "") -> R<std::vector<std::string>>
	{
		REQUIRE_CARLA_EPISODE();

		// Check that the path ends in a slash, add it otherwise
		if (!folder.empty() && folder[folder.size() - 1] != '/' && folder[folder.size() - 1] != '\\') {
			folder += "/";
		}

		// Get the map's folder absolute path and check if it's in its own folder
		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		const auto mapDir = GameMode->GetFullMapPath();
		const auto folderDir = mapDir + "/" + folder.c_str();
		const auto fileName = mapDir.EndsWith(Episode->GetMapName()) ? "*" : Episode->GetMapName();

		// Find all the xodr and bin files from the map
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *folderDir, *(fileName + ".xodr"), true, false, false);
		IFileManager::Get().FindFilesRecursive(Files, *folderDir, *(fileName + ".bin"), true, false, false);

		// Remove the start of the path until the content folder and put each file in the result
		std::vector<std::string> result;
		for (auto File : Files) {
			File.RemoveFromStart(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
			result.emplace_back(TCHAR_TO_UTF8(*File));
		}

		return result;
	};
	BIND_SYNC(request_file) << [this](std::string name) -> R<std::vector<uint8_t>>
	{
		REQUIRE_CARLA_EPISODE();

		// Get the absolute path of the file
		FString path(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()));
		path.Append(name.c_str());

		// Copy the binary data of the file into the result and return it
		TArray<uint8_t> Content;
		FFileHelper::LoadFileToArray(Content, *path, 0);
		std::vector<uint8_t> Result(Content.Num());
		memcpy(&Result[0], Content.GetData(), Content.Num());

		return Result;
	};

	BIND_SYNC(get_episode_settings) << [this]() -> R<cr::EpisodeSettings>
	{
		REQUIRE_CARLA_EPISODE();
		return cr::EpisodeSettings{Episode->GetSettings()};
	};

	BIND_SYNC(set_episode_settings) << [this](
		const cr::EpisodeSettings &settings) -> R<uint64_t>
	{
		REQUIRE_CARLA_EPISODE();
		Episode->ApplySettings(settings);
		StreamingServer.SetSynchronousMode(settings.synchronous_mode);

		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		if (!GameMode)
		{
			RESPOND_ERROR("unable to find CARLA game mode");
		}
		ALargeMapManager* LargeMap = GameMode->GetLMManager();
		if (LargeMap)
		{
			LargeMap->ConsiderSpectatorAsEgo(settings.spectator_as_ego);
		}

		return FCarlaEngine::GetFrameCounter();
	};

	BIND_SYNC(get_actor_definitions) << [this]() -> R<std::vector<cr::ActorDefinition>>
	{
		REQUIRE_CARLA_EPISODE();
		return MakeVectorFromTArray<cr::ActorDefinition>(Episode->GetActorDefinitions());
	};

	BIND_SYNC(get_spectator) << [this]() -> R<cr::Actor>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(Episode->GetSpectatorPawn());
		if (!CarlaActor)
		{
			RESPOND_ERROR("internal error: unable to find spectator");
		}
		return Episode->SerializeActor(CarlaActor);
	};

	BIND_SYNC(get_all_level_BBs) << [this](uint8 QueriedTag) -> R<std::vector<cg::BoundingBox>>
	{
		REQUIRE_CARLA_EPISODE();
		TArray<FBoundingBox> Result;
		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		if (!GameMode)
		{
			RESPOND_ERROR("unable to find CARLA game mode");
		}
		Result = GameMode->GetAllBBsOfLevel(QueriedTag);
		ALargeMapManager* LargeMap = GameMode->GetLMManager();
		if (LargeMap)
		{
			for(auto& Box : Result)
			{
				Box.Origin = LargeMap->LocalToGlobalLocation(Box.Origin);
			}
		}
		return MakeVectorFromTArray<cg::BoundingBox>(Result);
	};

	BIND_SYNC(get_environment_objects) << [this](uint8 QueriedTag) -> R<std::vector<cr::EnvironmentObject>>
	{
		REQUIRE_CARLA_EPISODE();
		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		if (!GameMode)
		{
			RESPOND_ERROR("unable to find CARLA game mode");
		}
		TArray<FEnvironmentObject> Result = GameMode->GetEnvironmentObjects(QueriedTag);
		ALargeMapManager* LargeMap = GameMode->GetLMManager();
		if (LargeMap)
		{
			for(auto& Object : Result)
			{
				Object.Transform = LargeMap->LocalToGlobalTransform(Object.Transform);
			}
		}
		return MakeVectorFromTArray<cr::EnvironmentObject>(Result);
	};

	BIND_SYNC(enable_environment_objects) << [this](std::vector<uint64_t> EnvObjectIds, bool Enable) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		if (!GameMode)
		{
			RESPOND_ERROR("unable to find CARLA game mode");
		}

		TSet<uint64> EnvObjectIdsSet;
		for(uint64 Id : EnvObjectIds)
		{
			EnvObjectIdsSet.Emplace(Id);
		}

		GameMode->EnableEnvironmentObjects(EnvObjectIdsSet, Enable);
		return R<void>::Success();
	};

	// ~~ Weather ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	BIND_SYNC(get_weather_parameters) << [this]() -> R<cr::WeatherParameters>
	{
		REQUIRE_CARLA_EPISODE();
		auto *Weather = Episode->GetWeather();
		if (Weather == nullptr)
		{
			UE_LOG(LogCarla, Log, TEXT("internal error: unable to find weather:: weather is disabled"));
			return cr::WeatherParameters();
		}
		return Weather->GetCurrentWeather();
	};

	BIND_SYNC(set_weather_parameters) << [this](
		const cr::WeatherParameters &weather) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		auto *Weather = Episode->GetWeather();
		if (Weather == nullptr)
		{
			RESPOND_ERROR("set_weather_parameters internal error: unable to find weather:: weather is disabled");
		}
		Weather->ApplyWeather(weather);
		return R<void>::Success();
	};

	BIND_SYNC(is_weather_enabled) << [this]() -> R<bool>
	{
		REQUIRE_CARLA_EPISODE();
		auto *Weather = Episode->GetWeather();
		if (Weather == nullptr)
		{
			return false;
		}
		return true;
	};

	// ~~ Actor operations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	BIND_SYNC(get_actors_by_id) << [this](
		const std::vector<FCarlaActor::IdType> &ids) -> R<std::vector<cr::Actor>>
	{
		REQUIRE_CARLA_EPISODE();
		std::vector<cr::Actor> Result;
		Result.reserve(ids.size());
		for (auto &&Id : ids)
		{
			FCarlaActor* View = Episode->FindCarlaActor(Id);
			if (View)
			{
				Result.emplace_back(Episode->SerializeActor(View));
			}
		}
		return Result;
	};

	BIND_SYNC(spawn_actor) << [this](
		cr::ActorDescription Description,
		const cr::Transform &Transform) -> R<cr::Actor>
	{
		REQUIRE_CARLA_EPISODE();

		FCarlaActor* Result = SvcActorLogic::SpawnActor(Episode, Description, Transform);
		if (!Result)
		{
			RESPOND_ERROR("Failed to spawn actor");
		}

		return Episode->SerializeActor(Result);
	};

	BIND_SYNC(spawn_actor_with_parent) << [this](
		cr::ActorDescription Description,
		const cr::Transform &Transform,
		cr::ActorId ParentId,
		cr::AttachmentType InAttachmentType) -> R<cr::Actor>
	{
		REQUIRE_CARLA_EPISODE();

		FCarlaActor* CarlaActor = SvcActorLogic::SpawnActorWithParent(Episode, Description, Transform, ParentId, InAttachmentType);
		if (!CarlaActor)
		{
			RESPOND_ERROR("Failed to spawn actor with parent");
		}

		return Episode->SerializeActor(CarlaActor);
	};

	BIND_SYNC(create_robot) << [this](std::string json) -> R<std::string>
	{
		REQUIRE_CARLA_EPISODE();
		FString JsonString(UTF8_TO_TCHAR(json.c_str()));
		FString ResultJson = SvcRobotLogic::CreateRobot(Episode, JsonString);
		FString TrimmedResult(ResultJson);
		TrimmedResult.TrimStartAndEndInline();
		if (TrimmedResult.IsEmpty())
		{
			return MakeJsonResponse(false, TEXT("create_robot failed"));
		}
		return std::string(TCHAR_TO_UTF8(*ResultJson));
	};

	BIND_SYNC(destroy_robot) << [this](cr::ActorId RobotId) -> R<bool>
	{
		REQUIRE_CARLA_EPISODE();
		bool ok = SvcRobotLogic::DestroyRobot(Episode, RobotId);
		return ok;
	};
	
	BIND_SYNC(create_object) << [this](std::string json) -> R<std::string>
	{
		REQUIRE_CARLA_EPISODE();

		FScopeLock Lock(&ActorMapCriticalSection);  // 加锁
		
		TSharedPtr<FJsonObject> JsonObject;
		FString ErrorMessage;
		if (!ParseJsonString(json, JsonObject, ErrorMessage))
		{
			return MakeJsonResponse(false, ErrorMessage);
		}
		
		FString type;
		if (!JsonObject->TryGetStringField(TEXT("type"), type))
		{
			return MakeJsonResponse(false, TEXT("Missing type field"));
		}
		
		const TSharedPtr<FJsonObject>* paramsObj;
		if (!JsonObject->TryGetObjectField(TEXT("params"), paramsObj))
		{
			return MakeJsonResponse(false, TEXT("Missing params field"));
		}
		
		FString category;
		if (!(*paramsObj)->TryGetStringField(TEXT("category"), category))
		{
			return MakeJsonResponse(false, TEXT("Missing category field"));
		}
		
		const TSharedPtr<FJsonObject>* transformObj;
		FVector location = FVector::ZeroVector;
		FRotator rotation = FRotator::ZeroRotator;
		FVector scale = FVector::OneVector;
		if ((*paramsObj)->TryGetObjectField(TEXT("transform"), transformObj))
		{
			const TSharedPtr<FJsonObject>* locObj;
			if ((*transformObj)->TryGetObjectField(TEXT("location"), locObj))
			{
				double x, y, z;
				if ((*locObj)->TryGetNumberField(TEXT("x"), x))
				{
					location.X = x * 100.0;
				}
				if ((*locObj)->TryGetNumberField(TEXT("y"), y))
				{
					location.Y = y * 100.0;
				}
				if ((*locObj)->TryGetNumberField(TEXT("z"), z))
				{
					location.Z = z * 100.0;
				}
			}

			const TSharedPtr<FJsonObject>* rotObj;
			if ((*transformObj)->TryGetObjectField(TEXT("rotation"), rotObj))
			{
				double p, yaw, r;
				if ((*rotObj)->TryGetNumberField(TEXT("pitch"), p)) rotation.Pitch = p;
				if ((*rotObj)->TryGetNumberField(TEXT("yaw"), yaw)) rotation.Yaw = yaw;
				if ((*rotObj)->TryGetNumberField(TEXT("roll"), r)) rotation.Roll = r;
			}

			const TSharedPtr<FJsonObject>* scaleObj;
			if ((*transformObj)->TryGetObjectField(TEXT("scale"), scaleObj))
			{
				double sx, sy, sz;
				if ((*scaleObj)->TryGetNumberField(TEXT("x"), sx)) scale.X = sx;
				if ((*scaleObj)->TryGetNumberField(TEXT("y"), sy)) scale.Y = sy;
				if ((*scaleObj)->TryGetNumberField(TEXT("z"), sz)) scale.Z = sz;
			}
		}
		
		// 3. 拼接 Key 并查找
		FString lookupKey = FString::Printf(TEXT("%s.%s"), *type, *category);
		UE_LOG(LogTemp, Warning, TEXT("Looking for resource with key: %s"), *lookupKey);
    
		// 使用新的加载方式，每次都重新加载
		TSubclassOf<AActor> actorClass = LoadActorClass(lookupKey);

		if (!actorClass)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to load actor class for key: %s"), *lookupKey);
			return MakeJsonResponse(false, FString::Printf(TEXT("Resource '%s' not found"), *lookupKey));
		}

		UWorld* world = GEngine->GetWorldFromContextObjectChecked(GEngine->GetCurrentPlayWorld());
		if (!world)
		{
			UE_LOG(LogTemp, Warning, TEXT("No valid world to spawn actor."));
			return MakeJsonResponse(false, TEXT("No valid world to spawn actor"));
		}

		FActorSpawnParameters spawnParams;
		AActor* spawnedActor = world->SpawnActor<AActor>(actorClass, location, rotation, spawnParams);
		if (!spawnedActor)
		{
			return MakeJsonResponse(false, TEXT("Failed to spawn actor"));
		}

		spawnedActor->SetActorScale3D(scale);
		if (UNiagaraComponent* niagaraComp = spawnedActor->FindComponentByClass<UNiagaraComponent>())
		{
			niagaraComp->TranslucencySortPriority = 50;
		}
		if (UPrimitiveComponent* meshComp = spawnedActor->FindComponentByClass<UPrimitiveComponent>())
		{
			// 1. 把物体设为世界静态物体（车肯定会撞它）
			meshComp->SetCollisionObjectType(ECC_WorldStatic);
			meshComp->SetCollisionEnabled(ECollisionEnabled::Type::QueryAndPhysics);
			meshComp->SetCollisionResponseToAllChannels(ECR_Block);
			meshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel4, ECR_Block);
			meshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel5, ECR_Block);
		}

		FString uuidFStr = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
		Episode->CreatedActorMap.Add(uuidFStr, spawnedActor);
		
		return MakeJsonResponse(true, FString(), [uuidFStr](TSharedPtr<FJsonObject> JsonResponse)
		{
			JsonResponse->SetStringField(TEXT("uuid"), uuidFStr);
		});
	};
	
	BIND_SYNC(destroy_object) << [this](std::string uuidStr) -> R<bool>
	{
		REQUIRE_CARLA_EPISODE();

		FScopeLock Lock(&ActorMapCriticalSection);  // 加锁

		FString uuid = UTF8_TO_TCHAR(uuidStr.c_str());
		
		TWeakObjectPtr<AActor>* actorPtr = Episode->CreatedActorMap.Find(uuid);
		if (!actorPtr || !actorPtr->IsValid())
			return false;
		
		AActor* actor = actorPtr->Get();
		if (!actor)
		{
			Episode->CreatedActorMap.Remove(uuid);
			return false;
		}
		
		UWorld* world = actor->GetWorld();
		if (!world)
		{
			Episode->CreatedActorMap.Remove(uuid);
			return false;
		}

		if (!world->DestroyActor(actor))
		{
			Episode->CreatedActorMap.Remove(uuid);
			return false;
		}

		Episode->CreatedActorMap.Remove(uuid);

		return true;
	};

	BIND_SYNC(destroy_objects) << [this](std::string jsonStr) -> R<std::string>
	{
		REQUIRE_CARLA_EPISODE();
		FScopeLock Lock(&ActorMapCriticalSection);  // 加锁
		TSharedPtr<FJsonObject> JsonObject;
		FString ErrorMessage;
		if (!ParseJsonString(jsonStr, JsonObject, ErrorMessage))
		{
			return MakeJsonResponse(false, ErrorMessage);
		}

		FString type; // 例如 "Effect", "Prop", "all"
		if (!JsonObject->TryGetStringField(TEXT("type"), type))
		{
			return MakeJsonResponse(false, TEXT("Missing type field"));
		}

		// 特殊处理：如果type是"all"，则删除所有actor
		if (type.Equals(TEXT("all"), ESearchCase::IgnoreCase))
		{
			UE_LOG(LogTemp, Warning, TEXT("Destroying ALL actors in CreatedActorMap"));
			
			TArray<FString> allUuids;
			TArray<AActor*> allActors;
			
			// 收集所有actor
			for (auto& pair : Episode->CreatedActorMap)
			{
				const FString& uuid = pair.Key;
				const TWeakObjectPtr<AActor>& actorPtr = pair.Value;
				
				allUuids.Add(uuid);
				
				if (actorPtr.IsValid())
				{
					AActor* actor = actorPtr.Get();
					if (actor && actor->IsValidLowLevelFast() && !actor->IsPendingKillPending())
					{
						allActors.Add(actor);
					}
				}
			}
			
			// 销毁所有actor
			int32 destroyedCount = 0;
			for (int32 i = 0; i < allActors.Num(); ++i)
			{
				AActor* actor = allActors[i];
				const FString& uuid = allUuids[i];
				
				if (!actor || !actor->IsValidLowLevelFast() || actor->IsPendingKillPending())
				{
					continue;
				}
				
				UWorld* world = actor->GetWorld();
				if (world && world->DestroyActor(actor))
				{
					destroyedCount++;
					UE_LOG(LogTemp, Warning, TEXT("Destroyed actor: %s (uuid: %s)"), *actor->GetName(), *uuid);
				}
			}
			
			// 清空整个map
			Episode->CreatedActorMap.Empty();
			
			UE_LOG(LogTemp, Warning, TEXT("Destroyed ALL actors. Total destroyed: %d"), destroyedCount);
			
			return MakeJsonResponse(true, FString(), [destroyedCount](TSharedPtr<FJsonObject> JsonResponse)
			{
				JsonResponse->SetNumberField(TEXT("destroyed_count"), destroyedCount);
			});
		}

		// --- 准备阶段：解析目标类型 ---
		const TMap<FString, FString>& ResourcePathMap = GetResourcePathMap();
		if (ResourcePathMap.Num() == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("ResourcePathMap is empty!"));
			return MakeJsonResponse(false, TEXT("ResourcePathMap not initialized"));
		}
		
		TArray<FString> targetKeys;
		
		// 1. 构造前缀 (例如 "Prop.")
		FString prefix = FString::Printf(TEXT("%s."), *type);
		UE_LOG(LogTemp, Warning, TEXT("Looking for objects with prefix: %s"), *prefix);

		// 收集所有匹配的key
		for (const auto& pair : ResourcePathMap)
		{
			if (pair.Key.StartsWith(prefix, ESearchCase::IgnoreCase))
			{
				if (!pair.Value.IsEmpty())
				{
					targetKeys.Add(pair.Key);
					UE_LOG(LogTemp, Warning, TEXT("Found target key: %s -> %s"), *pair.Key, *pair.Value);
				}
			}
		}

		if (targetKeys.Num() == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("No matching keys found for prefix: %s"), *prefix);
			return MakeJsonResponse(false, TEXT("No matching classes found for this type"), [](TSharedPtr<FJsonObject> JsonResponse)
			{
				JsonResponse->SetNumberField(TEXT("destroyed_count"), 0);
			});
		}

		// --- 收集阶段：只查找，不销毁 ---
		
		// 用于存储待清理的 UUID (用于更新 Map)
		TArray<FString> uuidsToRemove;
		// 用于存储待销毁的 Actor 指针 (用于执行销毁)
		TArray<AActor*> actorsToDestroy;

		// 2. 遍历当前所有已创建的 Actor
		UE_LOG(LogTemp, Warning, TEXT("Starting to iterate through %d actors in CreatedActorMap"), Episode->CreatedActorMap.Num());
		
		for (auto& pair : Episode->CreatedActorMap)
		{
			const FString& uuid = pair.Key;
			const TWeakObjectPtr<AActor>& actorPtr = pair.Value;

			// 2.1 清理已失效的弱引用
			if (!actorPtr.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("Found invalid actor pointer for uuid: %s"), *uuid);
				uuidsToRemove.Add(uuid);
				continue;
			}

			AActor* actor = actorPtr.Get();
			
			// 增强安全检查：确保Actor完全有效
			if (!actor || !actor->IsValidLowLevelFast() || actor->IsPendingKillPending())
			{
				UE_LOG(LogTemp, Warning, TEXT("Found invalid actor for uuid: %s"), *uuid);
				uuidsToRemove.Add(uuid);
				continue;
			}

			// 2.2 检查该 Actor 是否属于目标类别
			bool bShouldDestroy = false;
			UClass* actorClass = actor->GetClass();
			
			// 安全检查：防止 Class 指针无效
			if (!actorClass || !actorClass->IsValidLowLevel())
			{
				UE_LOG(LogTemp, Warning, TEXT("Found invalid actor class for uuid: %s"), *uuid);
				uuidsToRemove.Add(uuid);
				continue;
			}

			// 检查Actor类是否匹配目标类型
			FString actorClassName = actorClass->GetName();
			UE_LOG(LogTemp, VeryVerbose, TEXT("Checking actor: %s (class: %s)"), *actor->GetName(), *actorClassName);
			
			// 对每个目标key都重新加载类进行比较
			for (const FString& targetKey : targetKeys)
			{
				TSubclassOf<AActor> targetClass = LoadActorClass(targetKey);
				if (targetClass && actorClass->IsChildOf(targetClass))
				{
					bShouldDestroy = true;
					UE_LOG(LogTemp, Warning, TEXT("Marked actor for destruction: %s (class: %s, targetKey: %s)"), *actor->GetName(), *actorClassName, *targetKey);
					break;
				}
			}

			if (bShouldDestroy)
			{
				// 只做标记，不调用 DestroyActor
				uuidsToRemove.Add(uuid);
				actorsToDestroy.Add(actor);
			}
		}

		// --- 执行阶段：先销毁对象，后清理引用 ---

		// 3. 第一步：执行物理销毁
		int32 destroyedCount = 0;
		TArray<FString> successfullyRemovedUuids;
		
		UE_LOG(LogTemp, Warning, TEXT("Starting to destroy %d actors"), actorsToDestroy.Num());
		
		for (int32 i = 0; i < actorsToDestroy.Num(); ++i)
		{
			AActor* actor = actorsToDestroy[i];
			const FString& uuid = uuidsToRemove[i];
			
			// 增强安全检查
			if (!actor || !actor->IsValidLowLevelFast() || actor->IsPendingKillPending())
			{
				UE_LOG(LogTemp, Warning, TEXT("Actor already invalid during destruction: %s"), *uuid);
				// Actor已经无效，直接清理引用
				successfullyRemovedUuids.Add(uuid);
				continue;
			}
			
			UWorld* world = actor->GetWorld();
			if (!world)
			{
				UE_LOG(LogTemp, Warning, TEXT("World invalid during actor destruction: %s"), *uuid);
				// World无效，直接清理引用
				successfullyRemovedUuids.Add(uuid);
				continue;
			}
			
			// 执行销毁前再次检查
			FString actorName = actor->GetName();
			UE_LOG(LogTemp, Warning, TEXT("Attempting to destroy actor: %s (uuid: %s)"), *actorName, *uuid);
			
			// 执行销毁
			if (world->DestroyActor(actor))
			{
				destroyedCount++;
				successfullyRemovedUuids.Add(uuid);
				UE_LOG(LogTemp, Warning, TEXT("Successfully destroyed actor: %s"), *actorName);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to destroy actor: %s"), *actorName);
				// 销毁失败也清理引用，避免内存泄漏
				successfullyRemovedUuids.Add(uuid);
			}
		}

		// 4. 第二步：从 Map 中移除所有已处理的引用
		UE_LOG(LogTemp, Warning, TEXT("Cleaning up %d UUID references from CreatedActorMap"), successfullyRemovedUuids.Num());
		
		for (const FString& uuid : successfullyRemovedUuids)
		{
			Episode->CreatedActorMap.Remove(uuid);
		}
		
		UE_LOG(LogTemp, Warning, TEXT("Destruction completed. Successfully destroyed %d actors"), destroyedCount);

		return MakeJsonResponse(true, FString(), [destroyedCount](TSharedPtr<FJsonObject> JsonResponse)
		{
			JsonResponse->SetNumberField(TEXT("destroyed_count"), destroyedCount);
		});
	};


	BIND_SYNC(destroy_actor) << [this](cr::ActorId ActorId) -> R<bool>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if ( !CarlaActor )
		{
			RESPOND_ERROR("unable to destroy actor: not found");
		}
		UE_LOG(LogCarla, Log, TEXT("CarlaServer destroy_actor %d"), ActorId);
		// We need to force the actor state change, since dormant actors
		//  will ignore the FCarlaActor destruction
		CarlaActor->SetActorState(cr::ActorState::PendingKill);
		if (!Episode->DestroyActor(ActorId))
		{
			RESPOND_ERROR("internal error: unable to destroy actor");
		}
		return true;
	};

	BIND_SYNC(console_command) << [this](std::string cmd) -> R<bool>
	{
		REQUIRE_CARLA_EPISODE();
		APlayerController* PController= UGameplayStatics::GetPlayerController(Episode->GetWorld(), 0);
		if( PController )
		{
			auto result = PController->ConsoleCommand(UTF8_TO_TCHAR(cmd.c_str()), true);
			return !(
			  result.Contains(FString(TEXT("Command not recognized"))) ||
			  result.Contains(FString(TEXT("Error")))
			);
		}
		bool success = false;
#if WITH_EDITOR
		success = GEngine->Exec(Episode->GetWorld(), UTF8_TO_TCHAR(cmd.c_str()));
#endif
		return success;
	};

	BIND_SYNC(get_sensor_token) << [this](carla::streaming::detail::stream_id_type sensor_id) ->
								   R<carla::streaming::Token>
	{
		REQUIRE_CARLA_EPISODE();
		bool ForceInPrimary = false;

		// check for the world observer (always in primary server)
		if (sensor_id == 1)
		{
			ForceInPrimary = true;
		}

		// collision sensor always in primary server in multi-gpu
		FString Desc = Episode->GetActorDescriptionFromStream(sensor_id);
		if (Desc == "" || Desc == "sensor.other.collision")
		{
			ForceInPrimary = true;
		}

		if (SecondaryServer->HasClientsConnected() && !ForceInPrimary)
		{
			// multi-gpu
			UE_LOG(LogCarla, Log, TEXT("Sensor %d '%s' created in secondary server"), sensor_id, *Desc);
			return SecondaryServer->GetCommander().GetToken(sensor_id);
		}
		else
		{
			// single-gpu
			UE_LOG(LogCarla, Log, TEXT("Sensor %d '%s' created in primary server"), sensor_id, *Desc);
			return StreamingServer.GetToken(sensor_id);
		}
	};

	BIND_SYNC(enable_sensor_for_ros) << [this](carla::streaming::detail::stream_id_type sensor_id) ->
								   R<void>
	{
		REQUIRE_CARLA_EPISODE();
		bool ForceInPrimary = false;

		// check for the world observer (always in primary server)
		if (sensor_id == 1)
		{
			ForceInPrimary = true;
		}

		// collision sensor always in primary server in multi-gpu
		FString Desc = Episode->GetActorDescriptionFromStream(sensor_id);
		if (Desc == "" || Desc == "sensor.other.collision")
		{
			ForceInPrimary = true;
		}

		if (SecondaryServer->HasClientsConnected() && !ForceInPrimary)
		{
			// multi-gpu
			SecondaryServer->GetCommander().EnableForROS(sensor_id);
		}
		else
		{
			// single-gpu
			StreamingServer.EnableForROS(sensor_id);
		}
		return R<void>::Success();
	};

	BIND_SYNC(disable_sensor_for_ros) << [this](carla::streaming::detail::stream_id_type sensor_id) ->
								   R<void>
	{
		REQUIRE_CARLA_EPISODE();
		bool ForceInPrimary = false;

		// check for the world observer (always in primary server)
		if (sensor_id == 1)
		{
			ForceInPrimary = true;
		}

		// collision sensor always in primary server in multi-gpu
		FString Desc = Episode->GetActorDescriptionFromStream(sensor_id);
		if (Desc == "" || Desc == "sensor.other.collision")
		{
			ForceInPrimary = true;
		}

		if (SecondaryServer->HasClientsConnected() && !ForceInPrimary)
		{
			// multi-gpu
			SecondaryServer->GetCommander().DisableForROS(sensor_id);
		}
		else
		{
			// single-gpu
			StreamingServer.DisableForROS(sensor_id);
		}
		return R<void>::Success();
	};

	BIND_SYNC(is_sensor_enabled_for_ros) << [this](carla::streaming::detail::stream_id_type sensor_id) ->
									 R<bool>
	{
		REQUIRE_CARLA_EPISODE();
		bool ForceInPrimary = false;

		// check for the world observer (always in primary server)
		if (sensor_id == 1)
		{
			ForceInPrimary = true;
		}

		// collision sensor always in primary server in multi-gpu
		FString Desc = Episode->GetActorDescriptionFromStream(sensor_id);
		if (Desc == "" || Desc == "sensor.other.collision")
		{
			ForceInPrimary = true;
		}

		if (SecondaryServer->HasClientsConnected() && !ForceInPrimary)
		{
			// multi-gpu
			return SecondaryServer->GetCommander().IsEnabledForROS(sensor_id);
		}
		else
		{
			// single-gpu
			return StreamingServer.IsEnabledForROS(sensor_id);
		}
	};

	// ~~ Main camera control (JSON based) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	BIND_SYNC(control_main_camera) << [this](std::string json_str) -> R<std::string>
	{
		CARLA_ENSURE_GAME_THREAD();
		if (Episode == nullptr)
		{
			return MakeJsonResponse(false, TEXT("episode not ready"));
		}

		UWorld* World = Episode->GetWorld();
		FString JsonString = UTF8_TO_TCHAR(json_str.c_str());
		
		// 仅解析 JSON 以获取 robot_id（如果存在）
		AActor* TargetActor = nullptr;
		FString ErrorMessage;
		TSharedPtr<FJsonObject> RootObject;
		if (ParseJsonString(json_str, RootObject, ErrorMessage))
		{
			double RobotIdValue = 0.0;
			if (RootObject->TryGetNumberField(TEXT("robot_id"), RobotIdValue))
			{
				const cr::ActorId RobotId = static_cast<cr::ActorId>(RobotIdValue);
				FCarlaActor* CarlaActor = Episode->FindCarlaActor(RobotId);
				if (CarlaActor != nullptr)
				{
					TargetActor = CarlaActor->GetActor();
				}
			}
		}
		
		// 获取相机子系统，将 JSON 字符串和 Actor 一起传入
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (USevnceCameraSubsystem* CameraSubsystem = GameInstance->GetSubsystem<USevnceCameraSubsystem>())
			{
				FString ResponseString = CameraSubsystem->ControlMainCameraFromJson(JsonString, TargetActor);
				return std::string(TCHAR_TO_UTF8(*ResponseString));
			}
		}
		return MakeJsonResponse(false, TEXT("SevnceCameraSubsystem not found"));
	};

	// ~~ Actor physics ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	BIND_SYNC(set_actor_location) << [this](
		cr::ActorId ActorId,
		cr::Location Location) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_actor_location",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		CarlaActor->SetActorGlobalLocation(
			Location, ETeleportType::TeleportPhysics);
		return R<void>::Success();
	};

	BIND_SYNC(set_actor_transform) << [this](
		cr::ActorId ActorId,
		cr::Transform Transform) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_actor_transform",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		CarlaActor->SetActorGlobalTransform(
			Transform, ETeleportType::TeleportPhysics);
		return R<void>::Success();
	};

	BIND_SYNC(set_sensor_fov) << [this](cr::ActorId ActorId, float FOV) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
			return RespondError(
				"set_sensor_fov",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));

		ASceneCaptureSensor* Camera = Cast<ASceneCaptureSensor>(CarlaActor->GetActor());
		if (!Camera)
			return RespondError(
				"set_sensor_fov",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		std::cout << "server side : receive fov : " << FOV << std::endl;
		Camera->SetFOVAngle(FOV);
		return R<void>::Success();
	};

	BIND_SYNC(get_sensor_fov) << [this](cr::ActorId ActorId) -> R<float>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
			return RespondError(
				"get_fov",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));

		ASceneCaptureSensor* Camera = Cast<ASceneCaptureSensor>(CarlaActor->GetActor());
		if (!Camera)
			return RespondError(
				"get_fov",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		
		float fov = Camera->GetFOVAngle();
		return fov;
	};
	
	BIND_SYNC(set_walker_state) << [this] (
		cr::ActorId ActorId,
		cr::Transform Transform,
		float Speed) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_walker_state",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		// apply walker transform
		ECarlaServerResponse Response =
			CarlaActor->SetWalkerState(
				Transform,
				cr::WalkerControl(
				  Transform.GetForwardVector(), Speed, false));
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_walker_state",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(set_actor_target_velocity) << [this](
		cr::ActorId ActorId,
		cr::Vector3D vector) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_actor_target_velocity",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->SetActorTargetVelocity(vector.ToCentimeters().ToFVector());
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_actor_target_velocity",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(set_actor_target_angular_velocity) << [this](
		cr::ActorId ActorId,
		cr::Vector3D vector) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_actor_target_angular_velocity",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->SetActorTargetAngularVelocity(vector.ToFVector());
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_actor_target_angular_velocity",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(enable_actor_constant_velocity) << [this](
		cr::ActorId ActorId,
		cr::Vector3D vector) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"enable_actor_constant_velocity",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		ECarlaServerResponse Response =
			CarlaActor->EnableActorConstantVelocity(vector.ToCentimeters().ToFVector());
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"enable_actor_constant_velocity",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		return R<void>::Success();
	};

	BIND_SYNC(disable_actor_constant_velocity) << [this](
		cr::ActorId ActorId) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"disable_actor_constant_velocity",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		ECarlaServerResponse Response =
			CarlaActor->DisableActorConstantVelocity();
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"disable_actor_constant_velocity",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		return R<void>::Success();
	};

	BIND_SYNC(add_actor_impulse) << [this](
		cr::ActorId ActorId,
		cr::Vector3D vector) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"add_actor_impulse",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		ECarlaServerResponse Response =
			CarlaActor->AddActorImpulse(vector.ToCentimeters().ToFVector());
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"add_actor_impulse",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(add_actor_impulse_at_location) << [this](
		cr::ActorId ActorId,
		cr::Vector3D impulse,
		cr::Vector3D location) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"add_actor_impulse_at_location",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		FVector UELocation = location.ToCentimeters().ToFVector();
		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		ALargeMapManager* LargeMap = GameMode->GetLMManager();
		if (LargeMap)
		{
			UELocation = LargeMap->GlobalToLocalLocation(UELocation);
		}
		ECarlaServerResponse Response =
			CarlaActor->AddActorImpulseAtLocation(impulse.ToCentimeters().ToFVector(), UELocation);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"add_actor_impulse_at_location",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		return R<void>::Success();
	};

	BIND_SYNC(add_actor_force) << [this](
		cr::ActorId ActorId,
		cr::Vector3D vector) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"add_actor_force",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->AddActorForce(vector.ToCentimeters().ToFVector());
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"add_actor_force",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(add_actor_force_at_location) << [this](
		cr::ActorId ActorId,
		cr::Vector3D force,
		cr::Vector3D location) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"add_actor_force_at_location",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		FVector UELocation = location.ToCentimeters().ToFVector();
		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		ALargeMapManager* LargeMap = GameMode->GetLMManager();
		if (LargeMap)
		{
			UELocation = LargeMap->GlobalToLocalLocation(UELocation);
		}
		ECarlaServerResponse Response =
			CarlaActor->AddActorForceAtLocation(UELocation, force.ToCentimeters().ToFVector());
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"add_actor_force_at_location",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(add_actor_angular_impulse) << [this](
		cr::ActorId ActorId,
		cr::Vector3D vector) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"add_actor_angular_impulse",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->AddActorAngularImpulse(vector.ToFVector());
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"add_actor_angular_impulse",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(add_actor_torque) << [this](
		cr::ActorId ActorId,
		cr::Vector3D vector) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"add_actor_torque",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->AddActorTorque(vector.ToFVector());
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"add_actor_torque",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(get_physics_control) << [this](
		cr::ActorId ActorId) -> R<cr::VehiclePhysicsControl>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"get_physics_control",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		FVehiclePhysicsControl PhysicsControl;
		ECarlaServerResponse Response =
			CarlaActor->GetPhysicsControl(PhysicsControl);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"get_physics_control",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return cr::VehiclePhysicsControl::FromFVehiclePhysicsControl(PhysicsControl);
	};

	BIND_SYNC(get_vehicle_light_state) << [this](
		cr::ActorId ActorId) -> R<cr::VehicleLightState>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"get_vehicle_light_state",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		FVehicleLightState LightState;
		ECarlaServerResponse Response =
			CarlaActor->GetVehicleLightState(LightState);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"get_vehicle_light_state",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return cr::VehicleLightState(LightState);
	};

	BIND_SYNC(apply_physics_control) << [this](
		cr::ActorId ActorId,
		cr::VehiclePhysicsControl PhysicsControl) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"apply_physics_control",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->ApplyPhysicsControl(FVehiclePhysicsControl(PhysicsControl));
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"apply_physics_control",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(set_vehicle_light_state) << [this](
		cr::ActorId ActorId,
		cr::VehicleLightState LightState) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_vehicle_light_state",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->SetVehicleLightState(FVehicleLightState(LightState));
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_vehicle_light_state",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};


	BIND_SYNC(open_vehicle_door) << [this](
		cr::ActorId ActorId,
		cr::VehicleDoor DoorIdx) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"open_vehicle_door",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->OpenVehicleDoor(static_cast<EVehicleDoor>(DoorIdx));
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"open_vehicle_door",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(close_vehicle_door) << [this](
		cr::ActorId ActorId,
		cr::VehicleDoor DoorIdx) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"close_vehicle_door",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->CloseVehicleDoor(static_cast<EVehicleDoor>(DoorIdx));
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"close_vehicle_door",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(set_wheel_steer_direction) << [this](
	  cr::ActorId ActorId,
	  cr::VehicleWheelLocation WheelLocation,
	  float AngleInDeg) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if(!CarlaActor){
			return RespondError(
				"set_wheel_steer_direction",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->SetWheelSteerDirection(
				static_cast<EVehicleWheelLocation>(WheelLocation), AngleInDeg);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_wheel_steer_direction",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(get_wheel_steer_angle) << [this](
		const cr::ActorId ActorId,
		cr::VehicleWheelLocation WheelLocation) -> R<float>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if(!CarlaActor){
			return RespondError(
				"get_wheel_steer_angle",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		float Angle;
		ECarlaServerResponse Response =
			CarlaActor->GetWheelSteerAngle(
				static_cast<EVehicleWheelLocation>(WheelLocation), Angle);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"get_wheel_steer_angle",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return Angle;
	};

	BIND_SYNC(set_actor_simulate_physics) << [this](
		cr::ActorId ActorId,
		bool bEnabled) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_actor_simulate_physics",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->SetActorSimulatePhysics(bEnabled);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_actor_simulate_physics",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(set_actor_collisions) << [this](
		cr::ActorId ActorId,
		bool bEnabled) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_actor_collisions",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->SetActorCollisions(bEnabled);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_actor_collisions",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(set_actor_dead) << [this](
		cr::ActorId ActorId) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_actor_dead",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->SetActorDead();
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_actor_dead",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(set_actor_enable_gravity) << [this](
		cr::ActorId ActorId,
		bool bEnabled) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_actor_enable_gravity",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->SetActorEnableGravity(bEnabled);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_actor_enable_gravity",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(get_vehicle_bone_world_transforms) << [this](
		cr::ActorId ActorId) -> R<std::vector<cr::Transform>>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"get_vehicle_bone_world_transforms",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		else
		{
			ACarlaWheeledVehicle* CarlaVehicle = Cast<ACarlaWheeledVehicle>(CarlaActor->GetActor());
			return MakeVectorFromTArray<cr::Transform>(CarlaVehicle->GetWorldTransformedPose().LocalTransforms);
		}
	};

	// ~~ Apply control ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	BIND_SYNC(toggle_spray) << [this](cr::ActorId ActorId, const std::string& json_params) -> R<std::string>
	{
		REQUIRE_CARLA_EPISODE();
		
		// 解析JSON参数
		TSharedPtr<FJsonObject> JsonObject;
		FString ErrorMessage;
		if (!ParseJsonString(json_params, JsonObject, ErrorMessage))
		{
			return MakeJsonResponse(false, TEXT("Failed to parse JSON: ") + ErrorMessage);
		}

		// 获取CarlaActor
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return MakeJsonResponse(false, TEXT("Actor not found: ") + FString::FromInt(ActorId));
		}

		// 获取Actor
		AActor* Actor = CarlaActor->GetActor();
		if (!Actor)
		{
			return MakeJsonResponse(false, TEXT("Invalid Actor pointer for Id: ") + FString::FromInt(ActorId));
		}

		// 查找Niagara组件
		UNiagaraComponent* NiagaraComp = Actor->FindComponentByClass<UNiagaraComponent>();
		if (!NiagaraComp)
		{
			return MakeJsonResponse(false, TEXT("Actor does not have Niagara component"));
		}

		// 检查是否是NS_Sprayer_Frost资产
		FString AssetName = NiagaraComp->GetAsset()->GetName();
		if (!AssetName.Contains(TEXT("NS_Sprayer_Frost")))
		{
			UE_LOG(LogTemp, Warning, TEXT("Found Niagara component: %s"), *AssetName);
			return MakeJsonResponse(false, TEXT("Actor does not have NS_Sprayer_Frost asset"));
		}

		// 解析Distance参数
		double Distance = 0.0;
		if (JsonObject->HasField(TEXT("Distance")))
		{
			if (!JsonObject->TryGetNumberField(TEXT("Distance"), Distance))
			{
				return MakeJsonResponse(false, TEXT("Invalid Distance parameter"));
			}
		}

		// 解析Transform参数（可选）
		FTransform NewTransform = Actor->GetActorTransform();
		if (JsonObject->HasField(TEXT("transform")))
		{
			const TSharedPtr<FJsonObject>* TransformObj;
			if (!JsonObject->TryGetObjectField(TEXT("transform"), TransformObj))
			{
				return MakeJsonResponse(false, TEXT("Invalid transform parameter"));
			}

			// 解析location
			const TSharedPtr<FJsonObject>* LocObj;
			if ((*TransformObj)->TryGetObjectField(TEXT("location"), LocObj))
			{
				FVector Location = NewTransform.GetLocation();
				double x, y, z;
				if ((*LocObj)->TryGetNumberField(TEXT("x"), x)) Location.X = x * 100.0;
				if ((*LocObj)->TryGetNumberField(TEXT("y"), y)) Location.Y = y * 100.0;
				if ((*LocObj)->TryGetNumberField(TEXT("z"), z)) Location.Z = z * 100.0;
				NewTransform.SetLocation(Location);
			}

			// 解析rotation
			const TSharedPtr<FJsonObject>* RotObj;
			if ((*TransformObj)->TryGetObjectField(TEXT("rotation"), RotObj))
			{
				FRotator Rotation = NewTransform.Rotator();
				double pitch, yaw, roll;
				if ((*RotObj)->TryGetNumberField(TEXT("pitch"), pitch)) Rotation.Pitch = pitch;
				if ((*RotObj)->TryGetNumberField(TEXT("yaw"), yaw)) Rotation.Yaw = yaw;
				if ((*RotObj)->TryGetNumberField(TEXT("roll"), roll)) Rotation.Roll = roll;
				NewTransform.SetRotation(Rotation.Quaternion());
			}

			// 解析scale
			const TSharedPtr<FJsonObject>* ScaleObj;
			if ((*TransformObj)->TryGetObjectField(TEXT("scale"), ScaleObj))
			{
				FVector Scale = NewTransform.GetScale3D();
				double x, y, z;
				if ((*ScaleObj)->TryGetNumberField(TEXT("x"), x)) Scale.X = x;
				if ((*ScaleObj)->TryGetNumberField(TEXT("y"), y)) Scale.Y = y;
				if ((*ScaleObj)->TryGetNumberField(TEXT("z"), z)) Scale.Z = z;
				NewTransform.SetScale3D(Scale);
			}

			// 应用新的transform
			NiagaraComp->SetRelativeTransform(NewTransform);
		}

		// 控制Niagara系统
		FString ResultMessage;
		if (Distance > 0.0)
		{
			// 激活喷射
			NiagaraComp->Activate();
			NiagaraComp->SetFloatParameter(TEXT("Distance"), Distance * 100);
			ResultMessage = FString::Printf(TEXT("Spray activated with Distance: %f"), Distance);
			UE_LOG(LogTemp, Warning, TEXT("Activated spray with Distance: %f for Actor %d"), Distance, ActorId);
		}
		else
		{
			// 关闭喷射
			NiagaraComp->Deactivate();
			NiagaraComp->SetFloatParameter(TEXT("Distance"), 0.0);
			ResultMessage = TEXT("Spray deactivated");
			UE_LOG(LogTemp, Warning, TEXT("Deactivated spray for Actor %d"), ActorId);
		}

		return MakeJsonResponse(true, ResultMessage, [](TSharedPtr<FJsonObject> JsonResponse)
		{
		});
	};
	BIND_SYNC(get_robot_bones_transform) << [this](cr::ActorId ActorId) -> R<cr::RobotBoneControlOut>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
					"get_robot_bones_transform",
					ECarlaServerResponse::ActorNotFound,
					" Actor Id: " + FString::FromInt(ActorId));
		}

		// 尝试转换为机器人基类
		auto* robot = Cast<ACarlaWheeledVehicle>(CarlaActor->GetActor());
		if (!robot)
		{
			return RespondError(
				"set_robot_bones_transform",
				ECarlaServerResponse::Failure,
				" Not a supported robot actor. Actor Id: " + FString::FromInt(ActorId));
		}
	    
		// 获取骨骼网格组件
		USkeletalMeshComponent* skmComp = robot->GetMesh();
		if (!skmComp)
		{
			return RespondError(
				"set_robot_bones_transform",
				ECarlaServerResponse::Failure,
				" Can't find Skeletal Mesh Component. Actor Id: " + FString::FromInt(ActorId));
		}
	    
		// 获取动画实例
		UAnimInstance* animInst = skmComp->GetAnimInstance();
		if (!animInst)
		{
			return RespondError(
				"set_robot_bones_transform",
				ECarlaServerResponse::Failure,
				" Failed to find Anim Instance. Actor Id: " + FString::FromInt(ActorId));
		}
		
		auto* robotAnimInst = Cast<UWheeledRobotAnimationInstance>(animInst);
		if (!robotAnimInst)
			return RespondError(
				"set_robot_bones_transform",
				ECarlaServerResponse::Failure,
				" Failed to find Anim Instance. Actor Id: " + FString::FromInt(ActorId));
		
		FRobotBoneControlOut bones;
		FPoseSnapshot tempSnapShot;
		skmComp->SnapshotPose(tempSnapShot);
		
		for (int i=0; i<tempSnapShot.BoneNames.Num(); ++i)
		{
			FRobotBoneControlOutData Transforms;
			Transforms.World = skmComp->GetSocketTransform(tempSnapShot.BoneNames[i], ERelativeTransformSpace::RTS_World);
			Transforms.Component = skmComp->GetSocketTransform(tempSnapShot.BoneNames[i], ERelativeTransformSpace::RTS_Actor);
			Transforms.Relative = skmComp->GetSocketTransform(tempSnapShot.BoneNames[i], ERelativeTransformSpace::RTS_ParentBoneSpace);
			bones.BoneTransforms.Add(tempSnapShot.BoneNames[i].ToString(), Transforms);
		}
		
		std::vector<carla::rpc::BoneTransformDataOut> boneData;
		for (auto Bone : bones.BoneTransforms)
		{
			carla::rpc::BoneTransformDataOut Data;
			Data.bone_name = std::string(TCHAR_TO_UTF8(*Bone.Get<0>()));
			FRobotBoneControlOutData transform = Bone.Get<1>();
			Data.world = transform.World;
			Data.component = transform.Component;
			Data.relative = transform.Relative;
			boneData.push_back(Data);
		}
		return carla::rpc::RobotBoneControlOut(boneData);
	};

	BIND_SYNC(set_robot_bones_transform) << [this](cr::ActorId ActorId, cr::RobotBoneControlIn bones) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
					"set_robot_bones_transform",
					ECarlaServerResponse::ActorNotFound,
					" Actor Id: " + FString::FromInt(ActorId));
		}

		FRobotBoneControlIn robotBones = FRobotBoneControlIn(bones);

		AActor* actor = CarlaActor->GetActor();
	    
		// 尝试转换为机器人基类
		auto* robot = Cast<ACarlaWheeledVehicle>(actor);
		if (!robot)
		{
			return RespondError(
				"set_robot_bones_transform",
				ECarlaServerResponse::Failure,
				" Not a supported robot actor. Actor Id: " + FString::FromInt(ActorId));
		}
	    
		// 获取骨骼网格组件
		USkeletalMeshComponent* skmComp = robot->GetMesh();
		if (!skmComp)
		{
			return RespondError(
				"set_robot_bones_transform",
				ECarlaServerResponse::Failure,
				" Can't find Skeletal Mesh Component. Actor Id: " + FString::FromInt(ActorId));
		}
	    
		// 获取动画实例
		UAnimInstance* animInst = skmComp->GetAnimInstance();
		if (!animInst)
		{
			return RespondError(
				"set_robot_bones_transform",
				ECarlaServerResponse::Failure,
				" Failed to find Anim Instance. Actor Id: " + FString::FromInt(ActorId));
		}
		
		
		auto* robotAnimInst = Cast<UWheeledRobotAnimationInstance>(animInst);
		if (!robotAnimInst)
			return RespondError(
				"set_robot_bones_transform",
				ECarlaServerResponse::Failure,
				" Failed to find Anim Instance. Actor Id: " + FString::FromInt(ActorId));

		// 将 RPC 接收到的骨骼数据转换为 TMap<FName, FTransform>
		TMap<FName, FTransform> boneMap;
		for (const auto& BoneData : bones.bone_transforms)
		{
			boneMap.Add(FName(BoneData.first.c_str()), BoneData.second);
		}

		// 调用动画实例接口应用骨骼变换
		robotAnimInst->SetBonesTransform(boneMap);
		
		// robotAnimInst->bUseSnapshot = true;
		// // 拿快照
		// if (robotAnimInst->Snap.BoneNames.Num() == 0)
		// {
		// 	skmComp->SnapshotPose(robotAnimInst->Snap);
		// }
		//
		// TMap<FName, FTransform> inputBonesMap;
		// for (const TPair<FString, FTransform> &pair : robotBones.BoneTransforms)
		// {
		// 	FName BoneName = FName(*pair.Key);
		// 	inputBonesMap.Add(BoneName, pair.Value);
		// }
		//
		// int loopCount = robotAnimInst->Snap.BoneNames.Num();
		// for (int i = loopCount - 1; i >= 0; --i)
		// {
		// 	if (FTransform *trans = inputBonesMap.Find(robotAnimInst->Snap.BoneNames[i]))
		// 	{
		// 		robotAnimInst->Snap.LocalTransforms[i] = *trans;
		// 	}
		// 	else
		// 	{
		// 		robotAnimInst->Snap.BoneNames.RemoveAt(i);
		// 		robotAnimInst->Snap.LocalTransforms.RemoveAt(i);
		// 	}
		// }
		
		return R<void>::Success();
	};

	BIND_SYNC(apply_control_to_vehicle) << [this](
		cr::ActorId ActorId,
		cr::VehicleControl Control) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
    
		ECarlaServerResponse Response = SvcRobotLogic::ApplyControlToRobot(Episode, ActorId, Control);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"apply_control_to_robot",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(apply_ackermann_control_to_vehicle) << [this](
		cr::ActorId ActorId,
		cr::VehicleAckermannControl Control) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"apply_ackermann_control_to_vehicle",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->ApplyAckermannControlToVehicle(Control, EVehicleInputPriority::Client);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"apply_ackermann_control_to_vehicle",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(get_ackermann_controller_settings) << [this](
		cr::ActorId ActorId) -> R<cr::AckermannControllerSettings>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"get_ackermann_controller_settings",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		FAckermannControllerSettings Settings;
		ECarlaServerResponse Response =
			CarlaActor->GetAckermannControllerSettings(Settings);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"get_ackermann_controller_settings",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return cr::AckermannControllerSettings(Settings);
	};

	BIND_SYNC(apply_ackermann_controller_settings) << [this](
		cr::ActorId ActorId,
		cr::AckermannControllerSettings AckermannSettings) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"apply_ackermann_controller_settings",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->ApplyAckermannControllerSettings(FAckermannControllerSettings(AckermannSettings));
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"apply_ackermann_controller_settings",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(apply_control_to_walker) << [this](
		cr::ActorId ActorId,
		cr::WalkerControl Control) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"apply_control_to_walker",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->ApplyControlToWalker(Control);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"apply_control_to_walker",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(get_bones_transform) << [this](
		cr::ActorId ActorId) -> R<cr::WalkerBoneControlOut>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"get_bones_transform",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		FWalkerBoneControlOut Bones;
		ECarlaServerResponse Response =
			CarlaActor->GetBonesTransform(Bones);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"get_bones_transform",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		std::vector<carla::rpc::BoneTransformDataOut> BoneData;
		for (auto Bone : Bones.BoneTransforms)
		{
			carla::rpc::BoneTransformDataOut Data;
			Data.bone_name = std::string(TCHAR_TO_UTF8(*Bone.Get<0>()));
			FWalkerBoneControlOutData Transforms = Bone.Get<1>();
			Data.world = Transforms.World;
			Data.component = Transforms.Component;
			Data.relative = Transforms.Relative;
			BoneData.push_back(Data);
		}
		return carla::rpc::WalkerBoneControlOut(BoneData);
	};

	BIND_SYNC(set_bones_transform) << [this](
		cr::ActorId ActorId,
		carla::rpc::WalkerBoneControlIn Bones) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_bones_transform",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		FWalkerBoneControlIn Bones2 = FWalkerBoneControlIn(Bones);
		ECarlaServerResponse Response = CarlaActor->SetBonesTransform(Bones2);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_bones_transform",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		return R<void>::Success();
	};

	BIND_SYNC(blend_pose) << [this](
		cr::ActorId ActorId,
		float Blend) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"blend_pose",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		ECarlaServerResponse Response = CarlaActor->BlendPose(Blend);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"blend_pose",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		return R<void>::Success();
	};

	BIND_SYNC(get_pose_from_animation) << [this](
		cr::ActorId ActorId) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"get_pose_from_animation",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		ECarlaServerResponse Response = CarlaActor->GetPoseFromAnimation();
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"get_pose_from_animation",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}

		return R<void>::Success();
	};

	BIND_SYNC(set_actor_autopilot) << [this](
		cr::ActorId ActorId,
		bool bEnabled) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_actor_autopilot",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->SetActorAutopilot(bEnabled);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_actor_autopilot",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(show_vehicle_debug_telemetry) << [this](
		cr::ActorId ActorId,
		bool bEnabled) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"show_vehicle_debug_telemetry",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->ShowVehicleDebugTelemetry(bEnabled);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"show_vehicle_debug_telemetry",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(enable_carsim) << [this](
		cr::ActorId ActorId,
		std::string SimfilePath) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"enable_carsim",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->EnableCarSim(carla::rpc::ToFString(SimfilePath));
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"enable_carsim",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(use_carsim_road) << [this](
		cr::ActorId ActorId,
		bool bEnabled) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"use_carsim_road",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->UseCarSimRoad(bEnabled);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"use_carsim_road",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(enable_chrono_physics) << [this](
		cr::ActorId ActorId,
		uint64_t MaxSubsteps,
		float MaxSubstepDeltaTime,
		std::string VehicleJSON,
		std::string PowertrainJSON,
		std::string TireJSON,
		std::string BaseJSONPath) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"enable_chrono_physics",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->EnableChronoPhysics(
				MaxSubsteps, MaxSubstepDeltaTime,
				cr::ToFString(VehicleJSON),
				cr::ToFString(PowertrainJSON),
				cr::ToFString(TireJSON),
				cr::ToFString(BaseJSONPath));
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"enable_chrono_physics",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	// ~~ Traffic lights ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	BIND_SYNC(set_traffic_light_state) << [this](
		cr::ActorId ActorId,
		cr::TrafficLightState trafficLightState) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_traffic_light_state",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->SetTrafficLightState(
			static_cast<ETrafficLightState>(trafficLightState));
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_traffic_light_state",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(set_traffic_light_green_time) << [this](
		cr::ActorId ActorId,
		float GreenTime) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_traffic_light_green_time",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->SetLightGreenTime(GreenTime);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_traffic_light_green_time",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(set_traffic_light_yellow_time) << [this](
		cr::ActorId ActorId,
		float YellowTime) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_traffic_light_yellow_time",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->SetLightYellowTime(YellowTime);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_traffic_light_yellow_time",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(set_traffic_light_red_time) << [this](
		cr::ActorId ActorId,
		float RedTime) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"set_traffic_light_red_time",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->SetLightRedTime(RedTime);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"set_traffic_light_red_time",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(freeze_traffic_light) << [this](
		cr::ActorId ActorId,
		bool Freeze) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"freeze_traffic_light",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->FreezeTrafficLight(Freeze);
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"freeze_traffic_light",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(reset_traffic_light_group) << [this](
		cr::ActorId ActorId) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"reset_traffic_light_group",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ECarlaServerResponse Response =
			CarlaActor->ResetTrafficLightGroup();
		if (Response != ECarlaServerResponse::Success)
		{
			return RespondError(
				"reset_traffic_light_group",
				Response,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		return R<void>::Success();
	};

	BIND_SYNC(reset_all_traffic_lights) << [this]() -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		for (TActorIterator<ATrafficLightGroup> It(Episode->GetWorld()); It; ++It)
		{
			It->ResetGroup();
		}
		return R<void>::Success();
	};

	BIND_SYNC(freeze_all_traffic_lights) << [this]
		(bool frozen) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		auto* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		if (!GameMode)
		{
			RESPOND_ERROR("unable to find CARLA game mode");
		}
		auto* TraffiLightManager = GameMode->GetTrafficLightManager();
		TraffiLightManager->SetFrozen(frozen);
		return R<void>::Success();
	};

	BIND_SYNC(get_vehicle_light_states) << [this]() -> R<cr::VehicleLightStateList>
	{
		REQUIRE_CARLA_EPISODE();
		cr::VehicleLightStateList List;

		auto It = Episode->GetActorRegistry().begin();
		for (; It != Episode->GetActorRegistry().end(); ++It)
		{
			const FCarlaActor& View = *(It.Value().Get());
			if (View.GetActorType() == FCarlaActor::ActorType::Vehicle)
			{
				if(View.IsDormant())
				{
					// todo: implement
				}
				else
				{
					auto Actor = View.GetActor();
					if (IsValid(Actor))
					{
						const ACarlaWheeledVehicle *Vehicle = Cast<ACarlaWheeledVehicle>(Actor);
						List.emplace_back(
							View.GetActorId(),
							cr::VehicleLightState(Vehicle->GetVehicleLightState()).GetLightStateAsValue());
					}
				}
			}
		}
		return List;
	};

	BIND_SYNC(get_group_traffic_lights) << [this](
		const cr::ActorId ActorId) -> R<std::vector<cr::ActorId>>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			RESPOND_ERROR("unable to get group traffic lights: actor not found");
		}
		if (CarlaActor->IsDormant())
		{
			//todo implement
			return std::vector<cr::ActorId>();
		}
		else
		{
			auto TrafficLight = Cast<ATrafficLightBase>(CarlaActor->GetActor());
			if (TrafficLight == nullptr)
			{
				RESPOND_ERROR("unable to get group traffic lights: actor is not a traffic light");
			}
			std::vector<cr::ActorId> Result;
			for (auto* TLight : TrafficLight->GetGroupTrafficLights())
			{
				auto* View = Episode->FindCarlaActor(TLight);
				if (View)
				{
					Result.push_back(View->GetActorId());
				}
			}
			return Result;
		}
	};



	BIND_SYNC(get_navigable_area_points) << [this](const cr::ActorId ActorId, float dist /* 网格间距(cm) */)
		-> R<std::vector<carla::geom::Location>>
	{
		// 获取 Actor (TODO：根据传入actor包围盒调整区域）
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor || !CarlaActor->GetActor()) {
			return RespondError(
				"get_navigable_area_points",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		AActor* Actor = CarlaActor->GetActor();

		// 获取导航系统
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Actor->GetWorld());
		if (!NavSys) {
			return RespondError(
				"get_navigable_area_points",
				ECarlaServerResponse::Failure,
				" NavigationSystem not found " + FString::FromInt(ActorId));
		}

		// 获取 NavMesh 数据
		ARecastNavMesh* RecastNavMesh = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance());
		if (!RecastNavMesh) {
			return RespondError(
				"get_navigable_area_points",
				ECarlaServerResponse::Failure,
				" RecastMesh not found " + FString::FromInt(ActorId));
		}

		std::vector<carla::geom::Location> Result;
		
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dis(0.0f, dist);
		float offsetY = dis(gen);  // Y方向随机偏移0~dist

		FBox NavBounds = RecastNavMesh->GetBounds();
		for (float X = NavBounds.Min.X; X <= NavBounds.Max.X; X += dist) {
			for (float Y = NavBounds.Min.Y + offsetY; Y <= NavBounds.Max.Y; Y += dist) {
				FVector TestPoint(X, Y, NavBounds.Min.Z);

				FNavLocation OutNavLoc;
				bool bOnNav = NavSys->ProjectPointToNavigation(
					TestPoint,
					OutNavLoc,
					FVector(dist * 0.5f, dist * 0.5f, 500.0f)
				);

				if (bOnNav) {
					Result.emplace_back(
						OutNavLoc.Location.X / 100.0f, // x左为正 y上为正
						OutNavLoc.Location.Y / 100.0f,
						OutNavLoc.Location.Z / 100.0f
					);
				}
			}
		}
		
		return R<std::vector<carla::geom::Location>>(std::move(Result));
	};

	// 表计数据
	BIND_SYNC(get_gauges_transform) << [this]() -> R<std::vector<carla::geom::Transform>>
	{
		AActor* actor = UGameplayStatics::GetActorOfClass(Episode->GetWorld(), AGaugesManagerActor::StaticClass());
		
		AGaugesManagerActor* gaugesManager = Cast<AGaugesManagerActor>(actor);
		if (!gaugesManager)
			return RespondError(
				"get_gauges_transform",
				ECarlaServerResponse::Failure,
				" gaugesManager Actor Not Found!");

		std::vector<carla::geom::Transform> result;
		TArray<FTransform> gaugesTransforms = gaugesManager->GetGaugesTransform();
		for (const auto& transform : gaugesTransforms)
		{
			carla::geom::Transform carlaTrans;
        
			// 转换位置（从厘米到米）
			FVector tempLoc = transform.GetLocation();
			carlaTrans.location = {
				static_cast<float>(tempLoc.X) / 100.f,
				static_cast<float>(tempLoc.Y) / 100.f,
				static_cast<float>(tempLoc.Z) / 100.f
			};
        
			// 转换旋转
			FRotator rotator = transform.GetRotation().Rotator();
			carlaTrans.rotation = {
				static_cast<float>(rotator.Pitch),
				static_cast<float>(rotator.Yaw),
				static_cast<float>(rotator.Roll)
			};
			
			result.push_back(carlaTrans);
		}
		return R<std::vector<carla::geom::Transform>>(std::move(result));
	};
	
	BIND_SYNC(get_light_boxes) << [this](
		const cr::ActorId ActorId) -> R<std::vector<cg::BoundingBox>>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if (!CarlaActor)
		{
			return RespondError(
				"get_light_boxes",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		if (CarlaActor->IsDormant())
		{
			return RespondError(
				"get_light_boxes",
				ECarlaServerResponse::FunctionNotAvailiableWhenDormant,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		else
		{
			ATrafficLightBase* TrafficLight = Cast<ATrafficLightBase>(CarlaActor->GetActor());
			if (!TrafficLight)
			{
				return RespondError(
				  "get_light_boxes",
				  ECarlaServerResponse::NotATrafficLight,
				  " Actor Id: " + FString::FromInt(ActorId));
			}
			TArray<FBoundingBox> Result;
			TArray<uint8> OutTag;
			UBoundingBoxCalculator::GetTrafficLightBoundingBox(
				TrafficLight, Result, OutTag,
				static_cast<uint8>(carla::rpc::CityObjectLabel::TrafficLight));
			return MakeVectorFromTArray<cg::BoundingBox>(Result);
		}
	};

	// ~~ GBuffer tokens ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	BIND_SYNC(get_gbuffer_token) << [this](const cr::ActorId ActorId, uint32_t GBufferId) -> R<std::vector<unsigned char>>
	{
		REQUIRE_CARLA_EPISODE();
		FCarlaActor* CarlaActor = Episode->FindCarlaActor(ActorId);
		if(!CarlaActor)
		{
			return RespondError(
				"get_gbuffer_token",
				ECarlaServerResponse::ActorNotFound,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		if (CarlaActor->IsDormant())
		{
			return RespondError(
				"get_gbuffer_token",
				ECarlaServerResponse::FunctionNotAvailiableWhenDormant,
				" Actor Id: " + FString::FromInt(ActorId));
		}
		ASceneCaptureSensor* Sensor = Cast<ASceneCaptureSensor>(CarlaActor->GetActor());
		if (!Sensor)
		{
			return RespondError(
			  "get_gbuffer_token",
			  ECarlaServerResponse::ActorTypeMismatch,
			  " Actor Id: " + FString::FromInt(ActorId));
		}

		switch (GBufferId)
		{
		case 0:
			{
				const auto &Token = Sensor->CameraGBuffers.SceneColor.GetToken();
				return std::vector<unsigned char>(std::begin(Token.data), std::end(Token.data));
			}
		case 1:
			{
				const auto &Token = Sensor->CameraGBuffers.SceneDepth.GetToken();
				return std::vector<unsigned char>(std::begin(Token.data), std::end(Token.data));
			}
		case 2:
			{
				const auto& Token = Sensor->CameraGBuffers.SceneStencil.GetToken();
				return std::vector<unsigned char>(std::begin(Token.data), std::end(Token.data));
			}
		case 3:
			{
				const auto &Token = Sensor->CameraGBuffers.GBufferA.GetToken();
				return std::vector<unsigned char>(std::begin(Token.data), std::end(Token.data));
			}
		case 4:
			{
				const auto &Token = Sensor->CameraGBuffers.GBufferB.GetToken();
				return std::vector<unsigned char>(std::begin(Token.data), std::end(Token.data));
			}
		case 5:
			{
				const auto &Token = Sensor->CameraGBuffers.GBufferC.GetToken();
				return std::vector<unsigned char>(std::begin(Token.data), std::end(Token.data));
			}
		case 6:
			{
				const auto &Token = Sensor->CameraGBuffers.GBufferD.GetToken();
				return std::vector<unsigned char>(std::begin(Token.data), std::end(Token.data));
			}
		case 7:
			{
				const auto &Token = Sensor->CameraGBuffers.GBufferE.GetToken();
				return std::vector<unsigned char>(std::begin(Token.data), std::end(Token.data));
			}
		case 8:
			{
				const auto &Token = Sensor->CameraGBuffers.GBufferF.GetToken();
				return std::vector<unsigned char>(std::begin(Token.data), std::end(Token.data));
			}
		case 9:
			{
				const auto &Token = Sensor->CameraGBuffers.Velocity.GetToken();
				return std::vector<unsigned char>(std::begin(Token.data), std::end(Token.data));
			}
		case 10:
			{
				const auto &Token = Sensor->CameraGBuffers.SSAO.GetToken();
				return std::vector<unsigned char>(std::begin(Token.data), std::end(Token.data));
			}
		case 11:
			{
				const auto& Token = Sensor->CameraGBuffers.CustomDepth.GetToken();
				return std::vector<unsigned char>(std::begin(Token.data), std::end(Token.data));
			}
		case 12:
			{
				const auto& Token = Sensor->CameraGBuffers.CustomStencil.GetToken();
				return std::vector<unsigned char>(std::begin(Token.data), std::end(Token.data));
			}
		default:
			UE_LOG(LogCarla, Error, TEXT("Requested invalid GBuffer ID %u"), GBufferId);
			return {};
		}
	};

	// ~~ Logging and playback ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	BIND_SYNC(start_recorder) << [this](std::string name, bool AdditionalData) -> R<std::string>
	{
		REQUIRE_CARLA_EPISODE();
		return R<std::string>(Episode->StartRecorder(name, AdditionalData));
	};

	BIND_SYNC(stop_recorder) << [this]() -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		Episode->GetRecorder()->Stop();
		return R<void>::Success();
	};

	BIND_SYNC(show_recorder_file_info) << [this](
		std::string name,
		bool show_all) -> R<std::string>
	{
		REQUIRE_CARLA_EPISODE();
		return R<std::string>(Episode->GetRecorder()->ShowFileInfo(
			name,
			show_all));
	};

	BIND_SYNC(show_recorder_collisions) << [this](
		std::string name,
		char type1,
		char type2) -> R<std::string>
	{
		REQUIRE_CARLA_EPISODE();
		return R<std::string>(Episode->GetRecorder()->ShowFileCollisions(
			name,
			type1,
			type2));
	};

	BIND_SYNC(show_recorder_actors_blocked) << [this](
		std::string name,
		double min_time,
		double min_distance) -> R<std::string>
	{
		REQUIRE_CARLA_EPISODE();
		return R<std::string>(Episode->GetRecorder()->ShowFileActorsBlocked(
			name,
			min_time,
			min_distance));
	};

	BIND_SYNC(replay_file) << [this](
		std::string name,
		double start,
		double duration,
		uint32_t follow_id,
		bool replay_sensors) -> R<std::string>
	{
		REQUIRE_CARLA_EPISODE();
		return R<std::string>(Episode->GetRecorder()->ReplayFile(
			name,
			start,
			duration,
			follow_id,
			replay_sensors));
	};

	BIND_SYNC(set_replayer_time_factor) << [this](double time_factor) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		Episode->GetRecorder()->SetReplayerTimeFactor(time_factor);
		return R<void>::Success();
	};

	BIND_SYNC(set_replayer_ignore_hero) << [this](bool ignore_hero) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		Episode->GetRecorder()->SetReplayerIgnoreHero(ignore_hero);
		return R<void>::Success();
	};

	BIND_SYNC(set_replayer_ignore_spectator) << [this](bool ignore_spectator) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		Episode->GetRecorder()->SetReplayerIgnoreSpectator(ignore_spectator);
		return R<void>::Success();
	};

	BIND_SYNC(stop_replayer) << [this](bool keep_actors) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		Episode->GetRecorder()->StopReplayer(keep_actors);
		return R<void>::Success();
	};

	// ~~ Draw debug shapes ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	BIND_SYNC(draw_debug_shape) << [this](const cr::DebugShape &shape) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		auto *World = Episode->GetWorld();
		check(World != nullptr);
		FDebugShapeDrawer Drawer(*World);
		Drawer.Draw(shape);
		return R<void>::Success();
	};

	// ~~ Apply commands in batch ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	using C = cr::Command;
	using CR = cr::CommandResponse;
	using ActorId = carla::ActorId;

	auto parse_result = [](ActorId id, const auto &response) {
		return response.HasError() ? CR{response.GetError()} : CR{id};
	};

#define MAKE_RESULT(operation) return parse_result(c.actor, operation);

	auto command_visitor = carla::Functional::MakeRecursiveOverload(
		[=](auto self, const C::SpawnActor &c) -> CR {
		  auto result = c.parent.has_value() ?
		  spawn_actor_with_parent(
			  c.description,
			  c.transform,
			  *c.parent,
			  cr::AttachmentType::Rigid) :
		  spawn_actor(c.description, c.transform);
		  if (!result.HasError())
		  {
			ActorId id = result.Get().id;
			auto set_id = carla::Functional::MakeOverload(
				[](C::SpawnActor &) {},
				[](C::ConsoleCommand &) {},
				[id](auto &s) { s.actor = id; });
			for (auto command : c.do_after)
			{
			  std::visit(set_id, command.command);
			  std::visit(self, command.command);
			}
			return id;
		  }
		  return result.GetError();
		},
		[=](auto, const C::DestroyActor &c) {         MAKE_RESULT(destroy_actor(c.actor)); },
		[=](auto, const C::ApplyVehicleControl &c) {  MAKE_RESULT(apply_control_to_vehicle(c.actor, c.control)); },
		[=](auto, const C::ApplyVehicleAckermannControl &c) {  MAKE_RESULT(apply_ackermann_control_to_vehicle(c.actor, c.control)); },
		[=](auto, const C::ApplyWalkerControl &c) {   MAKE_RESULT(apply_control_to_walker(c.actor, c.control)); },
		[=](auto, const C::ApplyVehiclePhysicsControl &c) {  MAKE_RESULT(apply_physics_control(c.actor, c.physics_control)); },
		[=](auto, const C::ApplyTransform &c) {       MAKE_RESULT(set_actor_transform(c.actor, c.transform)); },
		[=](auto, const C::ApplyTargetVelocity &c) {  MAKE_RESULT(set_actor_target_velocity(c.actor, c.velocity)); },
		[=](auto, const C::ApplyTargetAngularVelocity &c) { MAKE_RESULT(set_actor_target_angular_velocity(c.actor, c.angular_velocity)); },
		[=](auto, const C::ApplyImpulse &c) {         MAKE_RESULT(add_actor_impulse(c.actor, c.impulse)); },
		[=](auto, const C::ApplyForce &c) {           MAKE_RESULT(add_actor_force(c.actor, c.force)); },
		[=](auto, const C::ApplyAngularImpulse &c) {  MAKE_RESULT(add_actor_angular_impulse(c.actor, c.impulse)); },
		[=](auto, const C::ApplyTorque &c) {          MAKE_RESULT(add_actor_torque(c.actor, c.torque)); },
		[=](auto, const C::SetSimulatePhysics &c) {   MAKE_RESULT(set_actor_simulate_physics(c.actor, c.enabled)); },
		[=](auto, const C::SetEnableGravity &c) {   MAKE_RESULT(set_actor_enable_gravity(c.actor, c.enabled)); },
		// TODO: SetAutopilot should be removed. This is the old way to control the vehicles
		[=](auto, const C::SetAutopilot &c) {         MAKE_RESULT(set_actor_autopilot(c.actor, c.enabled)); },
		[=](auto, const C::ShowDebugTelemetry &c) {   MAKE_RESULT(show_vehicle_debug_telemetry(c.actor, c.enabled)); },
		[=](auto, const C::SetVehicleLightState &c) { MAKE_RESULT(set_vehicle_light_state(c.actor, c.light_state)); },
  //      [=](auto, const C::OpenVehicleDoor &c) {      MAKE_RESULT(open_vehicle_door(c.actor, c.door_idx)); },
  //      [=](auto, const C::CloseVehicleDoor &c) {     MAKE_RESULT(close_vehicle_door(c.actor, c.door_idx)); },
		[=](auto, const C::ApplyWalkerState &c) {     MAKE_RESULT(set_walker_state(c.actor, c.transform, c.speed)); },
		[=](auto, const C::ConsoleCommand& c) -> CR {       return console_command(c.cmd); },
		[=](auto, const C::SetTrafficLightState& c) { MAKE_RESULT(set_traffic_light_state(c.actor, c.traffic_light_state)); },
		[=](auto, const C::ApplyLocation& c)        { MAKE_RESULT(set_actor_location(c.actor, c.location)); }
	);

#undef MAKE_RESULT

	BIND_SYNC(apply_batch) << [=](
		const std::vector<cr::Command> &commands,
		bool do_tick_cue)
	{
		std::vector<CR> result;
		result.reserve(commands.size());
		for (const auto &command : commands)
		{
			result.emplace_back(std::visit(command_visitor, command.command));
		}
		if (do_tick_cue)
		{
			tick_cue();
		}
		return result;
	};

	// ~~ Light Subsystem ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	BIND_SYNC(query_lights_state) << [this](std::string client) -> R<std::vector<cr::LightState>>
	{
		REQUIRE_CARLA_EPISODE();
		std::vector<cr::LightState> result;
		auto *World = Episode->GetWorld();
		if(World) {
			UCarlaLightSubsystem* CarlaLightSubsystem = World->GetSubsystem<UCarlaLightSubsystem>();
			result = CarlaLightSubsystem->GetLights(FString(client.c_str()));
		}
		return result;
	};

	BIND_SYNC(update_lights_state) << [this]
	  (std::string client, const std::vector<cr::LightState>& lights, bool discard_client) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		auto *World = Episode->GetWorld();
		if(World) {
			UCarlaLightSubsystem* CarlaLightSubsystem = World->GetSubsystem<UCarlaLightSubsystem>();
			CarlaLightSubsystem->SetLights(FString(client.c_str()), lights, discard_client);
		}
		return R<void>::Success();
	};

	BIND_SYNC(update_day_night_cycle) << [this]
	  (std::string client, const bool active) -> R<void>
	{
		REQUIRE_CARLA_EPISODE();
		auto *World = Episode->GetWorld();
		if(World) {
			UCarlaLightSubsystem* CarlaLightSubsystem = World->GetSubsystem<UCarlaLightSubsystem>();
			CarlaLightSubsystem->SetDayNightCycle(active);
		}
		return R<void>::Success();
	};


	// ~~ Ray Casting ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	BIND_SYNC(project_point) << [this]
		(cr::Location Location, cr::Vector3D Direction, float SearchDistance)
		-> R<std::pair<bool,cr::LabelledPoint>>
	{
		REQUIRE_CARLA_EPISODE();
		auto *World = Episode->GetWorld();
		constexpr float meter_to_centimeter = 100.0f;
		FVector UELocation = Location;
		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		ALargeMapManager* LargeMap = GameMode->GetLMManager();
		if (LargeMap)
		{
			UELocation = LargeMap->GlobalToLocalLocation(UELocation);
		}
		return URayTracer::ProjectPoint(UELocation, Direction.ToFVector(),
			meter_to_centimeter * SearchDistance, World);
	};

	BIND_SYNC(cast_ray) << [this]
		(cr::Location StartLocation, cr::Location EndLocation)
		-> R<std::vector<cr::LabelledPoint>>
	{
		REQUIRE_CARLA_EPISODE();
		auto *World = Episode->GetWorld();
		FVector UEStartLocation = StartLocation;
		FVector UEEndLocation = EndLocation;
		ACarlaGameModeBase* GameMode = UCarlaStatics::GetGameMode(Episode->GetWorld());
		ALargeMapManager* LargeMap = GameMode->GetLMManager();
		if (LargeMap)
		{
			UEStartLocation = LargeMap->GlobalToLocalLocation(UEStartLocation);
			UEEndLocation = LargeMap->GlobalToLocalLocation(UEEndLocation);
		}
		return URayTracer::CastRay(StartLocation, EndLocation, World);
	};

	BIND_SYNC(get_actor_name) << [this](
	  cr::ActorId ActorID) -> R<std::string>
	{
		REQUIRE_CARLA_EPISODE();
		auto CarlaActor = Episode->FindCarlaActor(ActorID);
		if (CarlaActor == nullptr)
			return std::string();
		auto Actor = CarlaActor->GetActor();
		if (Actor == nullptr)
			return std::string();
		auto Name = Actor->GetName();
		if (Name.Len() == 0)
			return std::string();
		auto NameStr = StringCast<UTF8CHAR>(*Name, Name.Len());
		return std::string((const char*)NameStr.Get(), NameStr.Length());
	};

	BIND_SYNC(get_actor_class_name) << [this](
	  cr::ActorId ActorID) -> R<std::string>
	{
		REQUIRE_CARLA_EPISODE();
		auto CarlaActor = Episode->FindCarlaActor(ActorID);
		if (CarlaActor == nullptr)
			return std::string();
		auto Actor = CarlaActor->GetActor();
		if (Actor == nullptr)
			return std::string();
		auto Class = Actor->GetClass();
		if (Class == nullptr)
			return std::string();
		auto Name = Class->GetName();
		if (Name.Len() == 0)
			return std::string();
		auto NameStr = StringCast<UTF8CHAR>(*Name, Name.Len());
		return std::string((const char*)NameStr.Get(), NameStr.Length());
	};

	// ~~ Line Trace Functions ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// BIND_SYNC(line_trace_single) << [this](const std::string& json_params) -> R<std::string>
	// {
	// 	REQUIRE_CARLA_EPISODE();
 //  
	// 	FString JsonStr(UTF8_TO_TCHAR(json_params.c_str()));
	// 	TSharedPtr<FJsonObject> JsonObject;
	// 	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
 //  
	// 	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	// 	{
	// 		return RespondError("line_trace_single", ECarlaServerResponse::Failure, "Invalid JSON parameters");
	// 	}
 //  
	// 	double sensorId;
	// 	double tmpU = 0.0, tmpV = 0.0;
 //  
	// 	if (!JsonObject->TryGetNumberField(TEXT("sensor_id"), sensorId))
	// 	{
	// 		return RespondError("line_trace_single", ECarlaServerResponse::Failure, "Missing sensor_id");
	// 	}
 //  
	// 	if (!JsonObject->TryGetNumberField(TEXT("u"), tmpU))
	// 	{
	// 		return RespondError("line_trace_single", ECarlaServerResponse::Failure, "Missing u coordinate");
	// 	}
 //  
	// 	if (!JsonObject->TryGetNumberField(TEXT("v"), tmpV))
	// 	{
	// 		return RespondError("line_trace_single", ECarlaServerResponse::Failure, "Missing v coordinate");
	// 	}
 //  
	// 	cr::ActorId sensorActorID = static_cast<cr::ActorId>(static_cast<int32>(sensorId));
	// 	float U = static_cast<float>(tmpU);
	// 	float V = static_cast<float>(tmpV);
	// 	float MaxDistance = 100000.f; // in cm
 //  	
	// 	FCarlaActor* sensorCarlaActor = Episode->FindCarlaActor(sensorActorID);
	// 	if (!sensorCarlaActor)
	// 	{
	// 		return RespondError("line_trace_single", ECarlaServerResponse::ActorNotFound,
	// 							"Sensor not found: " + FString::FromInt((int32)sensorActorID));
	// 	}
 //  
	// 	AActor* FoundSensorActor = sensorCarlaActor->GetActor();
	// 	if (!FoundSensorActor)
	// 	{
	// 		return RespondError("line_trace_single", ECarlaServerResponse::ActorNotFound,
	// 							"Sensor not found: ");
	// 	}
 //  
	// 	USceneCaptureComponent2D* SceneCaptureComponent = FoundSensorActor->FindComponentByClass<USceneCaptureComponent2D>();
	// 	if (!SceneCaptureComponent)
	// 	{
	// 		return RespondError("line_trace_single", ECarlaServerResponse::Failure,
	// 							"USceneCaptureComponent2D not found on sensor: ");
	// 	}
 //  
	// 	FVector SensorLocation = FoundSensorActor->GetActorLocation();
	// 	FRotator SensorRotation = FoundSensorActor->GetActorRotation();
 //  
	// 	int32 ImageWidth = 1920;
	// 	int32 ImageHeight = 1080;
	// 	if (SceneCaptureComponent->TextureTarget)
	// 	{
	// 		ImageWidth = SceneCaptureComponent->TextureTarget->SizeX;
	// 		ImageHeight = SceneCaptureComponent->TextureTarget->SizeY;
	// 		ImageWidth = FMath::Max(1, ImageWidth);
	// 		ImageHeight = FMath::Max(1, ImageHeight);
	// 	}
 //  	
	// 	float FOV = SceneCaptureComponent->FOVAngle; // degrees
	// 	float AspectRatio = static_cast<float>(ImageWidth) / static_cast<float>(ImageHeight);
	// 	float HalfFOV = FMath::DegreesToRadians(FOV * 0.5f);
	// 	float HalfWidth = FMath::Tan(HalfFOV);
	// 	float HalfHeight = HalfWidth / AspectRatio;
 //  	
	// 	float NDC_X = (U / static_cast<float>(ImageWidth)) * 2.0f - 1.0f;
	// 	float NDC_Y = 1.0f - (V / static_cast<float>(ImageHeight)) * 2.0f;
 //  	
	// 	FVector LocalDir = FVector(1.0f, NDC_X * HalfWidth, NDC_Y * HalfHeight);
	// 	LocalDir = LocalDir.GetSafeNormal();
 //  
	// 	FVector WorldRayDirection = SensorRotation.RotateVector(LocalDir).GetSafeNormal();
	// 	FVector RayStart = SensorLocation + WorldRayDirection * 40;
	// 	FVector RayEnd = RayStart + WorldRayDirection * MaxDistance;
 //  
	// 	FHitResult HitResult;
	// 	UWorld* World = Episode->GetWorld();
	// 	if (!World)
	// 	{
	// 		return RespondError("line_trace_single", ECarlaServerResponse::Failure, "World pointer is null");
	// 	}
 //  
	// 	FCollisionQueryParams Params(SCENE_QUERY_STAT(LineTraceSingle), true);
	// 	Params.bReturnPhysicalMaterial = false;
 //  
	// 	bool bHit = World->LineTraceSingleByChannel(
	// 		HitResult,
	// 		RayStart,
	// 		RayEnd,
	// 		ECC_Visibility,
	// 		Params
	// 	);
	// 	DrawDebugLine(World, RayStart, RayEnd, FColor::Red, false,  30.f, 0, 1.0f);
 //  
	// 	// Create result JSON
	// 	TSharedPtr<FJsonObject> ResultObject = MakeShareable(new FJsonObject);
	// 	ResultObject->SetBoolField(TEXT("hit"), bHit);
 //  
	// 	if (bHit)
	// 	{
	// 		TSharedPtr<FJsonObject> HitLocation = MakeShareable(new FJsonObject);
	// 		HitLocation->SetNumberField(TEXT("x"), HitResult.Location.X / 100.0f);
	// 		HitLocation->SetNumberField(TEXT("y"), HitResult.Location.Y / 100.0f);
	// 		HitLocation->SetNumberField(TEXT("z"), HitResult.Location.Z / 100.0f);
	// 		ResultObject->SetObjectField(TEXT("location"), HitLocation);
	//
	// 		FVector SensorForward = SensorRotation.Vector();
	// 		FVector ToHitPoint = HitResult.Location - SensorLocation;
	// 		float Depth = FVector::DotProduct(ToHitPoint, SensorForward) / 100.0f; 
	// 		ResultObject->SetNumberField(TEXT("Depth"), Depth);
	// 	}
 //  
	// 	FString OutputString;
	// 	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	// 	FJsonSerializer::Serialize(ResultObject.ToSharedRef(), Writer);
 //  
	// 	return std::string(TCHAR_TO_UTF8(*OutputString));
	// };
	//
	// BIND_SYNC(line_trace_multiple) << [this](const std::string& json_params) -> R<std::string>
	// {
	// 	REQUIRE_CARLA_EPISODE();
	//
	// 	FString JsonStr(UTF8_TO_TCHAR(json_params.c_str()));
	// 	TSharedPtr<FJsonObject> JsonObject;
	// 	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	//
	// 	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	// 	{
	// 		return RespondError("line_trace_multiple", ECarlaServerResponse::Failure, "Invalid JSON parameters");
	// 	}
	//
	// 	double tmpSensorId = 0.0;
	// 	if (!JsonObject->TryGetNumberField(TEXT("sensor_id"), tmpSensorId))
	// 	{
	// 		return RespondError("line_trace_multiple", ECarlaServerResponse::Failure, "Missing sensor_id");
	// 	}
	//
	// 	const TArray<TSharedPtr<FJsonValue>>* UVArray = nullptr;
	// 	if (!JsonObject->TryGetArrayField(TEXT("uvs"), UVArray) || !UVArray)
	// 	{
	// 		return RespondError("line_trace_multiple", ECarlaServerResponse::Failure, "Missing uvs array");
	// 	}
	//
	// 	cr::ActorId SensorActorId = static_cast<cr::ActorId>(static_cast<int32>(tmpSensorId));
	// 	float MaxDistance = 100000.f; // in cm
	//
	// 	FCarlaActor* SensorCarlaActor = Episode->FindCarlaActor(SensorActorId);
	// 	if (!SensorCarlaActor)
	// 	{
	// 		return RespondError("line_trace_multiple", ECarlaServerResponse::ActorNotFound,
	// 							"Sensor not found: " + FString::FromInt((int32)SensorActorId));
	// 	}
	//
	// 	AActor* FoundSensorActor = SensorCarlaActor->GetActor();
	// 	if (!FoundSensorActor)
	// 	{
	// 		return RespondError("line_trace_multiple", ECarlaServerResponse::ActorNotFound,
	// 							"Sensor actor is null: " + FString::FromInt((int32)SensorActorId));
	// 	}
	//
	// 	USceneCaptureComponent2D* SceneCaptureComponent = FoundSensorActor->FindComponentByClass<USceneCaptureComponent2D>();
	// 	if (!SceneCaptureComponent)
	// 	{
	// 		return RespondError("line_trace_multiple", ECarlaServerResponse::Failure,
	// 							"USceneCaptureComponent2D not found on sensor: " + FString::FromInt((int32)SensorActorId));
	// 	}
	//
	// 	FVector SensorLocation = FoundSensorActor->GetActorLocation();
	// 	FRotator SensorRotation = FoundSensorActor->GetActorRotation();
	// 	FVector SensorForward = SensorRotation.Vector();
	//
	// 	int32 ImageWidth = 1920;
	// 	int32 ImageHeight = 1080;
	// 	if (SceneCaptureComponent->TextureTarget)
	// 	{
	// 		ImageWidth = FMath::Max(1, SceneCaptureComponent->TextureTarget->SizeX);
	// 		ImageHeight = FMath::Max(1, SceneCaptureComponent->TextureTarget->SizeY);
	// 	}
	//
	// 	float FOV = SceneCaptureComponent->FOVAngle;
	// 	float AspectRatio = static_cast<float>(ImageWidth) / static_cast<float>(ImageHeight);
	// 	float HalfFOV = FMath::DegreesToRadians(FOV * 0.5f);
	// 	float HalfWidth = FMath::Tan(HalfFOV);
	// 	float HalfHeight = HalfWidth / AspectRatio;
	//
	// 	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	// 	UWorld* World = Episode->GetWorld();
	// 	if (!World)
	// 	{
	// 		return RespondError("line_trace_multiple", ECarlaServerResponse::Failure, "World pointer is null");
	// 	}
	//
	// 	FCollisionQueryParams Params(SCENE_QUERY_STAT(LineTraceMultiple), true);
	// 	Params.bReturnPhysicalMaterial = false;
	//
	// 	for (const TSharedPtr<FJsonValue>& UVValue : *UVArray)
	// 	{
	// 		if (!UVValue.IsValid()) continue;
	// 		const TSharedPtr<FJsonObject>* UVObjectPtr = nullptr;
	// 		if (!UVValue->TryGetObject(UVObjectPtr) || !UVObjectPtr || !(*UVObjectPtr).IsValid()) continue;
	//
	// 		double tmpU = 0.0, tmpV = 0.0;
	// 		if (!(*UVObjectPtr)->TryGetNumberField(TEXT("u"), tmpU) || !(*UVObjectPtr)->TryGetNumberField(TEXT("v"), tmpV))
	// 			continue;
	//
	// 		float U = static_cast<float>(tmpU);
	// 		float V = static_cast<float>(tmpV);
	//
	// 		float NDC_X = (U / static_cast<float>(ImageWidth)) * 2.0f - 1.0f;
	// 		float NDC_Y = 1.0f - (V / static_cast<float>(ImageHeight)) * 2.0f;
	//
	// 		FVector LocalDir = FVector(1.0f, NDC_X * HalfWidth, NDC_Y * HalfHeight).GetSafeNormal();
	// 		FVector WorldRayDirection = SensorRotation.RotateVector(LocalDir).GetSafeNormal();
	// 		FVector RayStart = SensorLocation + WorldRayDirection * 40;;
	// 		FVector RayEnd = RayStart + WorldRayDirection * MaxDistance;
	//
	// 		FHitResult HitResult;
	// 		bool bHit = World->LineTraceSingleByChannel(HitResult, RayStart, RayEnd, ECC_Visibility, Params);
	// 		DrawDebugLine(World, RayStart, RayEnd, FColor::Red, false, 30.0f, 0, 1.0f);
	// 		DrawDebugPoint(World, HitResult.Location, 20.f, FColor::Green, false, 30.f);
	//
	// 		TSharedPtr<FJsonObject> ResultObject = MakeShareable(new FJsonObject);
	// 		ResultObject->SetBoolField(TEXT("hit"), bHit);
	//
	// 		if (bHit)
	// 		{
	// 			TSharedPtr<FJsonObject> HitLocation = MakeShareable(new FJsonObject);
	// 			HitLocation->SetNumberField(TEXT("x"), HitResult.Location.X / 100.0f);
	// 			HitLocation->SetNumberField(TEXT("y"), HitResult.Location.Y / 100.0f);
	// 			HitLocation->SetNumberField(TEXT("z"), HitResult.Location.Z / 100.0f);
	// 			ResultObject->SetObjectField(TEXT("location"), HitLocation);
	//
	// 			float Depth = FVector::DotProduct(HitResult.Location - SensorLocation, SensorForward) / 100.0f;
	// 			ResultObject->SetNumberField(TEXT("Depth"), Depth);
	// 		}
	//
	// 		ResultsArray.Add(MakeShareable(new FJsonValueObject(ResultObject)));
	// 	}
	//
	// 	TSharedPtr<FJsonObject> FinalResult = MakeShareable(new FJsonObject);
	// 	FinalResult->SetArrayField(TEXT("results"), ResultsArray);
	//
	// 	FString OutputString;
	// 	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	// 	FJsonSerializer::Serialize(FinalResult.ToSharedRef(), Writer);
	//
	// 	return std::string(TCHAR_TO_UTF8(*OutputString));
	// };

	BIND_SYNC(line_trace_single) << [this](const std::string& json_params) -> R<std::string>
	{
		REQUIRE_CARLA_EPISODE();
		
		TSharedPtr<FJsonObject> JsonObject;
		FString ErrorMessage;
		if (!ParseJsonString(json_params, JsonObject, ErrorMessage))
		{
		  return RespondError("line_trace_single", ECarlaServerResponse::Failure, ErrorMessage);
		}
		
		double sensorId;
		double tmpU = 0.0, tmpV = 0.0;
		
		if (!JsonObject->TryGetNumberField(TEXT("sensor_id"), sensorId))
		{
		  return RespondError("line_trace_single", ECarlaServerResponse::Failure, "Missing sensor_id");
		}
		
		if (!JsonObject->TryGetNumberField(TEXT("u"), tmpU))
		{
		  return RespondError("line_trace_single", ECarlaServerResponse::Failure, "Missing u coordinate");
		}
		
		if (!JsonObject->TryGetNumberField(TEXT("v"), tmpV))
		{
		  return RespondError("line_trace_single", ECarlaServerResponse::Failure, "Missing v coordinate");
		}
		
		cr::ActorId sensorActorID = static_cast<cr::ActorId>(static_cast<int32>(sensorId));
		float U = static_cast<float>(tmpU);
		float V = static_cast<float>(tmpV);
		float MaxDistance = 100000.f; // in cm
		
		FCarlaActor* sensorCarlaActor = Episode->FindCarlaActor(sensorActorID);
		if (!sensorCarlaActor)
		{
		  return RespondError("line_trace_single", ECarlaServerResponse::ActorNotFound,
							  "Sensor not found: " + FString::FromInt((int32)sensorActorID));
		}
		
		AActor* FoundSensorActor = sensorCarlaActor->GetActor();
		if (!FoundSensorActor)
		{
		  return RespondError("line_trace_single", ECarlaServerResponse::ActorNotFound,
							  "Sensor not found: ");
		}
		
		USceneCaptureComponent2D* SceneCaptureComponent = FoundSensorActor->FindComponentByClass<USceneCaptureComponent2D>();
		if (!SceneCaptureComponent)
		{
		  return RespondError("line_trace_single", ECarlaServerResponse::Failure,
							  "USceneCaptureComponent2D not found on sensor: ");
		}
		
		FVector SensorLocation = FoundSensorActor->GetActorLocation();
		FRotator SensorRotation = FoundSensorActor->GetActorRotation();
		
		int32 ImageWidth = 1920;
		int32 ImageHeight = 1080;
		if (SceneCaptureComponent->TextureTarget)
		{
		  ImageWidth = SceneCaptureComponent->TextureTarget->SizeX;
		  ImageHeight = SceneCaptureComponent->TextureTarget->SizeY;
		  ImageWidth = FMath::Max(1, ImageWidth);
		  ImageHeight = FMath::Max(1, ImageHeight);
		}
		
		float FOV = SceneCaptureComponent->FOVAngle; // degrees
		
		UWorld* World = Episode->GetWorld();
		if (!World)
		{
		  return RespondError("line_trace_single", ECarlaServerResponse::Failure, "World pointer is null");
		}
		
		// 兼容归一化或其它分辨率坐标：
		// 1) 若传入[0,1]范围则转换为像素
		// 2) 若调用方基于不同分辨率（可在JSON中附带 image_width/image_height），则按比例映射到当前 ImageWidth/ImageHeight
		if (U >= 0.0f && U <= 1.0f && V >= 0.0f && V <= 1.0f)
		{
			U = U * static_cast<float>(ImageWidth);
			V = V * static_cast<float>(ImageHeight);
		}
		
		const FString OutputString = ULineTraceUtils::LineTraceSingleFromCameraJson(
			World,
			SensorLocation,
			SensorRotation,
			FOV,
			ImageWidth,
			ImageHeight,
			U,
			V,
			MaxDistance,
			true
		);
		
		return std::string(TCHAR_TO_UTF8(*OutputString));
	};
 
	BIND_SYNC(line_trace_multiple) << [this](const std::string& json_params) -> R<std::string>
	{
	    REQUIRE_CARLA_EPISODE();
 
	    TSharedPtr<FJsonObject> JsonObject;
	    FString ErrorMessage;
	    if (!ParseJsonString(json_params, JsonObject, ErrorMessage))
	    {
	        return RespondError("line_trace_multiple", ECarlaServerResponse::Failure, ErrorMessage);
	    }
 
	    double tmpSensorId = 0.0;
	    if (!JsonObject->TryGetNumberField(TEXT("sensor_id"), tmpSensorId))
	    {
	        return RespondError("line_trace_multiple", ECarlaServerResponse::Failure, "Missing sensor_id");
	    }
 
	    const TArray<TSharedPtr<FJsonValue>>* UVArray = nullptr;
	    if (!JsonObject->TryGetArrayField(TEXT("uvs"), UVArray) || !UVArray)
	    {
	        return RespondError("line_trace_multiple", ECarlaServerResponse::Failure, "Missing uvs array");
	    }
 
	    cr::ActorId SensorActorId = static_cast<cr::ActorId>(static_cast<int32>(tmpSensorId));
	    float MaxDistance = 100000.f; // in cm
 
	    FCarlaActor* SensorCarlaActor = Episode->FindCarlaActor(SensorActorId);
	    if (!SensorCarlaActor)
	    {
	        return RespondError("line_trace_multiple", ECarlaServerResponse::ActorNotFound,
	                            "Sensor not found: " + FString::FromInt((int32)SensorActorId));
	    }
 
	    AActor* FoundSensorActor = SensorCarlaActor->GetActor();
	    if (!FoundSensorActor)
	    {
	        return RespondError("line_trace_multiple", ECarlaServerResponse::ActorNotFound,
	                            "Sensor actor is null: " + FString::FromInt((int32)SensorActorId));
	    }
 
	    USceneCaptureComponent2D* SceneCaptureComponent = FoundSensorActor->FindComponentByClass<USceneCaptureComponent2D>();
	    if (!SceneCaptureComponent)
	    {
	        return RespondError("line_trace_multiple", ECarlaServerResponse::Failure,
	                            "USceneCaptureComponent2D not found on sensor: " + FString::FromInt((int32)SensorActorId));
	    }
 
			// 使用组件世界位姿
			FVector SensorLocation = SceneCaptureComponent->GetComponentLocation();
			FRotator SensorRotation = SceneCaptureComponent->GetComponentRotation();
		 
			int32 ImageWidth = 1920;
			int32 ImageHeight = 1080;
			if (SceneCaptureComponent->TextureTarget)
			{
				ImageWidth = FMath::Max(1, SceneCaptureComponent->TextureTarget->SizeX);
				ImageHeight = FMath::Max(1, SceneCaptureComponent->TextureTarget->SizeY);
			}
		 
			float FOV = SceneCaptureComponent->FOVAngle;
		 
			UWorld* World = Episode->GetWorld();
			if (!World)
			{
				return RespondError("line_trace_multiple", ECarlaServerResponse::Failure, "World pointer is null");
			}
		 
			// 解析 UV 坐标数组
			TArray<FVector2D> UVs;
			for (const TSharedPtr<FJsonValue>& UVValue : *UVArray)
			{
				if (!UVValue.IsValid()) continue;
				const TSharedPtr<FJsonObject>* UVObjectPtr = nullptr;
				if (!UVValue->TryGetObject(UVObjectPtr) || !UVObjectPtr || !(*UVObjectPtr).IsValid()) continue;
		 
				double tmpU = 0.0, tmpV = 0.0;
				if (!(*UVObjectPtr)->TryGetNumberField(TEXT("u"), tmpU) || !(*UVObjectPtr)->TryGetNumberField(TEXT("v"), tmpV))
					continue;
		 
				float uPix = static_cast<float>(tmpU);
				float vPix = static_cast<float>(tmpV);
				// 兼容归一化UV：若传入[0,1]范围则转换为像素
				if (uPix >= 0.0f && uPix <= 1.0f && vPix >= 0.0f && vPix <= 1.0f)
				{
					uPix *= static_cast<float>(ImageWidth);
					vPix *= static_cast<float>(ImageHeight);
				}
				UVs.Add(FVector2D(uPix, vPix));
			}
 
	    const FString OutputString = ULineTraceUtils::LineTraceMultipleFromCameraJson(
	        World,
	        SensorLocation,
	        SensorRotation,
	        FOV,
	        ImageWidth,
	        ImageHeight,
	        UVs,
	        MaxDistance,
	        true  // 使用前向方向计算深度
	    );
 
	    return std::string(TCHAR_TO_UTF8(*OutputString));
	};

	BIND_SYNC(line_trace_single_from_player_camera) << [this](const std::string& json_params) -> R<std::string>
	{
		REQUIRE_CARLA_EPISODE();
		
		TSharedPtr<FJsonObject> JsonObject;
		FString ErrorMessage;
		if (!ParseJsonString(json_params, JsonObject, ErrorMessage))
		{
			return RespondError("line_trace_single_from_player_camera", ECarlaServerResponse::Failure, ErrorMessage);
		}
		
		// 解析屏幕坐标
		double tmpScreenX = 0.0, tmpScreenY = 0.0;
		if (!JsonObject->TryGetNumberField(TEXT("screen_x"), tmpScreenX))
		{
			return RespondError("line_trace_single_from_player_camera", ECarlaServerResponse::Failure, "Missing screen_x");
		}
		if (!JsonObject->TryGetNumberField(TEXT("screen_y"), tmpScreenY))
		{
			return RespondError("line_trace_single_from_player_camera", ECarlaServerResponse::Failure, "Missing screen_y");
		}
		
		// 解析最大距离（可选，默认10000米）
		double tmpMaxDistance = 10000.0;
		JsonObject->TryGetNumberField(TEXT("max_distance"), tmpMaxDistance);
		float MaxDistance = static_cast<float>(tmpMaxDistance);
		
		float ScreenX = static_cast<float>(tmpScreenX);
		float ScreenY = static_cast<float>(tmpScreenY);
		
		UWorld* World = Episode->GetWorld();
		if (!World)
		{
			return RespondError("line_trace_single_from_player_camera", ECarlaServerResponse::Failure, "World pointer is null");
		}
		
		UCarlaGameInstance* GameInstance = UCarlaStatics::GetGameInstance(World);
		if (!GameInstance)
		{
			return RespondError("line_trace_single_from_player_camera", ECarlaServerResponse::Failure, "GameInstance not found");
		}
		
		USvcLineTraceSubsystem* LineTraceSubsystem = GameInstance->GetSubsystem<USvcLineTraceSubsystem>();
		if (!LineTraceSubsystem)
		{
			return RespondError("line_trace_single_from_player_camera", ECarlaServerResponse::Failure, "SvcLineTraceSubsystem not found");
		}
		
		// 调用子系统方法（PlayerController 参数为 nullptr，子系统会自动获取本地玩家控制器）
		const FString OutputString = LineTraceSubsystem->LineTraceSingleFromPlayerCameraJson(
			nullptr,  // 自动获取本地玩家控制器
			ScreenX,
			ScreenY,
			MaxDistance
		);
		
		if (OutputString.IsEmpty())
		{
			return RespondError("line_trace_single_from_player_camera", ECarlaServerResponse::Failure, "Line trace failed");
		}
		
		return std::string(TCHAR_TO_UTF8(*OutputString));
	};

  // ~~ Geometry Drawer (points/lines/cubes) via JSON ~~~~~~~~~~~~~~~~~~~~~~~~~~
  BIND_SYNC(draw_geometry_point) << [this](const std::string& json) -> R<std::string>
  {
    REQUIRE_CARLA_EPISODE();
    UCarlaGameInstance* GameInstance = UCarlaStatics::GetGameInstance(Episode->GetWorld());
    if (!GameInstance)
    	return std::string(R"({"ok":false,"error":"no GameInstance"})");
    if (USvcGeometryDrawerSubsystem* Subsys = GameInstance->GetSubsystem<USvcGeometryDrawerSubsystem>())
    {
      FString Id = Subsys->DrawPointJson(UTF8_TO_TCHAR(json.c_str()));
      return std::string(TCHAR_TO_UTF8(*Id));
    }
  	return std::string(R"({"ok":false,"error":"subsystem missing"})");
  };

  BIND_SYNC(draw_geometry_line) << [this](const std::string& json) -> R<std::string>
  {
    REQUIRE_CARLA_EPISODE();
    UCarlaGameInstance* GameInstance = UCarlaStatics::GetGameInstance(Episode->GetWorld());
    if (!GameInstance)
    	return std::string(R"({"ok":false,"error":"no GameInstance"})");
    if (USvcGeometryDrawerSubsystem* Subsys = GameInstance->GetSubsystem<USvcGeometryDrawerSubsystem>())
    {
      FString Id = Subsys->DrawLineJson(UTF8_TO_TCHAR(json.c_str()));
      return std::string(TCHAR_TO_UTF8(*Id));
    }
  	return std::string(R"({"ok":false,"error":"subsystem missing"})");
  };

  BIND_SYNC(draw_geometry_cube) << [this](const std::string& json) -> R<std::string>
  {
	REQUIRE_CARLA_EPISODE();
	UCarlaGameInstance* GameInstance = UCarlaStatics::GetGameInstance(Episode->GetWorld());
	if (!GameInstance)
	    return std::string(R"({"ok":false,"error":"no GameInstance"})");
	if (USvcGeometryDrawerSubsystem* Subsys = GameInstance->GetSubsystem<USvcGeometryDrawerSubsystem>())
	{
		FString Id = Subsys->DrawCubeJson(UTF8_TO_TCHAR(json.c_str()));
		return std::string(TCHAR_TO_UTF8(*Id));
	}
	return std::string(R"({"ok":false,"error":"subsystem missing"})");
  };

  BIND_SYNC(remove_geometry_object) << [this](const std::string& json) -> R<std::string>
  {
  	REQUIRE_CARLA_EPISODE();
  	UCarlaGameInstance* GameInstance = UCarlaStatics::GetGameInstance(Episode->GetWorld());
  	if (!GameInstance)
  		return std::string(R"({"ok":false,"error":"no GameInstance"})");
  	if (USvcGeometryDrawerSubsystem* Subsys = GameInstance->GetSubsystem<USvcGeometryDrawerSubsystem>())
  	{
  		return TCHAR_TO_UTF8(*Subsys->RemoveJson(UTF8_TO_TCHAR(json.c_str())));
  	}
  	return std::string(R"({"ok":false,"error":"subsystem missing"})");
  };

  BIND_SYNC(clear_geometry_objects) << [this](const std::string& json) ->  R<std::string>
  {
  	REQUIRE_CARLA_EPISODE();
  	UCarlaGameInstance* GameInstance = UCarlaStatics::GetGameInstance(Episode->GetWorld());
  	if (!GameInstance)
  		return std::string(R"({"ok":false,"error":"no GameInstance"})");
  	if (USvcGeometryDrawerSubsystem* Subsys = GameInstance->GetSubsystem<USvcGeometryDrawerSubsystem>())
  	{
  		return TCHAR_TO_UTF8(*Subsys->ClearJson(UTF8_TO_TCHAR(json.c_str())));
  	}
  	return std::string(R"({"ok":false,"error":"subsystem missing"})");
  };

	// ~~ Map Manager ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	BIND_SYNC(load_map) << [this](const std::string& AbsPath) -> R<bool>
	{
		REQUIRE_CARLA_EPISODE();
		UCarlaGameInstance* GameInstance = UCarlaStatics::GetGameInstance(Episode->GetWorld());
		if (!GameInstance)
			return false;
		if (USvcMapSubsystem* Subsys = GameInstance->GetSubsystem<USvcMapSubsystem>())
		{
			return Subsys->LoadMap(AbsPath.c_str());;
		}
		return false;
	};

	BIND_SYNC(unload_map) << [this]() -> R<bool>
	{
		REQUIRE_CARLA_EPISODE();
		UCarlaGameInstance* GameInstance = UCarlaStatics::GetGameInstance(Episode->GetWorld());
		if (!GameInstance)
			return false;
		if (USvcMapSubsystem* Subsys = GameInstance->GetSubsystem<USvcMapSubsystem>())
		{
			return Subsys->Unload();
		}
		return false;
	};
}

// =============================================================================
// -- Undef helper macros ------------------------------------------------------
// =============================================================================

#undef BIND_ASYNC
#undef BIND_SYNC
#undef REQUIRE_CARLA_EPISODE
#undef RESPOND_ERROR_FSTRING
#undef RESPOND_ERROR
#undef CARLA_ENSURE_GAME_THREAD

// =============================================================================
// -- FCarlaServer -------------------------------------------------------
// =============================================================================

FCarlaServer::FCarlaServer() : Pimpl(nullptr) {}

FCarlaServer::~FCarlaServer() {
  Stop();
}

FDataMultiStream FCarlaServer::Start(uint16_t RPCPort, uint16_t StreamingPort, uint16_t SecondaryPort)
{
  Pimpl = MakeUnique<FPimpl>(RPCPort, StreamingPort, SecondaryPort);
  StreamingPort = Pimpl->StreamingServer.GetLocalEndpoint().port();
  SecondaryPort = Pimpl->SecondaryServer->GetLocalEndpoint().port();

  UE_LOG(
      LogCarlaServer,
      Log,
      TEXT("Initialized CarlaServer: Ports(rpc=%d, streaming=%d, secondary=%d)"),
      RPCPort,
      StreamingPort,
      SecondaryPort);
  return Pimpl->BroadcastStream;
}

void FCarlaServer::NotifyBeginEpisode(UCarlaEpisode &Episode)
{
  check(Pimpl != nullptr);
  UE_LOG(LogCarlaServer, Log, TEXT("New episode '%s' started"), *Episode.GetMapName());
  Pimpl->Episode = &Episode;
}

void FCarlaServer::NotifyEndEpisode()
{
  check(Pimpl != nullptr);
  Pimpl->Episode = nullptr;
}

void FCarlaServer::AsyncRun(uint32 NumberOfWorkerThreads)
{
  check(Pimpl != nullptr);
  /// @todo Define better the number of threads each server gets.
  int ThreadsPerServer = std::max(2u, NumberOfWorkerThreads / 3u);
  int32_t RPCThreads;
  int32_t StreamingThreads;
  int32_t SecondaryThreads;

  UE_LOG(LogCarla, Log, TEXT("FCommandLine %s"), FCommandLine::Get());

  if(!FParse::Value(FCommandLine::Get(), TEXT("-RPCThreads="), RPCThreads))
  {
    RPCThreads = ThreadsPerServer;
  }
  if(!FParse::Value(FCommandLine::Get(), TEXT("-StreamingThreads="), StreamingThreads))
  {
    StreamingThreads = ThreadsPerServer;
  }
  if(!FParse::Value(FCommandLine::Get(), TEXT("-SecondaryThreads="), SecondaryThreads))
  {
    SecondaryThreads = ThreadsPerServer;
  }

  UE_LOG(LogCarla, Log, TEXT("FCarlaServer AsyncRun %d, RPCThreads %d, StreamingThreads %d, SecondaryThreads %d"),
        NumberOfWorkerThreads, RPCThreads, StreamingThreads, SecondaryThreads);

  Pimpl->Server.AsyncRun(RPCThreads);
  Pimpl->StreamingServer.AsyncRun(StreamingThreads);
  Pimpl->SecondaryServer->AsyncRun(SecondaryThreads);
}

void FCarlaServer::RunSome(uint32 Milliseconds)
{
  TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);
  Pimpl->Server.SyncRunFor(carla::time_duration::milliseconds(Milliseconds));
}

void FCarlaServer::Tick()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(FCarlaServer::Tick);
  (void)Pimpl->TickCuesReceived.fetch_add(1, std::memory_order_release);
}

bool FCarlaServer::TickCueReceived()
{
  TRACE_CPUPROFILER_EVENT_SCOPE(FCarlaServer::TickCueReceived);
  auto k = Pimpl->TickCuesReceived.fetch_sub(1, std::memory_order_acquire);
  bool flag = (k > 0);
  if (!flag)
    (void)Pimpl->TickCuesReceived.fetch_add(1, std::memory_order_release);
  return flag;
}

void FCarlaServer::Stop()
{
  if (Pimpl)
  {
    Pimpl->Server.Stop();
    Pimpl->SecondaryServer->Stop();
  }
}

FDataStream FCarlaServer::OpenStream() const
{
  check(Pimpl != nullptr);
  return Pimpl->StreamingServer.MakeStream();
}

std::shared_ptr<carla::multigpu::Router> FCarlaServer::GetSecondaryServer()
{
  return Pimpl->GetSecondaryServer();
}

carla::streaming::Server &FCarlaServer::GetStreamingServer()
{
  return Pimpl->StreamingServer;
}
