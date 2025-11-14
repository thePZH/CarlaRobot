#include "SvcLineDrawer.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SplineComponent.h"
#include "UObject/ConstructorHelpers.h"

void USvcLineDrawer::Initialize(AActor* InActor)
{
    m_HookActor = InActor;
	m_LineMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/SevnceGeometryDrawer/SMs/SM_Line.SM_Line"));
	m_LineMaterial = LoadObject<UMaterial>(nullptr, TEXT("/SevnceGeometryDrawer/Mats/M_PointLine.M_PointLine"));
    if (!m_LineMaterial || !m_LineMesh)
    	UE_LOG(LogTemp, Error, TEXT("USvcCubeDrawer::Initialize Failed"));
}

UPrimitiveComponent* USvcLineDrawer::Draw(const TArray<FVector>& Positions, const FLinearColor& Color, float Scale) const
{
    if (!m_HookActor || Positions.Num() < 2 || !m_LineMesh || !m_LineMaterial)
    {
        return nullptr;
    }
	
    UStaticMeshComponent* rootContainer = NewObject<UStaticMeshComponent>(m_HookActor);
    rootContainer->RegisterComponent();
    rootContainer->AttachToComponent(m_HookActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

    UMaterialInstanceDynamic* dynMat = UMaterialInstanceDynamic::Create(m_LineMaterial, rootContainer);
    dynMat->SetVectorParameterValue("Color", Color);

    TArray<FVector> pointTangents;
    pointTangents.SetNumUninitialized(Positions.Num());

    for (int32 i = 0; i < Positions.Num(); ++i)
    {
        if (i == 0) 
        {
            pointTangents[i] = (Positions[1] - Positions[0]).GetSafeNormal();
        }
        else if (i == Positions.Num() - 1) 
        {
            pointTangents[i] = (Positions[i] - Positions[i - 1]).GetSafeNormal();
        }
        else
        {
            pointTangents[i] = (Positions[i + 1] - Positions[i - 1]).GetSafeNormal();
        }
    }

    for (int32 i = 0; i < Positions.Num() - 1; ++i)
    {
        const FVector& StartPos = Positions[i];
        const FVector& EndPos = Positions[i + 1];
        const FVector& StartTangent = pointTangents[i];
        const FVector& EndTangent = pointTangents[i + 1];
		
        // 以 rootContainer 作为 Outer，这样销毁 rootContainer 会级联销毁所有段组件
        USplineMeshComponent* MeshComp = NewObject<USplineMeshComponent>(rootContainer);
        MeshComp->RegisterComponent();
        MeshComp->SetMobility(EComponentMobility::Movable);
        MeshComp->AttachToComponent(rootContainer, FAttachmentTransformRules::KeepRelativeTransform);
		MeshComp->SetForwardAxis(ESplineMeshAxis::Z);
        MeshComp->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent);
        
        MeshComp->SetStaticMesh(m_LineMesh);
        MeshComp->SetMaterial(0, dynMat);
    	
        MeshComp->SetStartScale(FVector2D(Scale));
        MeshComp->SetEndScale(FVector2D(Scale));
    	// 解决3DGS显示问题
    	MeshComp->SetTranslucentSortPriority(100);
    }

    // 5. 返回根容器
    return rootContainer;
}
