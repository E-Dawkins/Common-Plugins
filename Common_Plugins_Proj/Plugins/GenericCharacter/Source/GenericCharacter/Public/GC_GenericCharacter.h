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

UENUM(BlueprintType)
enum class EGC_InputMode : uint8
{
	Hold	UMETA(ToolTip = "Hold input only"),
	Toggle	UMETA(ToolTip = "Toggle input only"),
	Both	UMETA(ToolTip = "Hold & Toggle input allowed")
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
	// Checks if the passed in movement capability is enabled on the movement component
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
	UFUNCTION(BlueprintCallable, Category = "GenericCharacter|Jump")
	void SetJumpHeight(float NewHeight = 150.f);
#pragma endregion

#pragma region Crouch
public:
	// By default, will check for 'Hold' or 'Both' crouch input then set crouch state to 'InterpToCrouched'
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Crouch")
	void OnStartCrouch();

	// By default, will check for 'Hold' or 'Both' crouch input then set crouch state to 'InterpToUncrouched',
	// or 'Uncrouched' to defer state logic until Unreal allows us to uncrouch (i.e. not enough head room)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Crouch")
	void OnEndCrouch();

	// By default, will check for 'Toggle' or 'Both' crouch input then switch crouch state
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Crouch")
	void OnToggleCrouch();

	// Checks if we are not in the 'Uncrouched' state
	UFUNCTION(BlueprintPure, Category = "GenericCharacter|Crouch")
	bool IsInCrouchedState() const;

protected:
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	void SetCrouched(bool bNewState);
	// Controls crouch state machine
	void TickCrouchState(float DeltaSeconds);
	void InterpCrouch(float DeltaSeconds);
#pragma endregion

#pragma region Sprint
public:
	// By default, will check for 'Hold' or 'Both' sprint input and then set sprint state to 'true'
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Sprint")
	void OnStartSprint();

	// By default, will check for 'Hold' or 'Both' sprint input and then set sprint state to 'false'
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Sprint")
	void OnEndSprint();

	// By default, will check for 'Toggle' or 'Both' sprint input and then switch sprint state
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Sprint")
	void OnToggleSprint();

protected:
	void SetSprintState(bool bNewState);
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

	// Should jump input be allowed while we are crouched?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Jump")
	bool bAllowJumpWhileCrouched = false;

	// Which input modes should crouch allow?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Crouch")
	EGC_InputMode CrouchInput = EGC_InputMode::Both;

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

	// Which input modes should sprint allow?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Sprint")
	EGC_InputMode SprintInput = EGC_InputMode::Both;

	// The maximum ground speed when sprinting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Sprint", meta = (ForceUnits = "cm/s"))
	float SprintSpeed = 800.f;

	// Should sprint input be allowed while we are crouched?
	// This only affects if we will be sprinting when we un-crouch
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Sprint")
	bool bAllowSprintWhileCrouched = false;

	// The current sprint state
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Sprint|State")
	bool bIsSprinting = false;

	// The ground speed to go back to when sprint ends
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Sprint|State", meta = (ClampMin = "150.0", ForceUnits = "cm/s"))
	float StoredWalkSpeed = 0.f;

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
