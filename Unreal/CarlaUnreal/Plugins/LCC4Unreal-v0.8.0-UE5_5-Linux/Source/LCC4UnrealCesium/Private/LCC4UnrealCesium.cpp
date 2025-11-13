// Copyright Epic Games, Inc. All Rights Reserved.

#include "LCC4UnrealCesium.h"

#define LOCTEXT_NAMESPACE "FLCC4UnrealCesiumModule"

void FLCC4UnrealCesiumModule::StartupModule()
{
}

void FLCC4UnrealCesiumModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLCC4UnrealCesiumModule, LCC4UnrealCesium);
