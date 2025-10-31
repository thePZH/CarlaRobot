#include "SvcCubeDrawer.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

void USvcCubeDrawer::Initialize(AActor* InHostActor)
{
	m_HostActor = InHostActor;
	m_CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/SevnceGeometryDrawer/SMs/SM_BoxFrame.SM_BoxFrame"));
	m_CubeMaterial = LoadObject<UMaterial>(nullptr, TEXT("/SevnceGeometryDrawer/Mats/M_BoxFrame.M_BoxFrame"));

	if (!m_CubeMaterial || !m_CubeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("USvcCubeDrawer::Initialize Failed"));
	}
}

UPrimitiveComponent* USvcCubeDrawer::Draw(const FVector& Center, const FVector& Scale, const FLinearColor& Color)
{
	if (!m_HostActor)
		return nullptr;

	UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(m_HostActor);
	MeshComp->RegisterComponent();
	MeshComp->SetMobility(EComponentMobility::Movable);
	MeshComp->AttachToComponent(m_HostActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	MeshComp->SetStaticMesh(m_CubeMesh);

	MeshComp->SetWorldLocation(Center);
	MeshComp->SetWorldScale3D(Scale); //  cube 尺寸 100

	UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(m_CubeMaterial, MeshComp);
	DynMat->SetVectorParameterValue("Color", Color);
	MeshComp->SetMaterial(0, DynMat);

	return MeshComp;
}
