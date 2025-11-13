/*********************************************
 * Copyright: XGrids Corporation
 * Author   : Feng HaiLiang
 * ******************************************/

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "Camera/CameraTypes.h"
#include "LCCDefinition.h"
#include "ConvexVolume.h"
#include "LCCStructure.generated.h"

USTRUCT(BlueprintType)
struct FRenderInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "XGrids", meta = (InlineEditConditionToggle))
    bool bDistanceLimit;

    /**
     * The maximum distance(Meter) for rendering, any exceeding will be discarded.
     */
    UPROPERTY(EditAnywhere, Category = "XGrids",
        meta = (EditCondition="bDistanceLimit", DisplayName = "Max Distance(m)"))
    uint32 MaxDistance;

    UPROPERTY(EditAnywhere, Category = "XGrids", meta = (InlineEditConditionToggle))
    bool bMaxSplatNumLimit;
    /**
     * The maximum number of splats will be rendered, others will be discarded.
     */
    UPROPERTY(EditAnywhere, Category = "XGrids",
        meta = (EditCondition="bMaxSplatNumLimit", DisplayName="Max Splat Num(Ten Thousand)"))
    uint32 MaxSplatNum;

    UPROPERTY(EditAnywhere, Category = "XGrids", meta = (InlineEditConditionToggle))
    bool bCanSetLevelFactor;

    /**
     * Divide the default RangeForLevel(ProjectSetting/Plugin/LCC4Unreal) by a value to enlarge or reduce.
     * RangeForLevel determines the node level when rendering.
     * The larger this value, the fewer details, and the better the performance
     * It is recommended to keep this value at 1, or you can increase it for better performance 
     */
    UPROPERTY(EditAnywhere, Category = "XGrids",
        meta = (EditCondition="bCanSetLevelFactor", UIMin="0.01", ClampMin = "0.01", UIMax= "20", ClampMax = "20"))
    float LevelFactor;

    UPROPERTY(EditAnywhere, Category = "XGrids", meta = (InlineEditConditionToggle))
    bool bCanSetSortFactor;

    /**
    * Multiply the default SortFrequencyForLevel(ProjectSetting/Plugin/LCC4Unreal) by a value to enlarge or reduce.
    * SortFrequencyForLevel determines the node level's sort frequency when rendering.
    * The larger this value, the lower sort number, and the better the performance
    * It is recommended to keep this value at 1, or you can increase it for better performance 
    */
    UPROPERTY(EditAnywhere, Category = "XGrids",
        meta = (EditCondition="bCanSetSortFactor", UIMin="0.2", ClampMin = "0.2", UIMax= "5", ClampMax = "5"))
    float SortFactor;

    UPROPERTY(EditAnywhere, Category = "XGrids", meta = (InlineEditConditionToggle))
    bool bPreloadDistanceLimit;

    /**
     * The distance(Meter) from the camera to the node, the node will be preloaded if the distance is less than this value.
     */
    // UPROPERTY(EditAnywhere, Category = "XGrids",
    //     meta = (EditCondition="bPreloadDistanceLimit", ClampMin = "0.01", DisplayName="Preload Distance(m)"))
    uint32 PreloadDistance;

    UPROPERTY(EditAnywhere, Category = "XGrids", meta = (InlineEditConditionToggle))
    bool bCanSetStartLevel;

    /**
     * Determines the starting level of the LCC rendering node.
     * Default start from level 0 (more details).
     * However, some devices have limited performance and may need to start from a higher level (less details).
     */
    UPROPERTY(EditAnywhere, Category = "XGrids",
        meta=(EditCondition="bCanSetStartLevel", SliderExponent="1", UIMin="0", ClampMin="0", UIMax="20", ClampMax=
            "20"))
    uint32 StartLevel;

    UPROPERTY(EditAnywhere, Category = "XGrids", meta = (InlineEditConditionToggle))
    bool bCanSetEndLevel;

    /**
     * Determines the end level of the LCC rendering node.
     */
    UPROPERTY(EditAnywhere, Category = "XGrids",
        meta=(EditCondition="bCanSetEndLevel", SliderExponent="1", UIMin="0", ClampMin="0", UIMax="20", ClampMax=
            "20"))
    uint32 EndLevel;

    UPROPERTY(EditAnywhere, Category = "XGrids", meta = (InlineEditConditionToggle))
    bool bCollisionDistanceLimit;

    /**
     * The maximum distance(Meter) for load collision, any exceeding will not load.
     */
    UPROPERTY(EditAnywhere, Category = "XGrids",
        meta = (EditCondition="bCollisionDistanceLimit", DisplayName = "Max Load Collision Distance(m)"))
    uint32 CollisionLoadMaxDistance;

    UPROPERTY(EditAnywhere, Category = "XGrids", meta = (InlineEditConditionToggle))
    bool bEnableExtraPreNodes;

    /**
     * If this is enabled, the LCC will add extra preload nodes to the queue.
     * This can help solve the problem of screen edge holes caused by rapid rotation.
     * However, it may also increase the number of nodes that need to be rendered.
     * */
    UPROPERTY(EditAnywhere, Category = "XGrids", DisplayName="Add Extra Preload Nodes",
        meta=(EditCondition="bEnableExtraPreNodes"))
    bool bAddExtraPreNodes;

    UPROPERTY(EditAnywhere, Category = "XGrids", meta = (InlineEditConditionToggle))
    bool bCanSetUseFullLoad;

    /**
    * If this is enabled, the LCC will use full load mode to render if Level 0 splat num is less than FullLoadSplatNum.
    * */
    UPROPERTY(EditAnywhere, Category = "XGrids", meta = (EditCondition="bCanSetUseFullLoad"))
    bool bUseFullLoad;

    UPROPERTY(EditAnywhere, Category = "XGrids", meta = (InlineEditConditionToggle))
    bool bCanSetFullLoadNum;

    /**
     * If Level 0 splat num is less than this value, the LCC will use full load mode to render.
     * This requires bUseFullLoad to be enabled.
     * */
    UPROPERTY(EditAnywhere, Category = "XGrids", DisplayName="Full Load Splat Number",
        meta=(EditCondition="bCanSetFullLoadNum"))
    uint32 FullLoadSplatNum;

    FRenderInfo()
        : bDistanceLimit(false)
          , MaxDistance(LCC_MAX_DISTANCE)
          , bMaxSplatNumLimit(false)
          , MaxSplatNum(LCC_MAX_SPLAT)
          , bCanSetLevelFactor(false)
          , LevelFactor(1)
          , bCanSetSortFactor(false)
          , SortFactor(1)
          , bPreloadDistanceLimit(true)
          , PreloadDistance(200)
          , bCanSetStartLevel(false)
          , StartLevel(0)
          , bCanSetEndLevel(false)
          , EndLevel(20)
          , bCollisionDistanceLimit(false)
          , CollisionLoadMaxDistance(LCC_MAX_COLLISION_DISTANCE)
          , bEnableExtraPreNodes(false)
          , bAddExtraPreNodes(LCC_ADD_EXTRA_PRELOAD)
          , bCanSetUseFullLoad(false)
          , bUseFullLoad(true)
          , bCanSetFullLoadNum(false)
          , FullLoadSplatNum(LCC_FULL_LOAD_Num)
    {
    }

    FORCEINLINE bool operator==(const FRenderInfo& Other) const
    {
        return MaxDistance == Other.MaxDistance
            && MaxSplatNum == Other.MaxSplatNum
            && LevelFactor == Other.LevelFactor
            && SortFactor == Other.SortFactor
            && PreloadDistance == Other.PreloadDistance
            && StartLevel == Other.StartLevel
            && EndLevel == Other.EndLevel
            && bDistanceLimit == Other.bDistanceLimit
            && bMaxSplatNumLimit == Other.bMaxSplatNumLimit
            && bCanSetLevelFactor == Other.bCanSetLevelFactor
            && bCanSetSortFactor == Other.bCanSetSortFactor
            && bPreloadDistanceLimit == Other.bPreloadDistanceLimit
            && bCanSetStartLevel == Other.bCanSetStartLevel
            && bCanSetEndLevel == Other.bCanSetEndLevel
            && bEnableExtraPreNodes == Other.bEnableExtraPreNodes
            && bAddExtraPreNodes == Other.bAddExtraPreNodes;
    }

    FORCEINLINE bool operator!=(const FRenderInfo& Other) const
    {
        return !(*this == Other);
    }

    /** if there has max distance render limit.*/
    FORCEINLINE bool HasMaxDistanceLimit() const
    {
        return bDistanceLimit;
    }

    FORCEINLINE bool NeedPreload() const
    {
        return bPreloadDistanceLimit;
    }

    FORCEINLINE bool HasMaxNumLimit() const
    {
        return bMaxSplatNumLimit;
    }

    FORCEINLINE uint32 GetMaxDistance() const
    {
        return bDistanceLimit ? MaxDistance : LCC_MAX_DISTANCE;
    }

    FORCEINLINE uint32 GetMaxSplatNum() const
    {
        return bMaxSplatNumLimit ? MaxSplatNum : LCC_MAX_SPLAT;
    }

    FORCEINLINE float GetLevelFactor() const
    {
        return bCanSetLevelFactor ? LevelFactor : 1;
    }

    FORCEINLINE float GetSortFactor() const
    {
        return bCanSetSortFactor ? SortFactor : 1;
    }

    FORCEINLINE uint32 GetStartLevel() const
    {
        return bCanSetStartLevel ? StartLevel : 0;
    }

    FORCEINLINE uint32 GetEndLevel() const
    {
        return bCanSetEndLevel ? EndLevel : 20;
    }

    FORCEINLINE uint32 GetMaxCollisionDistance() const
    {
        return bCollisionDistanceLimit ? CollisionLoadMaxDistance : LCC_MAX_COLLISION_DISTANCE;
    }

    void Reset()
    {
        MaxDistance = LCC_MAX_DISTANCE;
        MaxSplatNum = LCC_MAX_SPLAT;
        CollisionLoadMaxDistance = LCC_MAX_COLLISION_DISTANCE;
        LevelFactor = 1;
        SortFactor = 1;
        PreloadDistance = 200;
        StartLevel = 0;
        EndLevel = 20;
        bAddExtraPreNodes = LCC_ADD_EXTRA_PRELOAD;
    }

    FORCEINLINE uint32 GetPreloadDistance() const
    {
        //return bPreloadDistanceLimit ? PreloadDistance : GetMaxDistance();
        return GetMaxDistance() + 35;
    }

    FORCEINLINE bool GetIfAddExtraPreNodes() const
    {
        if (!bEnableExtraPreNodes)
        {
            return LCC_ADD_EXTRA_PRELOAD;
        }
        return bAddExtraPreNodes;
    }

    FORCEINLINE bool GetIfUseFullLoad(const uint32 SplatNum) const
    {
        const bool bEnableFullLoad = !bCanSetUseFullLoad || (bCanSetUseFullLoad && bUseFullLoad);
        const uint32 FullLoadNum = bCanSetFullLoadNum ? FullLoadSplatNum : LCC_FULL_LOAD_Num;
        return bEnableFullLoad && SplatNum <= FullLoadNum;
    }
};

