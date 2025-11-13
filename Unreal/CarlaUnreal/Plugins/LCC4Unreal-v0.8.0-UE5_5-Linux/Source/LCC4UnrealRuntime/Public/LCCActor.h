/*********************************************
 * Copyright: XGrids Corporation
 * Author   : Feng HaiLiang
 * ******************************************/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LCCActor.generated.h"

/**
 * Adds support for importing, processing and rendering of LCC(XGrids) format 3D Gaussian Splatting.
 */
UCLASS(BlueprintType, hidecategories = (Materials,Physics,Collision),ConversionRoot,ComponentWrapperClass)
class LCC4UNREALRUNTIME_API ALCCActor : public AActor
{
    GENERATED_UCLASS_BODY()
    /**
     * Open a file dialog to select a lcc file
     */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Developer",
        meta=(DisplayName = "Load", DisplayPriority = 1))
    void SelectFile();

    /**
     * Unload the lcc file 
     */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Developer",
        meta=(DisplayName = "UnLoad", DisplayPriority = 2))
    void UnLoad();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Developer", meta=(DisplayPriority = 3))
    void Refresh();

    /** Open the Node visualization to visually node bound and level.
     * The color of NodeBox represents the node Level (red, orange, yellow, green, blue, purple)
     * red is the lowest level (highest detail), and white is the highest level (lowest detail)
     */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Developer",
        meta=(DisplayName = "Debug Node Bound", DisplayPriority = 4))
    void DebugNodeBound();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Developer", meta=(DisplayPriority = 5))
    void Stats();

    UPROPERTY(Category = "XGrids",
        VisibleAnywhere,
        BlueprintReadOnly,
        meta = (AllowPrivateAccess = "true", DisplayPriority = 1000))
    TObjectPtr<class ULCCComponent> LCCComponent;

    /** Returns LCCComponent **/
    UFUNCTION(BlueprintCallable, Category = "XGrids")
    ULCCComponent* GetLCCComponent() const { return LCCComponent; }

    /** Load lcc file*/
    UFUNCTION(BlueprintCallable, Category = "XGrids")
    void Load(const FString& String) const;

private:
#if WITH_EDITOR
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
};
