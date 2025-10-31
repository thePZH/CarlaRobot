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

	// mesh直径为2cm
	UPrimitiveComponent* Draw(const TArray<FVector>& Positions, const FLinearColor& Color, float Scale) const;

private:
	UPROPERTY()
	AActor* m_HookActor = nullptr;
	UPROPERTY()
	UMaterialInterface* m_LineMaterial = nullptr;
	UPROPERTY()
	UStaticMesh* m_LineMesh = nullptr;
};