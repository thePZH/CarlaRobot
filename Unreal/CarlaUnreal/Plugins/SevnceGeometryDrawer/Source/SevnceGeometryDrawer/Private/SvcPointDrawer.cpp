#include "SvcPointDrawer.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

void USvcPointDrawer::Initialize(AActor* InHostActor)
{
	m_HostActor = InHostActor;
	
	m_Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/SevnceGeometryDrawer/SMs/SM_Sphere.SM_Sphere"));
	m_Mat = LoadObject<UMaterial>(nullptr, TEXT("/SevnceGeometryDrawer/Mats/M_PointLine.M_PointLine"));
	if (!m_Mat || !m_Mesh)
		UE_LOG(LogTemp, Error, TEXT("USvcCubeDrawer::Initialize Failed"));
}

UPrimitiveComponent* USvcPointDrawer::Draw(const FVector& Location, const FLinearColor& Color, float Scale)
{
	if (!m_HostActor)
		return nullptr;

	UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(m_HostActor);
	MeshComp->RegisterComponent();
	MeshComp->SetMobility(EComponentMobility::Movable);
	MeshComp->AttachToComponent(m_HostActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	
	MeshComp->SetStaticMesh(m_Mesh);

	MeshComp->SetWorldLocation(Location);
	MeshComp->SetWorldScale3D(FVector(Scale));

	UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(m_Mat, MeshComp);
	DynMat->SetVectorParameterValue("Color", Color);
	MeshComp->SetMaterial(0, DynMat);

	MeshComp->SetTranslucentSortPriority(100);

	return MeshComp;
}
