// Copyright (c) 2025 Computer Vision Center (CVC).
// MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StaticObjectRegistryDataAsset.generated.h"

/**
 * 静态物体注册条目：定义 type/category 与对应的蓝图类
 */
USTRUCT(BlueprintType)
struct CARLA_API FStaticObjectRegistryEntry
{
	GENERATED_BODY()

	/** 物体类型（如 "effect", "prop" 等） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Object Registry")
	FString Type;

	/** 类别名称（如 "fire", "smoke01" 等） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Object Registry")
	FString Category;

	/** 蓝图类引用（可直接在编辑器中拖拽选择） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Object Registry")
	TSubclassOf<AActor> ActorClass;

	FStaticObjectRegistryEntry()
		: Type(TEXT("effect"))
		, Category(TEXT(""))
		, ActorClass(nullptr)
	{
	}
};

/**
 * 静态物体注册表 DataAsset
 *
 * 使用方法：
 * 1. 在 Content Browser 中右键 -> Miscellaneous -> Data Asset
 * 2. 选择 "Static Object Registry Data Asset"
 * 3. 在编辑器中添加条目，设置 Type/Category/ActorClass
 * 4. 保存 DataAsset
 * 5. 默认路径：/Game/Carla/StaticObjectRegistry.StaticObjectRegistry
 */
UCLASS(BlueprintType)
class CARLA_API UStaticObjectRegistryDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 注册条目列表 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Static Object Registry", meta = (TitleProperty = "Category"))
	TArray<FStaticObjectRegistryEntry> Entries;

	UStaticObjectRegistryDataAsset()
	{
	}
};
 
