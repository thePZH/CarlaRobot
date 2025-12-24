// Copyright (c) 2025 Computer Vision Center (CVC).
// MIT License.

#include "Carla/StaticObjects/StaticObjectRegistry.h"
#include "Carla/StaticObjects/StaticObjectRegistryDataAsset.h"

#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"
#include "Logging/LogMacros.h"

const FString FStaticObjectRegistry::DefaultDataAssetPath = TEXT("/Script/Carla.StaticObjectRegistryDataAsset'/Game/Sevnce/DA_CreatorsRegister.DA_CreatorsRegister'");

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
			RegisterClass(Entry.Type, Entry.Name, Entry.ActorClass);
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
	
	// 清理路径，只要包部分
	// 输入可能是：/Script/Carla.StaticObjectRegistryDataAsset'/Game/Sevnce/DA_CreatorsRegister.DA_CreatorsRegister'
	FString PackagePath = PathToLoad;

	// 去掉开头的类名引用部分（如果有）
	int32 QuoteIdx = PackagePath.Find(TEXT("'"));
	if (QuoteIdx != INDEX_NONE)
	{
		PackagePath = PackagePath.Mid(QuoteIdx + 1);
	}
	// 去掉结尾的单引号（如果有）
	PackagePath.RemoveFromEnd(TEXT("'"));
	// 去掉结尾的对象名（.DA_CreatorsRegister），只保留包路径
	int32 DotIdx = PackagePath.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (DotIdx != INDEX_NONE)
	{
		PackagePath = PackagePath.Left(DotIdx);
	}

	//Game/Sevnce/DA_CreatorsRegister

	// 强制加载所在的包
	// 如果包没在内存里，StaticFindObject 永远返回 nullptr
	UPackage* Package = LoadPackage(nullptr, *PackagePath, LOAD_None);
	if (!Package)
	{
		UE_LOG(LogTemp, Warning, TEXT("StaticObjectRegistry: Failed to load package at %s"), *PackagePath);
		return;
	}
	if (Package)
    {
        UE_LOG(LogTemp, Warning, TEXT("--- Debug: Dumping Package Contents ---"));
        for (TObjectIterator<UObject> It; It; ++It)
        {
            if (It->GetPackage() == Package)
            {
                UE_LOG(LogTemp, Warning, TEXT("Found Object: %s (Class: %s)"), *It->GetName(), *It->GetClass()->GetName());
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("----------------------------------------"));
    }
	// 在包里，只需要对象的名字
	// 原始字符串: ...'...DA_CreatorsRegister.DA_CreatorsRegister'
	FString ObjectName = PathToLoad;
	// 去掉前面的垃圾字符
	int32 LastDot = ObjectName.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (LastDot != INDEX_NONE)
	{
		ObjectName = ObjectName.Mid(LastDot + 1);
	}
	// 去掉最后的单引号
	ObjectName.RemoveFromEnd(TEXT("'"));

	UE_LOG(LogTemp, Warning, TEXT("Looking for object name: %s in package: %s"), *ObjectName, *PackagePath);

	// 直接在包里找：指定 InOuter=Package，只查名字
	UObject* LoadedObj = StaticFindObject(UStaticObjectRegistryDataAsset::StaticClass(), Package, *ObjectName);

	if (LoadedObj)
	{
		UStaticObjectRegistryDataAsset* DataAsset = Cast<UStaticObjectRegistryDataAsset>(LoadedObj);
		if (DataAsset)
		{
			LoadFromDataAsset(DataAsset);
			m_bDataAssetLoaded = true;
			UE_LOG(LogTemp, Log, TEXT("StaticObjectRegistry: Successfully loaded DataAsset %s"), *ObjectName);
			return;
		}
	}

	UE_LOG(LogTemp, Error, TEXT("StaticObjectRegistry: Package loaded, but object '%s' not found!"), *ObjectName);

}

bool FStaticObjectRegistry::ResolveClass(const FString& Type, const FString& Category, TSubclassOf<AActor>& OutClass)
{
	LoadAssetDataOnce();

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
	LoadAssetDataOnce();

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

void FStaticObjectRegistry::LoadAssetDataOnce()
{
	if (m_bDefaultsRegistered)
	{
		return;
	}

	LoadFromDataAssetPath();
}

