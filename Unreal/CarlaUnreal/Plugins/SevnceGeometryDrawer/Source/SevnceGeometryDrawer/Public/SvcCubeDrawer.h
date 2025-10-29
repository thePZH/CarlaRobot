#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SvcCubeDrawer.generated.h"

UCLASS()
class USvcCubeDrawer : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(AActor* InHostActor);
	UPrimitiveComponent* Draw(const FVector& Center, const FVector& Extent, const FLinearColor& Color);

private:
	AActor* m_HostActor = nullptr;
	UMaterialInterface* m_CubeMaterial = nullptr;
	UStaticMesh* m_CubeMesh = nullptr;
};
