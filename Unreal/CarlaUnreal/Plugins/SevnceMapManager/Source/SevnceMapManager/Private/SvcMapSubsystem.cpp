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


void USvcMapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	m_MapToComplexCollision.Add(TEXT("ljgc_01.lcc"), TEXT("/SevnceMapManager/Statics/LijiaCollisionComplex.LijiaCollisionComplex"));
	m_MapToComplexCollision.Add(TEXT("nmh_01.lcc"), TEXT(""));

	m_MapToSimpleCollision.Add(TEXT("ljgc_01.lcc"), TEXT("/SevnceMapManager/Statics/LijiaCollisionSimple.LijiaCollisionSimple"));
	m_MapToSimpleCollision.Add(TEXT("nmh_01.lcc"), TEXT(""));
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

	ALCCActor* lccActor = GetOrCreateLCCActor();
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

ALCCActor* USvcMapSubsystem::GetOrCreateLCCActor()
{
	ALCCActor* lccActor = m_LccActor.Get();
    
	if (!IsValid(lccActor))
	{
		UWorld* world = GetWorld();
		if (world)
		{
			lccActor = world->SpawnActor<ALCCActor>({0,0,0}, {0,0,0});
			lccActor->GetLCCComponent()->SetLCCCollisionEnable(false);
			m_LccActor = lccActor;
		}
	}
    
	return lccActor;
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
	meshComp->SetWorldScale3D({-100, -100, 100});
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
	naviMeshComp->SetWorldScale3D({-1, -1, 1});
    naviMeshComp->SetVisibility(false);
    naviMeshComp->SetHiddenInGame(true);
    naviMeshComp->RegisterComponent();
    naviMeshComp->AttachToComponent(targetActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    targetActor->AddInstanceComponent(naviMeshComp);

    // 添加一个日志确认创建成功
    UE_LOG(LogTemp, Log, TEXT("Successfully loaded and attached navigation mesh for dataset: %s"), *DataSetFileName);
}