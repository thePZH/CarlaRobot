// Copyright Epic Games, Inc. All Rights Reserved.

#include "SevnceMainCameraPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

ASevnceMainCameraPawn::ASevnceMainCameraPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, m_AttachedActor(nullptr)
	, m_CurrentMode(ECameraAttachmentMode::Fixed)
	, m_FixedLocalLocation(FVector::ZeroVector)
	, m_FixedLocalRotation(FRotator::ZeroRotator)
	, m_MouseSensitivity(2.0f)
	, m_CurrentSpringArmRotation(FRotator::ZeroRotator)
	, m_CurrentAnchorOffset(FVector::ZeroVector)
	, m_SpringArmAnchorOffset(FVector::ZeroVector)
{
	PrimaryActorTick.bCanEverTick = true;

	// 创建根组件
	USceneComponent* RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent = RootSceneComponent;

	// 创建弹簧臂组件
	m_SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	m_SpringArmComponent->SetupAttachment(RootComponent);
	m_SpringArmComponent->TargetArmLength = 500.0f;
	m_SpringArmComponent->bUsePawnControlRotation = true;
	m_SpringArmComponent->bInheritPitch = true;
	m_SpringArmComponent->bInheritYaw = true;
	m_SpringArmComponent->bInheritRoll = false;
	m_SpringArmComponent->bDoCollisionTest = true;
	m_SpringArmComponent->bEnableCameraRotationLag = true;
	m_SpringArmComponent->CameraRotationLagSpeed = 10.0f;

	// 创建相机组件（默认附加到根组件，在绑定到机器人时会重新附加）
	m_CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	m_CameraComponent->SetupAttachment(RootComponent);

	// 默认使用固定模式，弹簧臂暂时禁用
	m_SpringArmComponent->SetActive(false);
}

void ASevnceMainCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	// 设置默认的固定模式位置（在机器人上方）
	m_FixedLocalLocation = FVector(0.0f, 0.0f, 200.0f);
	m_FixedLocalRotation = FRotator(-20.0f, 0.0f, 0.0f);
}

void ASevnceMainCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 根据当前模式更新相机位置
	if (m_AttachedActor != nullptr)
	{
		UpdateAttachedActorTransform();

		if (m_CurrentMode == ECameraAttachmentMode::SpringArm)
		{
			UpdateSpringArmRotation();
		}
	}
}

void ASevnceMainCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 绑定鼠标输入（仅在弹簧臂模式下有效）
	PlayerInputComponent->BindAxis("MouseX", this, &ASevnceMainCameraPawn::OnMouseX);
	PlayerInputComponent->BindAxis("MouseY", this, &ASevnceMainCameraPawn::OnMouseY);
}

void ASevnceMainCameraPawn::SetCameraTransform(const FTransform& InTransform)
{
	if (m_AttachedActor != nullptr)
	{
		// 如果已绑定到其他 Actor，先解绑
		DetachFromActorTarget();
	}

	SetActorTransform(InTransform);
}

void ASevnceMainCameraPawn::AttachToActorTarget(AActor* InTargetActor, ECameraAttachmentMode InMode)
{
	if (InTargetActor == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASevnceMainCameraPawn::AttachToActorTarget: Target actor is null"));
		return;
	}

	if (m_AttachedActor != nullptr)
	{
		DetachFromActorTarget();
	}

	m_AttachedActor = InTargetActor;
	m_CurrentMode = InMode;

	if (m_CurrentMode == ECameraAttachmentMode::Fixed)
	{
		m_SpringArmComponent->SetActive(false);
		m_CameraComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		m_CameraComponent->SetRelativeLocation(m_FixedLocalLocation);
		m_CameraComponent->SetRelativeRotation(m_FixedLocalRotation);
		m_CurrentAnchorOffset = FVector::ZeroVector;
	}
	else if (m_CurrentMode == ECameraAttachmentMode::SpringArm)
	{
		m_SpringArmComponent->SetActive(true);
		m_CameraComponent->AttachToComponent(
			m_SpringArmComponent,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			USpringArmComponent::SocketName);
		m_SpringArmComponent->SetRelativeLocation(FVector::ZeroVector);
		m_SpringArmComponent->SetRelativeRotation(FRotator::ZeroRotator);
		m_CurrentAnchorOffset = m_SpringArmAnchorOffset;
		SetSpringArmRotation(m_CurrentSpringArmRotation);

		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			// PC->bShowMouseCursor = false;
			// PC->bEnableClickEvents = false;
			// PC->bEnableMouseOverEvents = false;
			// PC->SetInputMode(FInputModeGameOnly());
		}
	}

	UpdateAttachedActorTransform();
}

