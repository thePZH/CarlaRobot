#include "SvcGeometryDrawerManager.h"

#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"

#include "SvcPointDrawer.h"
#include "SvcLineDrawer.h"
#include "SvcCubeDrawer.h"

namespace
{
	static void DestroyComponentWithChildren(UPrimitiveComponent* inComponent)
	{
		if (!IsValid(inComponent))
		{
			return;
		}

		if (USceneComponent* sceneComponent = Cast<USceneComponent>(inComponent))
		{
			TArray<USceneComponent*> children;
			sceneComponent->GetChildrenComponents(true, children);
			for (USceneComponent* child : children)
			{
				if (IsValid(child))
				{
					if (UActorComponent* asActorComponent = Cast<UActorComponent>(child))
					{
						asActorComponent->DestroyComponent();
					}
				}
			}
		}

		inComponent->DestroyComponent(true);
	}
}

void USvcGeometryDrawerManager::InitializeManager(UWorld* in_world)
{
	if (!in_world)
	{
		UE_LOG(LogTemp, Error, TEXT("USvcGeometryDrawerManager::InitializeManager - InWorld is null"));
		return;
	}
	
	m_HookActor = in_world->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	m_HookActor->SetActorEnableCollision(false);
	// 确保有有效根组件，方便子组件 Attach
	if (!m_HookActor->GetRootComponent())
	{
		USceneComponent* Root = NewObject<USceneComponent>(m_HookActor);
		Root->RegisterComponent();
		m_HookActor->SetRootComponent(Root);
	}
	
	m_LineDrawer  = NewObject<USvcLineDrawer>(this, USvcLineDrawer::StaticClass());
	m_PointDrawer = NewObject<USvcPointDrawer>(this, USvcPointDrawer::StaticClass());
	m_CubeDrawer  = NewObject<USvcCubeDrawer>(this, USvcCubeDrawer::StaticClass());
	
	m_LineDrawer->Initialize(m_HookActor);
	m_PointDrawer->Initialize(m_HookActor);
	m_CubeDrawer->Initialize(m_HookActor);

	UE_LOG(LogTemp, Log, TEXT("[SvcGeometryDrawerManager] Initialized successfully"));
}

void USvcGeometryDrawerManager::DeinitializeManager()
{
	ClearDrawObjects(EGeometryDrawType::All);

	if (IsValid(m_HookActor))
	{
		m_HookActor->Destroy();
		m_HookActor = nullptr;
	}

	m_LineDrawer  = nullptr;
	m_PointDrawer = nullptr;
	m_CubeDrawer  = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[SvcGeometryDrawerManager] Deinitialized"));
}

FString USvcGeometryDrawerManager::DrawLine(const TArray<FVector>& positions, const FLinearColor& color, float Scale)
{
	if (!m_LineDrawer)
	{
		UE_LOG(LogTemp, Warning, TEXT("LineDrawer not initialized"));
		return TEXT("");
	}

	UPrimitiveComponent* comp = m_LineDrawer->Draw(positions, color, Scale);
	if (!comp) return TEXT("");

	const FString new_id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

	m_DrawObjects.Add(new_id, comp);
	m_TypeGroups.FindOrAdd(EGeometryDrawType::Line).IDs.Add(new_id);

	return new_id;
}

FString USvcGeometryDrawerManager::DrawPoint(const FVector& location, const FLinearColor& color, float Scale)
{
	if (!m_PointDrawer)
	{
		UE_LOG(LogTemp, Warning, TEXT("PointDrawer not initialized"));
		return TEXT("");
	}

	UPrimitiveComponent* comp = m_PointDrawer->Draw(location, color, Scale);
	if (!comp) return TEXT("");

	const FString new_id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

	m_DrawObjects.Add(new_id, comp);
	m_TypeGroups.FindOrAdd(EGeometryDrawType::Point).IDs.Add(new_id);

	return new_id;
}

FString USvcGeometryDrawerManager::DrawCube(const FVector& Center, const FVector& Scale, const FLinearColor& Color)
{
	if (!m_CubeDrawer)
	{
		UE_LOG(LogTemp, Warning, TEXT("CubeDrawer not initialized"));
		return TEXT("");
	}

	UPrimitiveComponent* comp = m_CubeDrawer->Draw(Center, Scale, Color);
	if (!comp) return TEXT("");

	const FString new_id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

	m_DrawObjects.Add(new_id, comp);
	m_TypeGroups.FindOrAdd(EGeometryDrawType::Cube).IDs.Add(new_id);

	return new_id;
}

bool USvcGeometryDrawerManager::RemoveDrawObject(const FString& object_id)
{
	if (UPrimitiveComponent* comp = m_DrawObjects.FindRef(object_id))
	{
		DestroyComponentWithChildren(comp);
		m_DrawObjects.Remove(object_id);

		for (auto& pair : m_TypeGroups)
		{
			pair.Value.IDs.Remove(object_id);
		}

		return true;
	}

	return false;
}

bool USvcGeometryDrawerManager::ClearDrawObjects(EGeometryDrawType type)
{
	if (type == EGeometryDrawType::All)
	{
		for (auto& pair : m_DrawObjects)
		{
			DestroyComponentWithChildren(pair.Value);
		}
		m_DrawObjects.Empty();
		m_TypeGroups.Empty();
		return true;
	}

	if (FIdArrayWrapper* wrapper = m_TypeGroups.Find(type))
	{
		TArray<FString> idsToRemove = wrapper->IDs;
		for (const FString& id : idsToRemove)
		{
			if (UPrimitiveComponent* comp = m_DrawObjects.FindRef(id))
			{
				DestroyComponentWithChildren(comp);
				m_DrawObjects.Remove(id);
			}
		}
		m_TypeGroups.Remove(type);
	}

	return true;
}
