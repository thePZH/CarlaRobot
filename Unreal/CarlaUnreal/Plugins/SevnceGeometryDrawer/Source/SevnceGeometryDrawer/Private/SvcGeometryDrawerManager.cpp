#include "SvcGeometryDrawerManager.h"

#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"

#include "SvcPointDrawer.h"
#include "SvcLineDrawer.h"
#include "SvcCubeDrawer.h"

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
		if (IsValid(comp))
		{
	      // 先销毁子组件（例如线段的 USplineMeshComponent 段）
	      if (USceneComponent* SceneComp = Cast<USceneComponent>(comp))
	      {
	        TArray<USceneComponent*> Children = SceneComp->GetAttachChildren();
	        for (USceneComponent* Child : Children)
	        {
	          if (IsValid(Child))
	          {
	            if (UActorComponent* AsActorComp = Cast<UActorComponent>(Child))
	            {
	              AsActorComp->DestroyComponent();
	            }
	          }
	        }
	      }
			comp->DestroyComponent();
		}
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
			if (IsValid(pair.Value))
			{
				pair.Value->DestroyComponent();
			}
		}
		m_DrawObjects.Empty();
		m_TypeGroups.Empty();
		return true;
	}

	if (FIdArrayWrapper* wrapper = m_TypeGroups.Find(type))
	{
		for (const FString& id : wrapper->IDs)
		{
			if (UPrimitiveComponent* comp = m_DrawObjects.FindRef(id))
			{
				if (IsValid(comp))
				{
					comp->DestroyComponent();
				}
				m_DrawObjects.Remove(id);
			}
		}
		m_TypeGroups.Remove(type);
	}

	return true;
}
