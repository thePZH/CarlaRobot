#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Components/SplineMeshComponent.h"
#include "SvcLineDrawer.generated.h"

UCLASS()
class USvcLineDrawer : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(AActor* InActor);

	UPrimitiveComponent* Draw(const TArray<FVector>& Positions, const FLinearColor& Color, float Thickness) const;

private:
	UPROPERTY()
	AActor* m_HookActor = nullptr;
	UPROPERTY()
	UMaterialInterface* m_LineMaterial = nullptr;
	UPROPERTY()
	UStaticMesh* m_LineMesh = nullptr;
};