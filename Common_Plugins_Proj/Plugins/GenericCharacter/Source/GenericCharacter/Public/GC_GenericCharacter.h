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

	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnCmcUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

	// Triggered at the end of every movement tick, after all physics and collision calculations, but before 'Actor::Tick'
	UFUNCTION(BlueprintImplementableEvent, Category = "GenericCharacter", meta = (DisplayName = "Tick - CMC"))
	void TickCmc(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

public:
	// By default, this will add movement input along flattened camera right/forward vectors
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Move")
	void OnMove(const FVector2D& MoveDirection);

	// By default, this will add look input to yaw/pitch (x/y respectively)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Look")
	void OnLook(FVector2D LookDirection);

	// By default, ...
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Jump")
	void OnJump();

	UFUNCTION(BlueprintCallable)
	void SetJumpHeight(float NewHeight = 150.f);

protected:
	UPROPERTY()
	UCameraComponent* CameraComponent;

	// Multiplier applied to look input
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Look", meta = (ClampMin = "0.1", Units = "Times"))
	float SensitivityMultiplier = 1.f;

	// Should look input 'y' be inverted?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Look")
	bool bInvertY = true;

	// How high should a single jump reach?
	// If you want to dynamically adjust this number, use 'SetJumpHeight'
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Jump", meta = (ClampMin = "0", Units = "cm"), BlueprintSetter = SetJumpHeight)
	float JumpHeight = 150.f;

	// Number of jumps allowed before needing to touch the ground again
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Jump", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxJumpCount = 1;

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
