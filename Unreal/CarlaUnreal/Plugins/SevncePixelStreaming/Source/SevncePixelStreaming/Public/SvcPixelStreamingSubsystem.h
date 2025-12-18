#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SvcPixelStreamingSubsystem.generated.h"

// 前向声明
class UPixelStreamingInput;

UCLASS()
class SEVNCEPIXELSTREAMING_API USvcPixelStreamingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 委托声明 - 用于接收来自Web的消息
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWebMessageReceived, const FString&, Message);
    
	UPROPERTY(BlueprintAssignable)
	FOnWebMessageReceived OnWebMessageReceived;

	UFUNCTION(BlueprintCallable, Category = "SevncePixelStreaming")
	void SendNotify(const FString& Name, const FString& PayloadJson);

	UFUNCTION(BlueprintCallable, Category = "SevncePixelStreaming")
	void SendResponse(const FString& Name, const FString& PayloadJson);

private:
	void BindInputComponent();

  	// 发送消息到Web端
  	void SendMessageToWeb(const FString& JsonString);
	
  	// 延迟到世界初始化后再完成绑定
  	FDelegateHandle m_InitHandle;
		
	// 处理从Web接收到的消息
  	UFUNCTION()
  	void HandleWebInputEvent(const FString& Descriptor);
	
  	UPROPERTY()
  	AActor* m_PixelStreamingActor = nullptr;
	
  	UPROPERTY()
  	UPixelStreamingInput* m_PixelInputComponent = nullptr;
};