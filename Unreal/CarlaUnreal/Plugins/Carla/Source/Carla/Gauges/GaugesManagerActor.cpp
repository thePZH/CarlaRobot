// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.


#include "GaugesManagerActor.h"


AGaugesManagerActor::AGaugesManagerActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGaugesManagerActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGaugesManagerActor::BeginPlay()
{
	Super::BeginPlay();
	UpdateGaugesInfo();
}

TArray<FTransform> AGaugesManagerActor::GetGaugesTransform()
{
	return m_GaugesTrans;
}

void AGaugesManagerActor::UpdateGaugesInfo()
{
	TArray<AActor*> outActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("gauge"), outActors);
	if (outActors.IsEmpty())
		return;
	
	for (const auto& actor : outActors)
	{
		if (!actor)
			continue;
		m_GaugesTrans.Emplace(actor->GetTransform());
	}
}


