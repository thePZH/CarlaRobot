#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SvcPointDrawer.generated.h"

UCLASS()
class USvcPointDrawer : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(AActor* InHostActor);
	UPrimitiveComponent* Draw(const FVector& Location, const FLinearColor& Color, float Size);

private:
	AActor* m_HostActor = nullptr;
	UMaterialInterface* m_Mat = nullptr;
	UStaticMesh* m_Mesh = nullptr;
};
