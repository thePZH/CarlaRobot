// Copyright (c) 2025 Computer Vision Center (CVC).
// MIT License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"

class AActor;
class UStaticObjectRegistryDataAsset;

/**
 * 轻量级的静态物体注册与类缓存管理器。
 * 支持按 type/category 注册资源路径，运行时按需加载并缓存 UClass。
 * 支持从 DataAsset 加载编辑器配置的注册表。
 * 当前用于 CarlaServer 的 create_object / destroy_objects.
 */
class FStaticObjectRegistry
{
public:
	static FStaticObjectRegistry& Get();

	/** 注册一个资源路径，type/category 自定义，path 为软引用类路径 */
	bool RegisterAsset(const FString& Type, const FString& Category, const FString& AssetPath);

	/** 注册一个蓝图类引用（直接使用已加载的类，用于从DataAsset加载） */
	bool RegisterClass(const FString& Type, const FString& Category, TSubclassOf<AActor> ActorClass);

	/** 从 DataAsset 加载注册表配置 */
	void LoadFromDataAsset(UStaticObjectRegistryDataAsset* DataAsset);

	/** 从指定路径加载 DataAsset 并注册（路径为空时使用默认路径） */
	void LoadFromDataAssetPath(const FString& DataAssetPath = TEXT(""));

	/** 获取指定 type/category 对应的已加载类（按需同步加载），失败返回 false */
	bool ResolveClass(const FString& Type, const FString& Category, TSubclassOf<AActor>& OutClass);

	/** 获取某个 type 下已注册的全部类（会确保加载）。不存在时返回空数组。 */
	void GetClassesByType(const FString& Type, TArray<TSubclassOf<AActor>>& OutClasses);

	/** 确保默认的内置资产完成注册（仅调用一次）。会尝试加载DataAsset。 */
	void EnsureDefaults();

private:
	FStaticObjectRegistry() = default;
	FStaticObjectRegistry(const FStaticObjectRegistry&) = delete;
	FStaticObjectRegistry& operator=(const FStaticObjectRegistry&) = delete;

	struct FEntry
	{
		FString Type;
		FString Category;
		TSoftClassPtr<AActor> SoftClass;
		/** 已加载的类引用（如果从DataAsset加载，直接使用） */
		TSubclassOf<AActor> LoadedClass;
	};

	/** key: Type -> (Category -> Entry) */
	TMap<FString, TMap<FString, FEntry>> m_Entries;

	bool m_bDefaultsRegistered = false;
	bool m_bDataAssetLoaded = false;

	/** 默认 DataAsset 路径 */
	static const FString DefaultDataAssetPath;
};

