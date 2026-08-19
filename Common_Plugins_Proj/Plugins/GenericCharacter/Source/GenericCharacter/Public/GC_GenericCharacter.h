// Copyright © 2026 Ethan Dawkins. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GC_GenericCharacter.generated.h"

// Unreal
class UCameraComponent;

UENUM(BlueprintType)
enum class EGC_CrouchState : uint8
{
	Uncrouched,
	InterpToCrouched,
	Crouched,
	InterpToUncrouched,
};

UENUM(BlueprintType)
enum class EGC_MovementCapability : uint8
{
	Crouch,
	Jump,
	Walk,
	Swim,
	Fly
};

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

	virtual FVector GetPawnViewLocation() const override;
	virtual void RecalculateBaseEyeHeight() override;

public:
	UFUNCTION(BlueprintPure, Category = "GenericCharacter|Helpers")
	bool CheckMovementCapability(EGC_MovementCapability CapabilityToCheck) const;

public:
	// By default, this will add movement input along flattened camera right/forward vectors
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Move")
	void OnMove(const FVector2D& MoveDirection);

	// By default, this will add look input to yaw/pitch (x/y respectively)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Look")
	void OnLook(FVector2D LookDirection);

#pragma region Jump
public:
	// By default, launches character upwards to reach desired jump height
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Jump")
	void OnJump();

	// Set jump height, and calculate jump force appropriately
	UFUNCTION(BlueprintCallable)
	void SetJumpHeight(float NewHeight = 150.f);
#pragma endregion

#pragma region Crouch
public:
	// By default, sets crouch state to 'InterpToCrouched'
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Crouch")
	void OnStartCrouch();

	// By default, sets crouch state to 'InterpToUncrouched' if part-way
	// through crouch, or defaults to standard Unreal 'UnCrouch'
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Crouch")
	void OnEndCrouch();

	// By default, calls 'OnStartCrouch' or 'OnEndCrouch' depending on current crouch state
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Crouch")
	void OnToggleCrouch();

protected:
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	// Controls crouch state machine
	void TickCrouchState(float DeltaSeconds);
	void InterpCrouch(float DeltaSeconds);
#pragma endregion

protected:
	UPROPERTY()
	UCameraComponent* CameraComponent;

	// The current eye offset from the characters feet
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|General|State", meta = (Units = "cm"))
	float EyeHeightFromFeet = 0.f;

	// Multiplier applied to look input
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Look", meta = (ClampMin = "0.1", Units = "Times"))
	float SensitivityMultiplier = 1.f;

	// Should look input 'y' be inverted?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Look")
	bool bInvertY = true;

	// How high should a single jump reach?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Jump", meta = (ClampMin = "0", Units = "cm"), BlueprintSetter = SetJumpHeight)
	float JumpHeight = 150.f;

	// Number of jumps allowed before needing to touch the ground again
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Jump", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxJumpCount = 1;

	// How long should it take to enter/exit crouch?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Crouch", meta = (ClampMin = "0.1", ClampMax = "2.0", Units = "s"))
	float CrouchDuration = 0.3f;

	// Maps crouch time% (X) to height% (Y). Expected X range: [0..1], Y range [0..1]
	// If not set, enter crouch defaults to linear interpolation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Crouch")
	UCurveFloat* EnterCrouchCurve;

	// Maps crouch time% (X) to height% (Y). Expected X range: [1..0], Y range [0..1]
	// If not set, exit crouch defaults to linear interpolation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Crouch")
	UCurveFloat* ExitCrouchCurve;

	// The current crouch state
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Crouch|State")
	EGC_CrouchState CrouchState = EGC_CrouchState::Uncrouched;

	// The current crouch time (how long crouch has been going for)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Crouch|State", meta = (Units = "s"))
	float CrouchTime = 0.f;

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