USTRUCT(BlueprintType)
struct FBoundingBoxInfo
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    TArray<float> Min;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    TArray<float> Max;
};

USTRUCT(BlueprintType)
struct FBoundingBoxInfoWithName
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    FString Name;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    FBoundingBoxInfo BoundingBoxInfo;

    FORCEINLINE bool operator==(const FBoundingBoxInfoWithName& InBound) const
    {
        return Name.Equals(InBound.Name);
    }
};

USTRUCT(BlueprintType)
struct FMetaInfo
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    FString Name;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    FString Version;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    FString Description;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    FString Source;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    FString DataType;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    FString FileType;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    TArray<float> Offset;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    TArray<float> Shift;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    int32 Epsg;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    int TotalLevel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    int TotalSplats;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    float CellLengthX;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    float CellLengthY;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    TArray<int> Splats;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    TArray<FBoundingBoxInfoWithName> Attributes;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    FBoundingBoxInfo BoundingBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    int IndexDataSize;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    FString GUID;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    FString Encoding;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "XGrids")
    TArray<int> Scale;

    FMetaInfo()
        : Shift{0, 0, 0}
          , Epsg(0)
          , TotalLevel(0)
          , TotalSplats(0)
          , CellLengthX(0)
          , CellLengthY(0)
          , IndexDataSize(0)
    {
    }

    FORCEINLINE bool IsValid() const
    {
        return TotalLevel > 0 && TotalSplats > 0 && CellLengthX > 0 && CellLengthY > 0 && IndexDataSize > 0;
    }

    FORCEINLINE void Reset()
    {
        TotalLevel = 0;
        TotalSplats = 0;
        CellLengthX = 0;
        CellLengthY = 0;
    }

    FVector3f GetScale() { return FVector3f(Scale[0], Scale[1], Scale[2]); }
    FVector2f GetCellLength() const;
    FBox GetVisibleBoundingBox() const;
    int32 GetXNum() const;
    int32 GetYNum() const;
    FBox GetEnvironmentVisibleBox() const;
    FBox GetScaleBox() const;
    FBox GetEnvironmentScaleBox() const;
    FBox GetShcoef() const;
    FBox GetEnvShcoef() const;
    FVector2f GetOpacity() const;
    FBox GetNormal() const;
    FBox GetEnvNormal() const;
    FBox GetColor() const;
    EFileType GetFileType() const;
    float GetVersion() const;

    //get splat num of level 0,unit:tex thousand
    FORCEINLINE uint32 GetLevel0SplatNum() const
    {
        if (Splats.IsValidIndex(0))
        {
            return Splats[0] / 10000;
        }
        return 0;
    }

    FORCEINLINE bool IsRTK() const
    {
        return Epsg > 0 && GetOffset() != FVector::ZeroVector;
    }

    FORCEINLINE FVector GetOffset() const
    {
        if (Offset.Num() != 3)
        {
            LCC_WARNING("MetaInfo's Offset has no 3 elements!");
            return FVector::ZeroVector;
        }
        return FVector(Offset[0], Offset[1], Offset[2]);
    }