void ASevnceMainCameraPawn::DetachFromActorTarget()
{
	if (m_AttachedActor == nullptr)
	{
		return;
	}

	m_AttachedActor = nullptr;
	m_CurrentMode = ECameraAttachmentMode::Fixed;
	m_CurrentAnchorOffset = FVector::ZeroVector;

	m_SpringArmComponent->SetActive(false);
	m_SpringArmComponent->SetRelativeLocation(FVector::ZeroVector);
	m_SpringArmComponent->SetRelativeRotation(FRotator::ZeroRotator);

	m_CameraComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	m_CameraComponent->SetRelativeLocation(FVector::ZeroVector);
	m_CameraComponent->SetRelativeRotation(FRotator::ZeroRotator);

}

void ASevnceMainCameraPawn::SetFixedModeTransform(const FVector& InLocalLocation, const FRotator& InLocalRotation)
{
	m_FixedLocalLocation = InLocalLocation;
	m_FixedLocalRotation = InLocalRotation;

	// 如果当前是固定模式且已绑定，立即更新
	if (m_AttachedActor != nullptr && m_CurrentMode == ECameraAttachmentMode::Fixed)
	{
		m_CameraComponent->SetRelativeLocation(m_FixedLocalLocation);
		m_CameraComponent->SetRelativeRotation(m_FixedLocalRotation);
		UpdateAttachedActorTransform();
	}
}

void ASevnceMainCameraPawn::SetSpringArmParameters(float InTargetArmLength, const FVector& InTargetOffset, bool bInDoCollisionTest)
{
	m_SpringArmComponent->TargetArmLength = InTargetArmLength;
	m_SpringArmComponent->TargetOffset = InTargetOffset;
	m_SpringArmComponent->bDoCollisionTest = bInDoCollisionTest;
}

void ASevnceMainCameraPawn::SetSpringArmAnchorOffset(const FVector& InLocalLocation)
{
	m_SpringArmAnchorOffset = InLocalLocation;

	if (m_AttachedActor != nullptr && m_CurrentMode == ECameraAttachmentMode::SpringArm)
	{
		m_CurrentAnchorOffset = m_SpringArmAnchorOffset;
		UpdateAttachedActorTransform();
	}
}

void ASevnceMainCameraPawn::SetSpringArmRotation(const FRotator& InRotation)
{
	m_CurrentSpringArmRotation = InRotation;
	UpdateSpringArmRotation();
}

void ASevnceMainCameraPawn::OnMouseX(float Value)
{
	// 仅在弹簧臂模式下响应鼠标输入
	if (m_CurrentMode == ECameraAttachmentMode::SpringArm && m_AttachedActor != nullptr)
	{
		m_CurrentSpringArmRotation.Yaw += Value * m_MouseSensitivity;
	}
}

void ASevnceMainCameraPawn::OnMouseY(float Value)
{
	// 仅在弹簧臂模式下响应鼠标输入
	if (m_CurrentMode == ECameraAttachmentMode::SpringArm && m_AttachedActor != nullptr)
	{
		m_CurrentSpringArmRotation.Pitch = FMath::Clamp(
			m_CurrentSpringArmRotation.Pitch - Value * m_MouseSensitivity,
			-89.0f,
			89.0f
		);
	}
}

void ASevnceMainCameraPawn::UpdateSpringArmRotation()
{
	if (m_AttachedActor == nullptr)
	{
		return;
	}

	// 更新弹簧臂的旋转
	m_SpringArmComponent->SetRelativeRotation(m_CurrentSpringArmRotation);
}

void ASevnceMainCameraPawn::UpdateAttachedActorTransform()
{
	if (m_AttachedActor == nullptr)
	{
		return;
	}

	const FTransform ActorTransform = m_AttachedActor->GetActorTransform();
	const FVector WorldLocation = ActorTransform.TransformPosition(m_CurrentAnchorOffset);
	const FRotator WorldRotation = ActorTransform.GetRotation().Rotator();

	SetActorLocationAndRotation(WorldLocation, WorldRotation);
}

