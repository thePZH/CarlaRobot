/*********************************************
 * Copyright: XGrids Corporation
 * Author   : Feng HaiLiang
 * ******************************************/

#pragma once

#include "CoreMinimal.h"
#include "LCCDefinition.h"
#include "GameFramework/Volume.h"
#include "LCCClippingVolume.generated.h"

class ULCCComponent;

/**
 * Clipping volume for LCC scene
 * You can use box or sphere volume to clip LCC scene
 */
UCLASS(
    HideCategories=("Collision", "Replication", "Input", "Cooking","WorldPartition",
        "Lighting", "Lighting", "LOD", "HLOD","Navigation","DataLayers","Networking","LevelInstance","BrushSettings"),
    meta=(DisplayName="LCC Clipping Volume"))
class LCC4UNREALRUNTIME_API ALCCClippingVolume : public AVolume
{
    GENERATED_UCLASS_BODY()
    /*
         * If enabled, this volume will be used to clip LCC scene
         */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XGrids")
    bool bEnabled;

    /*
    * Control whether the Clipping volume cuts inside or outside 
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XGrids")
    EClipType Mode;

    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XGrids")
    int32 Priority;

    /*
    * Control whether the Clipping volume is a box or a sphere
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XGrids")
    EClipVolumeType VolumeType;

    void SetUpdateComponent(ULCCComponent* Component);

    UFUNCTION(BlueprintCallable, Category = "XGrids")
    void Refresh();

private:
#if WITH_EDITOR
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual bool CanEditChange(const FProperty* InProperty) const override;
    virtual void EditorApplyRotation(const FRotator& DeltaRotation,
                                     bool bAltDown,
                                     bool bShiftDown,
                                     bool bCtrlDown) override;

    virtual void EditorApplyScale(const FVector& DeltaScale,
                                  const FVector* PivotLocation,
                                  bool bAltDown,
                                  bool bShiftDown,
                                  bool bCtrlDown) override;

    virtual void EditorApplyTranslation(const FVector& DeltaTranslation,
                                        bool bAltDown,
                                        bool bShiftDown,
                                        bool bCtrlDown) override;

    void RefreshBuilder();
#endif

    virtual void PostActorCreated() override;
    virtual void Destroyed() override;
    void UpdateRenderProxy() const;

    TObjectPtr<ULCCComponent> LCCComponent;
};
