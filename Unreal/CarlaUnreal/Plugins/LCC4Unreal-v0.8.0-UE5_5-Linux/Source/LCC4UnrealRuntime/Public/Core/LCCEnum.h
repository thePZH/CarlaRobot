/*********************************************
 * Copyright: XGrids Corporation
 * Author   : Feng HaiLiang
 * ******************************************/

#pragma once

#include "CoreMinimal.h"
#include "LCCEnum.generated.h"

/**
 * Locale,can be EN_US or ZH_CN
 */
UENUM(BlueprintType)
enum class ELocale : uint8
{
    EN_US,
    ZH_CN
};

/**
 * RenderMode,can be 3DGS or point cloud
 */
UENUM(BlueprintType)
enum class ERenderMode : uint8
{
    Splatting UMETA(DisplayName="3DGS", ToolTip="Render as 3D Gaussian Splatting."),
    PointCloud UMETA(DisplayName="Point Cloud", ToolTip="Render as point cloud.")
};

UENUM(BlueprintType)
enum class ESortMethod : uint8
{
    GPU UMETA(DisplayName="GPU", ToolTip="Using GPU to sort transparency."),
    CPU UMETA(DisplayName="CPU", ToolTip="Using CPU to sort transparency.")
};

UENUM(BlueprintType)
enum class EClipType : uint8
{
    Inside UMETA(DisplayName="Inside", ToolTip="The volume inside will be clipped."),
    Outside UMETA(DisplayName="Outside", ToolTip="The volume outdise will be clipped.")
};

UENUM(BlueprintType)
enum class EClipVolumeType : uint8
{
    Box UMETA(ToolTip="Use a box volume to clip"),
    Sphere UMETA(ToolTip="Use a sphere volume to clip"),
};

UENUM(BlueprintType)
enum class ESectionType : uint8
{
    Upside UMETA(DisplayName="Upside", ToolTip="The Upside of the plane will be cut."),
    Downside UMETA(DisplayName="Downside", ToolTip="The Downside of the plane will be cut.")
};

UENUM(BlueprintType)
enum class ELCCSourceType : uint8
{
    Local,
    Http
};

UENUM(BlueprintType)
enum class ECollisionType : uint8
{
    None,
    //old version collsion file
    Bin,
    //new version collsion file
    Lci
};

enum class ENodeType : uint8
{
    None,
    Main,
    Environment
};

UENUM(BlueprintType)
enum class EFileFormat : uint8
{
    None,
    LCC,
    Splats,
    LAS,
    PLY
};

UENUM(BlueprintType)
enum class ETraversalType : uint8
{
    Circle UMETA(DisplayName="Circle",
                 ToolTip=
                 "Traversal method of a circle with the camera as the center and the maximum rendering distance as the radius."),
    Sector UMETA(DisplayName="Frustum", ToolTip="Traversal method based on camera frustum.")
};

UENUM(BlueprintType)
enum class ESortMode : uint8
{
    Ascending,
    Descending
};

UENUM(BlueprintType)
enum class ELoadMode : uint8
{
    Both UMETA(DisplayName="Both", ToolTip="Render both main and environment."),
    OnlyMain UMETA(DisplayName="OnlyMain", ToolTip="Render only Main."),
    OnlyEnvironment UMETA(DisplayName="OnlyEnvironment", ToolTip="Render only Environment."),
    None UMETA(DisplayName="None", ToolTip="Render nothing.")
};

UENUM(BlueprintType)
enum class ELoadStatus : uint8
{
    Success,
    Failure,
    Progress
};

UENUM(BlueprintType)
enum class EColorationMode : uint8
{
    None UMETA(DisplayName="None", ToolTip="Uses color tint only."),
    Data UMETA(DisplayName="Data", ToolTip="Uses imported RGB."),
    Elevation UMETA(DisplayName="Evelation",
                    ToolTip="The cloud's color will be overridden with elevation-based color."),
    Position UMETA(DisplayName="Position",
                   ToolTip="The cloud's color will be overridden with relative position-based color.")
};

UENUM(BlueprintType)
enum class ELightMode : uint8
{
    Unlit UMETA(DisplayName="Unlit", ToolTip="The scene will not be affected by lighting."),
    Lit UMETA(DisplayName="Lit", ToolTip="The scene is affected by lighting."),
};

UENUM(BlueprintType)
enum class EShadowMode : uint8
{
    None UMETA(DisplayName="None", ToolTip="The scene does not receive shadows."),
    Static UMETA(DisplayName="Static", ToolTip="The scene receives static shadows."),
    Dynamic UMETA(DisplayName="Dynamic", ToolTip="The scene receives dynamic shadows.")
};


UENUM(BlueprintType)
enum class EFileType : uint8
{
    Portable UMETA(DisplayName="Portable", ToolTip="Only have RGB data,no SH."),
    Quality UMETA(DisplayName="Quality", ToolTip="Both RBG and SH.")
};

UENUM(BlueprintType)
enum class EComputeType : uint8
{
    Compute UMETA(ToolTip="Pass uses compute on the graphics pipe."),
    AsyncCompute UMETA(ToolTip="Pass uses compute on the async compute pipe.")
};
