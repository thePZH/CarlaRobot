/*********************************************
 * Copyright: XGrids Corporation
 * Author   : Feng HaiLiang
 * ******************************************/

#pragma once

#include "CoreMinimal.h"
#include "LCCDefinition.h"
#include "GameFramework/Volume.h"
#include "LCCSectionPlane.generated.h"

class UArrowComponent;
class ULCCComponent;

/**
 * Section planes for lcc 
 */
UCLASS(
    HideCategories=("Collision", "Replication", "Input", "Tags", "Cooking",
        "Lighting", "Rendering", "Lighting", "LOD", "HLOD","Navigation","DataLayers","Networking","LevelInstance",
        "BrushSettings","WorldPartition"),
    meta=(DisplayName="LCC Section Plane"))
class LCC4UNREALRUNTIME_API ALCCSectionPlane : public AVolume
{
    GENERATED_UCLASS_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XGrids")
    bool bEnabled;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XGrids")
    ESectionType Mode;

    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XGrids")
    int32 Priority;

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
#endif

    virtual void Destroyed() override;
    void UpdateRenderProxy() const;

#if WITH_EDITORONLY_DATA
    /* Reference to the editor only arrow visualization component */
    UPROPERTY()
    TObjectPtr<UArrowComponent> ArrowComponent;
#endif

    TObjectPtr<ULCCComponent> LCCComponent;
};
