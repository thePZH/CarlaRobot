// Copyright (c) 2025 Computer Vision Center (CVC).
// MIT License.

#include "Carla/StaticObjects/StaticObjectRegistry.h"
#include "Carla/StaticObjects/StaticObjectRegistryDataAsset.h"

#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"
#include "Logging/LogMacros.h"

const FString FStaticObjectRegistry::DefaultDataAssetPath = TEXT("/Game/Carla/StaticObjectRegistry.StaticObjectRegistry");

FStaticObjectRegistry& FStaticObjectRegistry::Get()
{
	static FStaticObjectRegistry Singleton;
	return Singleton;
}

bool FStaticObjectRegistry::RegisterAsset(const FString& Type, const FString& Category, const FString& AssetPath)
{
	if (Type.IsEmpty() || Category.IsEmpty() || AssetPath.IsEmpty())
	{
		return false;
	}

	FString NormalizedType = Type.ToLower();
	FString NormalizedCategory = Category.ToLower();

	TMap<FString, FEntry>& CategoryMap = m_Entries.FindOrAdd(NormalizedType);

	FEntry Entry;
	Entry.Type = NormalizedType;
	Entry.Category = NormalizedCategory;
	Entry.SoftClass = TSoftClassPtr<AActor>(FSoftObjectPath(AssetPath));

	CategoryMap.Add(NormalizedCategory, Entry);
	return true;
}

bool FStaticObjectRegistry::RegisterClass(const FString& Type, const FString& Category, TSubclassOf<AActor> ActorClass)
{
	if (Type.IsEmpty() || Category.IsEmpty() || !ActorClass)
	{
		return false;
	}

	FString NormalizedType = Type.ToLower();
	FString NormalizedCategory = Category.ToLower();

	TMap<FString, FEntry>& CategoryMap = m_Entries.FindOrAdd(NormalizedType);

	FEntry Entry;
	Entry.Type = NormalizedType;
	Entry.Category = NormalizedCategory;
	Entry.LoadedClass = ActorClass;
	// 同时保存软引用路径，以便后续需要时使用
	if (ActorClass)
	{
		Entry.SoftClass = TSoftClassPtr<AActor>(ActorClass);
	}

	CategoryMap.Add(NormalizedCategory, Entry);
	return true;
}

void FStaticObjectRegistry::LoadFromDataAsset(UStaticObjectRegistryDataAsset* DataAsset)
{
	if (!DataAsset)
	{
		return;
	}

	for (const auto& Entry : DataAsset->Entries)
	{
		if (Entry.ActorClass)
		{
			RegisterClass(Entry.Type, Entry.Category, Entry.ActorClass);
		}
	}

	m_bDataAssetLoaded = true;
}

void FStaticObjectRegistry::LoadFromDataAssetPath(const FString& DataAssetPath)
{
	if (m_bDataAssetLoaded)
	{
		return;
	}

	FString PathToLoad = DataAssetPath.IsEmpty() ? DefaultDataAssetPath : DataAssetPath;
	UStaticObjectRegistryDataAsset* DataAsset = LoadObject<UStaticObjectRegistryDataAsset>(nullptr, *PathToLoad);
	
	if (DataAsset)
	{
		LoadFromDataAsset(DataAsset);
		UE_LOG(LogTemp, Log, TEXT("StaticObjectRegistry: Loaded DataAsset from %s"), *PathToLoad);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("StaticObjectRegistry: DataAsset not found at %s, using defaults only"), *PathToLoad);
	}
}

bool FStaticObjectRegistry::ResolveClass(const FString& Type, const FString& Category, TSubclassOf<AActor>& OutClass)
{
	EnsureDefaults();

	const FString NormalizedType = Type.ToLower();
	const FString NormalizedCategory = Category.ToLower();

	TMap<FString, FEntry>* CategoryMap = m_Entries.Find(NormalizedType);
	if (!CategoryMap)
	{
		return false;
	}

	FEntry* Entry = CategoryMap->Find(NormalizedCategory);
	if (!Entry)
	{
		return false;
	}

	// 如果已有直接加载的类，直接使用
	if (Entry->LoadedClass)
	{
		OutClass = Entry->LoadedClass;
		return true;
	}

	// 否则从软引用加载
	if (!Entry->SoftClass.IsValid())
	{
		Entry->SoftClass.LoadSynchronous();
	}

	if (!Entry->SoftClass.IsValid())
	{
		return false;
	}

	OutClass = Entry->SoftClass.Get();
	if (OutClass)
	{
		// 缓存已加载的类，避免下次再加载
		Entry->LoadedClass = OutClass;
	}
	return OutClass != nullptr;
}

void FStaticObjectRegistry::GetClassesByType(const FString& Type, TArray<TSubclassOf<AActor>>& OutClasses)
{
	EnsureDefaults();

	const FString NormalizedType = Type.ToLower();
	OutClasses.Reset();

	TMap<FString, FEntry>* CategoryMap = m_Entries.Find(NormalizedType);
	if (!CategoryMap)
	{
		return;
	}

	for (auto& Pair : *CategoryMap)
	{
		FEntry& Entry = Pair.Value;
		
		// 优先使用已加载的类
		if (Entry.LoadedClass)
		{
			OutClasses.Add(Entry.LoadedClass);
			continue;
		}

		// 否则从软引用加载
		if (!Entry.SoftClass.IsValid())
		{
			Entry.SoftClass.LoadSynchronous();
		}
		if (Entry.SoftClass.IsValid())
		{
			TSubclassOf<AActor> LoadedClass = Entry.SoftClass.Get();
			if (LoadedClass)
			{
				Entry.LoadedClass = LoadedClass;
				OutClasses.Add(LoadedClass);
			}
		}
	}
}

void FStaticObjectRegistry::EnsureDefaults()
{
	if (m_bDefaultsRegistered)
	{
		return;
	}

	// 首先尝试从 DataAsset 加载（编辑器配置优先）
	LoadFromDataAssetPath();

	// 如果 DataAsset 中没有注册，则使用默认的硬编码资源
	// 这些作为后备，确保即使没有配置 DataAsset 也能工作
	if (!m_bDataAssetLoaded || m_Entries.Num() == 0)
	{
		RegisterAsset(TEXT("effect"), TEXT("fire"), TEXT("/Game/Sevnce/CarVFX/BP_Fire.BP_Fire_C"));
		RegisterAsset(TEXT("effect"), TEXT("smoke01"), TEXT("/Game/Sevnce/CarVFX/BP_Smoke01.BP_Smoke01_C"));
		RegisterAsset(TEXT("effect"), TEXT("smoke02"), TEXT("/Game/Sevnce/CarVFX/BP_Smoke02.BP_Smoke02_C"));
		RegisterAsset(TEXT("effect"), TEXT("smoke03"), TEXT("/Game/Sevnce/CarVFX/BP_Smoke03.BP_Smoke03_C"));
	}

	m_bDefaultsRegistered = true;
}