private:
    const FBoundingBoxInfoWithName* GetAttributeInfoWithName(FString InName) const;
    static FBox GetBoxFromBoundingInfo(const FBoundingBoxInfo& Bound, bool NeedScale = false);
};

struct FCameraInfo
{
    //view index,not equals to ViewStart->UniqueID,because unique id if not always start zero
    uint8 CameraIndex;
    int32 CameraUniqueID;
    //camera index map,key is view state unique id,value is camera index
    TMap<int32, int32> CameraIndexMap;
    //total camera num 
    uint32 CameraNum;
    //world location
    FVector Location;
    //prev world location
    TMap<int32, FVector> PreLocationMap;
    TMap<int32, FVector> LocationMap;
    //world rotation
    FRotator Rotation;
    float FOV;
    float AspectRatio;
    FTransform ComponentToWorld;
    FTransform ComponentToWorld_RTK;
    FConvexVolume ViewFrustum;
    //if camera info change,calculate by LCCManager
    TArray<bool> bCameraChanged;
    //whether lcc is use RTK
    bool bUseRTK;
    FMatrix RTKMatrix;

    FCameraInfo()
    {
        Reset();
    }

    FVector GetWorldLocation() const
    {
        return Location;
    }

    FRotator GetWorldRotation() const
    {
        return Rotation;
    }

