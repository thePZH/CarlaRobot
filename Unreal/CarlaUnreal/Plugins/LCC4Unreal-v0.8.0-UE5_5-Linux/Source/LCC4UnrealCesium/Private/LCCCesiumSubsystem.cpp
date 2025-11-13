/*********************************************
 * Copyright: XGrids Corporation
 * Author   : Feng HaiLiang
 * ******************************************/

#include "LCCCesiumSubsystem.h"
#if LCC_WITH_CESIUM
#include "CesiumGeoreference.h"
#endif

void ULCCCesiumSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void ULCCCesiumSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

bool ULCCCesiumSubsystem::RequestCesiumOrigin(double& Latitude, double& Longitude, double& Altitude)
{
#if LCC_WITH_CESIUM
    if (ACesiumGeoreference* CesiumGeoreference = ACesiumGeoreference::GetDefaultGeoreference(GetWorld()))
    {
        Latitude = CesiumGeoreference->GetOriginLatitude();
        Longitude = CesiumGeoreference->GetOriginLongitude();
        Altitude = CesiumGeoreference->GetOriginHeight();
        CesiumGeoreference->OnGeoreferenceUpdated.AddUniqueDynamic(this, &ULCCCesiumSubsystem::OnCesiumGeoreferenceUpdate);
    }
    return true;
#else
    return false;
#endif
}

void ULCCCesiumSubsystem::OnCesiumGeoreferenceUpdate()
{
    if (OnCesiumUpdate.IsBound())
    {
        OnCesiumUpdate.Broadcast();
    }
}
