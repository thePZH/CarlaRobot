// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

class AActor;
#include "SevnceMainCameraPawn.generated.h"

UENUM(BlueprintType)
enum class ECameraAttachmentMode : uint8
{
	Fixed			UMETA(DisplayName = "Fixed"),
	SpringArm		UMETA(DisplayName = "SpringArm")
};

UCLASS()
class SEVNCECAMERASYSTEM_API ASevnceMainCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ASevnceMainCameraPawn(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// 设置相机 Transform
	UFUNCTION(BlueprintCallable, Category = "Sevnce Camera")
	void SetCameraTransform(const FTransform& InTransform);

	// 绑定到目标 Actor
	UFUNCTION(BlueprintCallable, Category = "Sevnce Camera")
	void AttachToActorTarget(AActor* InTargetActor, ECameraAttachmentMode InMode = ECameraAttachmentMode::Fixed);

	// 解绑
	UFUNCTION(BlueprintCallable, Category = "Sevnce Camera")
	void DetachFromActorTarget();

	// 设置固定模式的局部位置和旋转
	UFUNCTION(BlueprintCallable, Category = "Sevnce Camera")
	void SetFixedModeTransform(const FVector& InLocalLocation, const FRotator& InLocalRotation);

	// 设置弹簧臂模式的参数
	UFUNCTION(BlueprintCallable, Category = "Sevnce Camera")
	void SetSpringArmParameters(float InTargetArmLength, const FVector& InTargetOffset, bool bInDoCollisionTest = true);

	// 设置弹簧臂旋转
	UFUNCTION(BlueprintCallable, Category = "Sevnce Camera")
	void SetSpringArmRotation(const FRotator& InRotation);

	// 设置弹簧臂锚点的局部偏移
	UFUNCTION(BlueprintCallable, Category = "Sevnce Camera")
	void SetSpringArmAnchorOffset(const FVector& InLocalLocation);

	// 获取相机组件
	UFUNCTION(BlueprintCallable, Category = "Sevnce Camera")
	UCameraComponent* GetCameraComponent() const { return m_CameraComponent; }

	// 获取弹簧臂组件
	UFUNCTION(BlueprintCallable, Category = "Sevnce Camera")
	USpringArmComponent* GetSpringArmComponent() const { return m_SpringArmComponent; }

protected:
	// 相机组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* m_CameraComponent;

	// 弹簧臂组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* m_SpringArmComponent;

	// 当前绑定的目标 Actor
	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	AActor* m_AttachedActor;

	// 当前绑定模式
	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	ECameraAttachmentMode m_CurrentMode;

	// 固定模式的局部位置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Fixed Mode")
	FVector m_FixedLocalLocation;

	// 固定模式的局部旋转
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Fixed Mode")
	FRotator m_FixedLocalRotation;

	// 鼠标输入处理
	void OnMouseX(float Value);
	void OnMouseY(float Value);

	// 鼠标旋转速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|SpringArm Mode")
	float m_MouseSensitivity;

	// 当前鼠标旋转角度（用于弹簧臂模式）
	FRotator m_CurrentSpringArmRotation;

private:
	// 同步自身 Transform 到被绑定 Actor
	void UpdateAttachedActorTransform();

	// 更新弹簧臂模式的旋转
	void UpdateSpringArmRotation();

	// 当前锚点偏移（cm）
	FVector m_CurrentAnchorOffset;

	// 弹簧臂模式的锚点偏移
	FVector m_SpringArmAnchorOffset;
};