    FVector GetLocalLocation() const
    {
        return ComponentToWorld.InverseTransformPosition(Location);
    }

    //world location
    FVector GetCameraLocationByIndex(const int32& InCameraIndex) const
    {
        return LocationMap.FindRef(InCameraIndex);
    }

    //world location
    FVector GetPrevCameraLocationByIndex(const int32& InCameraIndex) const
    {
        return PreLocationMap.FindRef(InCameraIndex);
    }

    FVector GetRTKLocalLocation() const
    {
        return ComponentToWorld_RTK.InverseTransformPosition(Location);
    }

    FQuat GetLocalRotation() const
    {
        return ComponentToWorld.InverseTransformRotation(Rotation.Quaternion());
    }

    FORCEINLINE bool operator==(const FCameraInfo& Other) const
    {
        return Location.Equals(Other.Location, LCC_SMALL_NUMBER)
            && Rotation.Equals(Other.Rotation,UE_SMALL_NUMBER)
            && FMath::Abs(FOV - Other.FOV) < UE_SMALL_NUMBER
            && FMath::Abs(AspectRatio - Other.AspectRatio) < UE_SMALL_NUMBER
            && ComponentToWorld.Equals(Other.ComponentToWorld, LCC_SMALL_NUMBER);
    }

    FORCEINLINE bool operator!=(const FCameraInfo& Other) const
    {
        return !(*this == Other);
    }

