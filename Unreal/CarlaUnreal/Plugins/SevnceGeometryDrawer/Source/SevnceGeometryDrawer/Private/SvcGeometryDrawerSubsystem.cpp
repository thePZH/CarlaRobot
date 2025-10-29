// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.


#include "SvcGeometryDrawerSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"

void USvcGeometryDrawerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	TWeakObjectPtr<USvcGeometryDrawerSubsystem> WeakThis(this);
	m_InitHandle = FWorldDelegates::OnPostWorldInitialization.AddLambda(
		[WeakThis](UWorld* World, const UWorld::InitializationValues)
		{
			if (!WeakThis.IsValid() || !World || (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE))
				return;
			
			USvcGeometryDrawerManager* NewDrawerManager = NewObject<USvcGeometryDrawerManager>();
			if (!NewDrawerManager)
				return;
			
			WeakThis->m_DrawerManager = NewDrawerManager;
			FWorldDelegates::OnPostWorldInitialization.Remove(WeakThis->m_InitHandle);
			WeakThis->m_DrawerManager->InitializeManager(World);
		}
	);
}

void USvcGeometryDrawerSubsystem::Deinitialize()
{
	if (m_DrawerManager)
	{
		m_DrawerManager->DeinitializeManager();
		m_DrawerManager = nullptr;
	}
	Super::Deinitialize();
}

FString USvcGeometryDrawerSubsystem::DrawLine(const TArray<FVector>& Positions, const FLinearColor& Color, float Thickness)
{
	return m_DrawerManager ? m_DrawerManager->DrawLine(Positions, Color, Thickness) : TEXT("");
}

FString USvcGeometryDrawerSubsystem::DrawPoint(const FVector& Location, const FLinearColor& Color, float Size)
{
	return m_DrawerManager ? m_DrawerManager->DrawPoint(Location, Color, Size) : TEXT("");
}

FString USvcGeometryDrawerSubsystem::DrawCube(const FVector& Center, const FVector& Extent, const FLinearColor& Color)
{
	return m_DrawerManager ? m_DrawerManager->DrawCube(Center, Extent, Color) : TEXT("");
}

bool USvcGeometryDrawerSubsystem::RemoveDrawObject(FString ObjectId)
{
	if (!m_DrawerManager)
		return false;
	
	return m_DrawerManager->RemoveDrawObject(ObjectId);
	
}

bool USvcGeometryDrawerSubsystem::ClearDrawObjects(EGeometryDrawType Type)
{
	if (!m_DrawerManager)
		return false;
	
	return m_DrawerManager->ClearDrawObjects(Type);
}