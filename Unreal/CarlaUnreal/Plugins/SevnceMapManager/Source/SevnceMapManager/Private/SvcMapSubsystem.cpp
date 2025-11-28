#include "SvcMapSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"
#include "UObject/UObjectGlobals.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "LCCActor.h"
#include "LCCComponent.h"
#include "Kismet/GameplayStatics.h"


void USvcMapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	m_MapToComplexCollision.Add(TEXT("ljgc_01.lcc"), TEXT("/SevnceMapManager/Statics/LijiaCollisionComplex.LijiaCollisionComplex"));
	m_MapToComplexCollision.Add(TEXT("nmh_01.lcc"), TEXT("/SevnceMapManager/Statics/nmhCollisionComplex.nmhCollisionComplex"));

	m_MapToSimpleCollision.Add(TEXT("ljgc_01.lcc"), TEXT("/SevnceMapManager/Statics/LijiaCollisionSimple.LijiaCollisionSimple"));
	m_MapToSimpleCollision.Add(TEXT("nmh_01.lcc"), TEXT("/SevnceMapManager/Statics/nmhCollisionSimple.nmhCollisionSimple"));
}

void USvcMapSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

bool USvcMapSubsystem::LoadMap(const FString& Path)
{
	
	UWorld* world = GetWorld();
	if (!world)
	{
		return false;
	}

	ALCCActor* lccActor = InitLCCActor();
	if (!IsValid(lccActor))
	{
		return false;
	}

	ULCCComponent* lccComp = lccActor->GetLCCComponent();
	if (!IsValid(lccComp))
	{
		return false;
	}

	// 卸载当前地图
	if (lccComp->CheckIfLoaded())
		lccComp->UnLoad();
	
	// 加载新地图
	const bool loaded = lccComp->Load(Path);

	if (!loaded)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load dataset from: %s"), *Path);
		return false;
	}
	FString cleanName = FPaths::GetCleanFilename(Path);
	// 碰撞文件
	LoadAndAttachQueryMesh(cleanName);
	LoadAndAttachNaviMesh(cleanName);
	return true;
}

bool USvcMapSubsystem::Unload()
{
    ALCCActor* actor = m_LccActor.Get();
    if (!IsValid(actor))
    {
        return false;
    }
    if (ULCCComponent* comp = actor->GetLCCComponent())
    {
        comp->UnLoad();
    }
    return true;
}

ALCCActor* USvcMapSubsystem::InitLCCActor()
{
	if (!GetWorld())
		return nullptr;
	
	FStringAssetReference BlueprintPath(TEXT("/SevnceMapManager/BPs/BP_LCCActor.BP_LCCActor_C"));
	UClass* actorClass = Cast<UClass>(BlueprintPath.TryLoad());
	
	AActor* actor = UGameplayStatics::GetActorOfClass(GetWorld(), actorClass);
	if (!actor)
		return nullptr;
	
	m_LccActor = Cast<ALCCActor>(actor);
	return m_LccActor.Get();
	// FStringAssetReference BlueprintPath(TEXT("/SevnceMapManager/BPs/BP_LCCActor.BP_LCCActor_C"));
	// UClass* actorClass = Cast<UClass>(BlueprintPath.TryLoad());
	//
	// if (!IsValid(actorClass))
	// {
	// 	UE_LOG(LogTemp, Error, TEXT("无法加载蓝图类，请检查路径 /SevnceMapManager/BPs/BP_LCCActor.BP_LCCActor_C"));
	// 	return nullptr; // 或者返回一个错误码
	// }
	//
	// if (!IsValid(m_LccActor.Get()))
	// {
	// 	UWorld* world = GetWorld();
	// 	if (world)
	// 	{
	// 		FVector SpawnLocation = FVector(0.0f, 0.0f, 0.0f);
	// 		FRotator SpawnRotation = FRotator::ZeroRotator;
	//
	// 		AActor* actor = world->SpawnActor(actorClass, &SpawnLocation, &SpawnRotation);
	// 		
	// 		m_LccActor = Cast<ALCCActor>(actor);
	//
	// 		// 关键：检查转换是否成功
	// 		if (IsValid(m_LccActor.Get()))
	// 		{
	// 			ULCCComponent* LCCComp = m_LccActor->GetLCCComponent();
	// 			if (IsValid(LCCComp))
	// 			{
	// 				LCCComp->SetLCCCollisionEnable(false);
	// 				UE_LOG(LogTemp, Log, TEXT("LCCActor生成并初始化成功！"));
	// 			}
	// 			else
	// 			{
	// 				UE_LOG(LogTemp, Error, TEXT("LCCActor生成成功，但获取LCCComponent失败！"));
	// 				// 这里可能需要清理掉已经生成的无效Actor
	// 				m_LccActor->Destroy();
	// 				m_LccActor = nullptr;
	// 			}
	// 		}
	// 		else
	// 		{
	// 			UE_LOG(LogTemp, Error, TEXT("生成的Actor无法转换为ALCCACtor类型，请检查蓝图基类！"));
	// 		}
	// 	}
	// }
	//
	// // 返回最终生成的Actor指针
	// return m_LccActor.Get();
}