    FORCEINLINE void Reset()
    {
        Location = FVector::ZeroVector;
        Rotation = FRotator::ZeroRotator;
        FOV = 0.0f;
        AspectRatio = 0.0f;
        ComponentToWorld = FTransform::Identity;
        CameraIndex = 0;
        CameraIndexMap.Empty();
        CameraNum = 0;
        bCameraChanged.Empty();
        CameraUniqueID = 0;
        bUseRTK = false;
    }

    FORCEINLINE bool IsValid() const
    {
        return FOV > 0 || AspectRatio > 0 || Location != FVector::ZeroVector || Rotation != FRotator::ZeroRotator;
    }

    FORCEINLINE int32 GetCameraIndexFromUniqueID(const int32 UniqueID) const
    {
        const int32* Ret = CameraIndexMap.Find(UniqueID);
        return Ret ? *Ret : INDEX_NONE;
    }

    FORCEINLINE int32 GetCameraIndexFromUniqueID(const uint32 UniqueID) const
    {
        const int32 Id = static_cast<int32>(UniqueID);
        return GetCameraIndexFromUniqueID(Id);
    }

    FORCEINLINE int32 GetUniqueIDFromCameraIndex(const int32 Index) const
    {
        const int32* Ret = CameraIndexMap.FindKey(Index);
        return Ret ? *Ret : INDEX_NONE;
    }

    /**
    * If it is first camera when multi viewport 
    * @return If first camera
    */
    FORCEINLINE bool IsFirstCamera() const
    {
        return CameraIndex == 0;
    }

    /**
    * If it is last camera when multi viewport
    * @return If last camera
    */
    FORCEINLINE bool IsLastCamera() const
    {
        return CameraIndex == CameraNum - 1;
    }

    FORCEINLINE bool IfCameraChanged() const
    {
        if (bCameraChanged.IsValidIndex(CameraIndex))
        {
            return bCameraChanged[CameraIndex];
        }
        return true;
    }

    FORCEINLINE void SetCameraChanged(const bool InCameraChanged)
    {
        if (bCameraChanged.IsValidIndex(CameraIndex))
        {
            bCameraChanged[CameraIndex] = InCameraChanged;
        }
    }

    FORCEINLINE bool IfAnyCameraChanged() const
    {
        for (const auto Changed : bCameraChanged)
        {
            if (Changed)
            {
                return true;
            }
        }
        return false;
    }
};

