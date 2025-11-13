/*********************************************
 * Copyright: XGrids Corporation
 * Author   : Feng HaiLiang
 * ******************************************/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LCCDefinition.h"
#include "LCCUtilLibrary.generated.h"

class UTexture2D;
/**
 * Utility functions for LCC
 */
UCLASS()
class LCC4UNREALRUNTIME_API ULCCUtilLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Check path valid
     * @param Path Path
     * @return true if path valid, false otherwise
     * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static bool CheckPathValid(FString Path);

    /**
     * Check LCC valid
     * @param Path LCC path
     * @param OutWorkPath Work path
     * @return true if LCC valid, false otherwise
     * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static bool CheckLCCValid(FString Path, FString& OutWorkPath);

    /**
    * Determine the file format of the file
    * @param Path File path
    * @return EFileFormat File format
    * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static EFileFormat DetermineFileFormat(const FString& Path);

    /**
    * Determine the source type of the file
    * @param Path File path
    * @return ESourceType Source type
    * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static ELCCSourceType DetermineSourceType(const FString& Path);

    /**
     * Determine the collision type of the file
     * @param Path File path
     * @return ECollisionType Collision type
     * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static ECollisionType DetermineCollisionType(const FString& Path);

    /**
     * Convert meta info to struct 
     * @param Json string
     * @return MetaInfo Meta info
     * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static FMetaInfo ConvertStrToMetaInfo(const FString& JsonStr);

    /** Open a select file dialog*/
    static FString SelectFile();

    /*
     * Get the version of LCC4Unreal
     * @return LCC4Unreal version
     * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static FString GetLCC4UnrealVersion();

    /**
     * Get the unique id of player controller
     * @return Player controller unique id
     * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static bool GetPlayerUniqueID(class APlayerController* PlayerController, int32& OutUniqueID);

    /**
     * Get the unique id of scene capture component
     * @return Scene capture component unique id
     * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static bool GetSceneCaptureUniqueID(class USceneCaptureComponent2D* SceneCaptureComponent2D, int32& OutUniqueID);

    static FConvexVolume CalculateConvexVolume(const FVector& Location, const FRotator& Rotation, float FOV,
                                               float AspectRatio); 

    /**
     * Get the project id
     * @return Project id
     * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static FString GetProjectId();

    /**
     * Copy string to clipboard
     * @param CopyString The string to copy
     * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static void CopyToClipboard(FString CopyString);

    /**
     * Get string from clipboard
     * @return The string from clipboard
     * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static FString GetClipboardString();

    /**
     * Get locale
     * @return The locale
     * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static ELocale GetLocale();

    /**
     * Get texture from base64 string
     * @param Base64String The base64 string
     * @return The texture
     * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static UTexture2D* GetTextureFromBase64(const FString& Base64String);

    /**
     * Get LCC config path
     * @return The LCC config path
     * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static FString GetLCCConfigPath();
    /**
     * Get LCC4Unreal root path
     * @return The LCC4Unreal root path
     * */
    UFUNCTION(BlueprintCallable, Category = "XGrids|Util")
    static FString GetLCC4UnrealRootPath();
};


