// Copyright © 2026 Ethan Dawkins. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GC_GenericCharacter.generated.h"

// Unreal
class UCameraComponent;

UCLASS(meta = (DisplayName = "Generic Character"))
class GENERICCHARACTER_API AGC_GenericCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGC_GenericCharacter();

protected:
	virtual void BeginPlay() override;

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter")
	void OnMove(const FVector2D& MoveDirection);

protected:
	UPROPERTY()
	UCameraComponent* CameraComponent;

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