USTRUCT(BlueprintType)
struct FLCCVector
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XGrids")
    int32 X;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XGrids")
    int32 Y;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XGrids")
    int32 Level;
    //use fo
    bool bModify;
    bool bRenderNow;
    //every node has it's camera info
    FCameraInfo CameraInfo;
    //Camera visible map
    uint32 VisibilityMap;
    //prelaod?
    bool bPreload;

    FLCCVector()
        : X(0)
          , Y(0)
          , Level(0)
          , bModify(false)
          , bRenderNow(false)
          , VisibilityMap(0)
          , bPreload(false)
    {
    }

    FLCCVector(const int32 InX, const int32 InY, const int32 InLevel)
        : X(FMath::Max(InX, 0))
          , Y(FMath::Max(InY, 0))
          , Level(FMath::Max(InLevel, 0))
          , bModify(false)
          , bRenderNow(false)
          , VisibilityMap(0)
          , bPreload(false)
    {
    }

    FLCCVector(const int32 InX, const int32 InY, const int32 InLevel, const bool InRenderNow)
        : FLCCVector(InX, InY, InLevel)
    {
        bRenderNow = InRenderNow;
    }

    FLCCVector(const int32 InX, const int32 InY, const int32 InLevel, const bool InRenderNow, const bool InModify)
        : FLCCVector(InX, InY, InLevel, InRenderNow)
    {
        bModify = InModify;
        VisibilityMap = 0;
    }

    //move construct
    FLCCVector(FLCCVector&& Other)
        : X(FMath::Max(Other.X, 0))
          , Y(FMath::Max(Other.Y, 0))
          , Level(FMath::Max(Other.Level, 0))
          , bModify(Other.bModify)
          , bRenderNow(Other.bRenderNow)
          , CameraInfo(Other.CameraInfo)
          , VisibilityMap(0)
          , bPreload(false)
    {
    }

    FLCCVector(const FLCCVector& Other)
        : X(FMath::Max(Other.X, 0))
          , Y(FMath::Max(Other.Y, 0))
          , Level(FMath::Max(Other.Level, 0))
          , bModify(Other.bModify)
          , bRenderNow(Other.bRenderNow)
          , CameraInfo(Other.CameraInfo)
          , VisibilityMap(Other.VisibilityMap)
          , bPreload(false)
    {
    }

    FLCCVector& operator=(FLCCVector&& Other) noexcept
    {
        if (this != &Other)
        {
            X = Other.X;
            Y = Other.Y;
            Level = Other.Level;
            bModify = Other.bModify;
            bRenderNow = Other.bRenderNow;
            CameraInfo = Other.CameraInfo;
            Other.Reset();
        }
        return *this;
    }

    FLCCVector& operator=(const FLCCVector& Other)
    {
        if (this != &Other)
        {
            X = Other.X;
            Y = Other.Y;
            Level = Other.Level;
            bModify = Other.bModify;
            bRenderNow = Other.bRenderNow;
            CameraInfo = Other.CameraInfo;
            VisibilityMap = Other.VisibilityMap;
        }
        return *this;
    }

    bool operator==(const FLCCVector& Other) const
    {
        //if x and y equal,we think they are equal
        return X == Other.X && Y == Other.Y && Level == Other.Level;
    }

    bool operator==(const FLCCVector* Other) const
    {
        return X == Other->X && Y == Other->Y && Level == Other->Level;
    }

    FString ToString() const
    {
        return FString::Printf(TEXT("X:%i,Y:%i,Level:%i"), X, Y, Level);
    }

    void Reset()
    {
        X = 0;
        Y = 0;
        Level = 0;
        bModify = false;
        bRenderNow = false;
        CameraInfo.Reset();
        VisibilityMap = 0;
        bPreload = false;
    }

    static double Dist2D(const FLCCVector& A, const FLCCVector& B)
    {
        return FVector::Dist2D(FVector(A.X, A.Y, A.Level), FVector(B.X, B.Y, B.Level));
    }

    ~FLCCVector()
    {
    }
};

USTRUCT(BlueprintType)
struct FLCCSplat
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XGrids")
    FVector Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XGrids")
    FLCCVector NodeVector;

    FLCCSplat()
        : Location(FVector::ZeroVector)
          , NodeVector(FLCCVector())
    {
    }

    FLCCSplat(const FVector& Location)
        : Location(Location)
    {
    }

    FLCCSplat(const FVector& Location, const FLCCVector& NodeVector)
        : Location(Location)
          , NodeVector(NodeVector)
    {
    }

    FLCCSplat(const FLCCSplat& Other)
        : FLCCSplat()
    {
        CopyFrom(Other);
    }

    FLCCSplat& operator=(const FLCCSplat& Other)
    {
        CopyFrom(Other);
        return *this;
    }

    FORCEINLINE void CopyFrom(const FLCCSplat& Other)
    {
        Location = Other.Location;
        NodeVector = Other.NodeVector;
    }

    bool operator==(const FLCCSplat& P) const
    {
        return Location == P.Location;
    }
};
