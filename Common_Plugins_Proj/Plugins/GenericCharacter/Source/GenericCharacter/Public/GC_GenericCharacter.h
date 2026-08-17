// Copyright © 2026 Ethan Dawkins. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GC_GenericCharacter.generated.h"

// Unreal classes
class UCameraComponent;
class UInputMappingContext;

// GC classes
class UGC_InputActions;

UCLASS(meta = (DisplayName = "Generic Character"))
class GENERICCHARACTER_API AGC_GenericCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGC_GenericCharacter();

protected:
	virtual void BeginPlay() override;

public:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "GenericCharacter|Input", meta = (DisplayName = "Input - Move"))
	void Input_Move_BP(const FInputActionValue& Value);
	void Input_Move_Native(const FInputActionValue& Value);

protected:
	UPROPERTY()
	UCameraComponent* CameraComponent;

protected:
	// Default input mapping context to add in 'BeginPlay'
	UPROPERTY(EditDefaultsOnly, Category = "GenericCharacter|Input", meta = (DisplayName = "Default IMC"))
	UInputMappingContext* DefaultImc;

	// Data asset containing all our input actions
	UPROPERTY(EditDefaultsOnly, Category = "GenericCharacter|Input")
	UGC_InputActions* InputActions;

};

#pragma region AssertLogic
static void PrintDebugMessage(const FString& Msg) {
	if (GEngine) {
		GEngine->AddOnScreenDebugMessage(-1, 3, FColor::Orange, Msg);
	}

	UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
}

#define CHECK_VALID(x) \
{ \
	if (!IsValid(x)) \
	{ \
		PrintDebugMessage(FString::Format(TEXT("[{0}] [Line {1}] '{2}' is invalid on '{3}'!"), { __FUNCTION__, __LINE__, #x, UKismetSystemLibrary::GetDisplayName(this) })); \
		return; \
	} \
}
#pragma endregion