AActor* USvcMapSubsystem::GetOrCreateMeshActor()
{
	// 检查缓存的Actor是否有效
	AActor* meshActor = m_MeshActor.Get();
	if (IsValid(meshActor))
	{
		return meshActor;
	}

	// 如果无效，则创建一个新的
	UWorld* world = GetWorld();
	if (!world)
	{
		UE_LOG(LogTemp, Error, TEXT("GetOrCreateMeshActor: Could not get UWorld."));
		return nullptr;
	}

	meshActor = world->SpawnActor<AActor>(AActor::StaticClass(), FTransform());
	if (IsValid(meshActor))
	{
		USceneComponent* rootComp = NewObject<USceneComponent>(meshActor, TEXT("DefaultSceneRoot"));
		if (rootComp)
		{
			rootComp->RegisterComponent();
			meshActor->SetRootComponent(rootComp);
			meshActor->AddInstanceComponent(rootComp);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create a root component for the MeshActor."));
			meshActor->Destroy();
			return nullptr;
		}
		m_MeshActor = meshActor;
		UE_LOG(LogTemp, Log, TEXT("Successfully created and cached a new MeshActor for collision."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to spawn a new MeshActor."));
	}

	return meshActor;
}

void USvcMapSubsystem::LoadAndAttachQueryMesh(const FString& DataSetFileName)
{
    AActor* targetActor = GetOrCreateMeshActor();
    if (!IsValid(targetActor))
    {
        UE_LOG(LogTemp, Error, TEXT("LoadAndAttachQueryMesh: Could not get a valid TargetActor."));
        return;
    }
    
    const FString* foundAssetPath = m_MapToComplexCollision.Find(DataSetFileName);
    if (!foundAssetPath)
    {
        UE_LOG(LogTemp, Warning, TEXT("No query mesh mapping found for dataset: %s"), *DataSetFileName);
        return;
    }

    const FString& assetPath = *foundAssetPath;
    if (assetPath.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Found mapping for '%s', but assetPath is empty."), *DataSetFileName);
        return;
    }
    
    UStaticMesh* mesh = LoadObject<UStaticMesh>(nullptr, *assetPath);
    if (!mesh)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load StaticMesh from: %s"), *assetPath);
        return;
    }

    // 清理现有的Mesh组件
    TArray<UStaticMeshComponent*> existingComps;
    targetActor->GetComponents(existingComps);
    for (UStaticMeshComponent* comp : existingComps)
    {
        comp->DestroyComponent();
    }
    
    UStaticMeshComponent* meshComp = NewObject<UStaticMeshComponent>(targetActor, UStaticMeshComponent::StaticClass(), TEXT("CollisionMeshComp"));
    if (!meshComp)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create StaticMeshComponent"));
        return;
    }
    meshComp->SetStaticMesh(mesh);
    meshComp->SetCollisionObjectType(ECC_GameTraceChannel4); // GSSceneObject
    meshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
    meshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel5, ECR_Block);// GSLidarChannel
    meshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	
    meshComp->SetMobility(EComponentMobility::Movable);
    meshComp->SetVisibility(false);
    meshComp->SetHiddenInGame(true);
	
    meshComp->RegisterComponent();
    meshComp->AttachToComponent(targetActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    targetActor->AddInstanceComponent(meshComp);
	
    if (meshComp->GetCollisionEnabled() != ECollisionEnabled::QueryOnly)
    {
        UE_LOG(LogTemp, Warning, TEXT("Mesh component collision is not QueryOnly!"));
    }

    if (meshComp->GetCollisionObjectType() != ECC_GameTraceChannel4)
    {
        UE_LOG(LogTemp, Warning, TEXT("Mesh component Object Type is not 3dgs!"));
    }
}

void USvcMapSubsystem::LoadAndAttachNaviMesh(const FString& DataSetFileName)
{
    AActor* targetActor = GetOrCreateMeshActor();
    if (!IsValid(targetActor))
    {
        UE_LOG(LogTemp, Error, TEXT("LoadAndAttachNaviMesh: Could not get a valid TargetActor."));
        return;
    }
    
    // 从 m_MapToSimpleCollision 中查找资产路径
    const FString* foundAssetPath = m_MapToSimpleCollision.Find(DataSetFileName);
    if (!foundAssetPath)
    {
        UE_LOG(LogTemp, Warning, TEXT("No navigation mesh mapping found for dataset: %s"), *DataSetFileName);
        return;
    }

    const FString& assetPath = *foundAssetPath;
    if (assetPath.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Found navigation mapping for '%s', but assetPath is empty."), *DataSetFileName);
        return;
    }
    
    UStaticMesh* mesh = LoadObject<UStaticMesh>(nullptr, *assetPath);
    if (!mesh)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load navigation StaticMesh from: %s"), *assetPath);
        return;
    }
	
    UStaticMeshComponent* naviMeshComp = NewObject<UStaticMeshComponent>(targetActor, UStaticMeshComponent::StaticClass(), TEXT("NaviMeshComp"));
    if (!naviMeshComp)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create NaviMesh StaticMeshComponent"));
        return;
    }

    naviMeshComp->SetStaticMesh(mesh);
    naviMeshComp->SetCollisionObjectType(ECC_WorldStatic); 
	naviMeshComp->SetCollisionResponseToAllChannels(ECR_Block);
    naviMeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	naviMeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel5, ECR_Ignore);
    naviMeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    naviMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    naviMeshComp->SetMobility(EComponentMobility::Movable); // 导航网格通常是静态的
    naviMeshComp->SetVisibility(false);
    naviMeshComp->SetHiddenInGame(true);
    naviMeshComp->RegisterComponent();
    naviMeshComp->AttachToComponent(targetActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    targetActor->AddInstanceComponent(naviMeshComp);

    // 添加一个日志确认创建成功
    UE_LOG(LogTemp, Log, TEXT("Successfully loaded and attached navigation mesh for dataset: %s"), *DataSetFileName);
}