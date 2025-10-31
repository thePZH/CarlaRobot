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
	UPrimitiveComponent* Draw(const FVector& Center, const FVector& Scale, const FLinearColor& Color);

private:
	UPROPERTY()
	AActor* m_HostActor = nullptr;
	UPROPERTY()
	UMaterialInterface* m_CubeMaterial = nullptr;
	UPROPERTY()
	UStaticMesh* m_CubeMesh = nullptr;
};
