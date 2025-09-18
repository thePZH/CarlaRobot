// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "CoreMinimal.h"

#include "GaugesManagerActor.generated.h"

UCLASS()
class CARLA_API AGaugesManagerActor : public AActor
{
	GENERATED_BODY()

public:
	AGaugesManagerActor();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintCallable)
	TArray<FTransform> GetGaugesTransform();
private:
	void UpdateGaugesInfo();

private:
	TArray<FTransform> m_GaugesTrans;
};
