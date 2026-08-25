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
	FallingRequestCrouched,
	FallingRequestUncrouched,
};

UENUM(BlueprintType)
enum class EGC_SlideState : uint8
{
	InterpEnter,
	Sliding,
	InterpExit,
	NotSliding
};

UENUM(BlueprintType)
enum class EGC_SlideExitType : uint8
{
	IntoStanding,
	IntoCrouched,
	Invalid
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

	void TickEyeHeight(float DeltaSeconds);

	using FOnReachedEyeHeightTargetCallback = void(AGC_GenericCharacter::*)();
	void SetEyeHeightTarget(FVector2D TargetRange, float Duration, UCurveFloat* TimeHeightCurve = nullptr, FOnReachedEyeHeightTargetCallback ReachedTargetCallback = nullptr);

	virtual FVector GetPawnViewLocation() const override;
	virtual void RecalculateBaseEyeHeight() override;
	virtual void OnWalkingOffLedge_Implementation(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta) override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

public:
	// Checks if the passed in movement capability is enabled on the movement component
	UFUNCTION(BlueprintPure, Category = "GenericCharacter|Helpers")
	bool CheckMovementCapability(EGC_MovementCapability CapabilityToCheck) const;

	// Checks if movement component is valid, then gets its' falling state. Defaults to false.
	UFUNCTION(BlueprintPure, Category = "GenericCharacter|Helpers")
	bool IsCharacterFalling() const;

	// Checks capsule is valid, then sets its unscaled half height.
	// @param bScaleFromBottom Should capsule be offset so scaling pivots from its' lowest point?
	UFUNCTION(BlueprintCallable, Category = "GenericCharacter|Helpers")
	void SetCapsuleHalfHeight(float HalfHeight, bool bScaleFromBottom = false);

	// Checks if re-sizing the capsule component would collide with anything.
	// @param OutHit The result of the bounds check. Defaults to empty result if capsule / inputs are invalid.
	// @param bScaleHeight Should height input be multiplied with current component scale?
	// @param bCheckFromBottom Should capsule check happen pivoted from its' lowest point, or centered on actor location?
	// @param DrawDebug Should we debug draw the bounds check? ('ForDuration' uses 3.0s lifetime)
	UFUNCTION(BlueprintCallable, Category = "GenericCharacter|Helpers")
	void CheckCapsuleHeight(FHitResult& OutHit, float HalfHeight, bool bScaleHeight, bool bCheckFromBottom, EDrawDebugTrace::Type DrawDebug = EDrawDebugTrace::None);

public:
	// By default, this will add movement input along flattened camera right/forward vectors
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Move")
	void OnMove(const FVector2D& MoveDirection);

	// By default, this will add look input to yaw/pitch (x/y respectively)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Look")
	void OnLook(FVector2D LookDirection);

#pragma region Jump
public:
	// By default, calls normal Unreal 'Jump'
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Jump")
	void OnJump();

	// Set jump height, and calculate jump force appropriately
	UFUNCTION(BlueprintCallable, Category = "GenericCharacter|Jump")
	void SetJumpHeight(float NewHeight = 150.f);

	UFUNCTION(BlueprintPure, Category = "GenericCharacter|Jump")
	bool IsInCoyoteTimeWindow() const;

	UFUNCTION(BlueprintPure, Category = "GenericCharacter|Jump")
	bool IsInJumpBufferWindow() const;

	virtual bool CanJumpInternal_Implementation() const;
	virtual void CheckJumpInput(float DeltaTime);
	virtual void NotifyJumpApex() override;
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

	// Checks if we are not in the 'Uncrouched' state or if we are in standard
	// Unreal crouch state (i.e. player under a ledge but wants to UnCrouch)
	UFUNCTION(BlueprintPure, Category = "GenericCharacter|Crouch")
	bool IsInCrouchedState() const;

protected:
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	void SetCrouched(bool bNewState);
	// Controls crouch state machine
	void TickCrouchState(float DeltaSeconds);

	void OnFinishInterpCrouch();
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

#pragma region Slide
public:
	// By default, set slide state to 'true'
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Slide")
	void OnStartSlide();

	// By default, set slide state to 'false'
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Slide")
	void OnEndSlide();

	// By default, tries to move out of collision after failing to exit 'SlideMaxExitAttempts' times.
	// @param DepenetrationVector The computed horizontal offset that should get us out of collision. Will be zeroed if not computable.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Slide")
	void OnFailedSlideExit(const FVector& DepenetrationVector);

	// By default, checks that we are not falling and speed is above 'SlideAutoExitSpeed'
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GenericCharacter|Slide")
	bool CanSlide() const;

	// Checks if we are not in the 'NotSliding' state
	UFUNCTION(BlueprintPure, Category = "GenericCharacter|Slide")
	bool IsInSlideState() const;

protected:
	void TickSlideState(float DeltaSeconds);

	virtual void StartInterpEnterSlide();
	virtual void FinishInterpEnterSlide();

	virtual void StartInterpExitSlide();
	virtual void FinishInterpExitSlide();

	// Run bounds checks for standing / crouching, or compute a location offset to get out of collision.
	// @param OutPenetrationOffset Will be non-zero if a valid depenetration vector can be computed.
	EGC_SlideExitType FindSuitableSlideExitType(FVector& OutPenetrationOffset);

	void AttemptSlideExit();

	bool SlideShouldExitToCrouched() const;

	void OnFinishInterpSlide();
	
#pragma endregion

protected:
	UPROPERTY()
	UCameraComponent* CameraComponent;

	DECLARE_DELEGATE(FOnReachedEyeHeightTarget);
	FOnReachedEyeHeightTarget OnReachedEyeHeightTarget;

	// Should a box be drawn at the current 'PawnViewLocation'?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|General")
	bool bDebugEyeHeight = false;

	// The current eye offset from the characters feet
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|General|State|EyeHeight", meta = (Units = "cm"))
	float EyeHeightFromFeet = 0.f;

	// The target eye height range where X = 0 time%, and Y = 1 time%
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|General|State|EyeHeight")
	FVector2D TargetEyeHeightRange = { 0.f, 0.f };

	// How long the current eye height interp will take to get from 0..1 time%
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|General|State|EyeHeight", meta = (Units = "s"))
	float EyeHeightInterpDuration = 0.f;

	// The current eye height interp time used to calculate time%
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|General|State|EyeHeight", meta = (Units = "s"))
	float EyeHeightInterpTime = 0.f;

	// The current, optional, curve that maps time% (X) to height% (Y). Expected X range: [0..1], Y range [0..1]
	// If not set, defaults to linear interpolation
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|General|State|EyeHeight")
	UCurveFloat* EyeHeightInterpCurve = nullptr;

	// Exact 'World::TimeSeconds' that we walked off a ledge
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|General|State", meta = (Units = "s"))
	double TimeWalkedOffLedge = 0.f;

	// Multiplier applied to look input
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Look", meta = (ClampMin = "0.1", Units = "Times"))
	float SensitivityMultiplier = 1.f;

	// Should look input 'y' be inverted?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Look")
	bool bInvertY = true;

	// How high should a single jump reach?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Jump", meta = (ClampMin = "0", Units = "cm"), BlueprintSetter = SetJumpHeight)
	float JumpHeight = 150.f;

	// Should first jump be allowed mid-air? i.e. jump count = 2, would allow 2 mid-air jumps
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Jump")
	bool bAllowFirstJumpWhileFalling = false;

	// Should jump input be allowed while we are crouched?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Jump")
	bool bAllowJumpWhileCrouched = false;

	// Should there be a window after walking off a ledge where jump is still allowed?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Jump", meta = (InlineEditConditionToggle))
	bool bUseCoyoteTime = true;

	// Coyote time is a window after walking off a ledge where jump is still allowed.
	// How long should that window be?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Jump", meta = (EditCondition = "bUseCoyoteTime", Units = "s", ClampMin = "0.1", ClampMax = "1.0"))
	float CoyoteTimeDuration = 0.25f;

	// Should there be a window before landing where jump input gets stored, then consumed upon landing?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Jump", meta = (InlineEditConditionToggle))
	bool bUseJumpBuffer = true;

	// Jump buffer is a window before landing where jump input gets stored, then consumed upon landing.
	// How long should that window be?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Jump", meta = (EditCondition = "bUseJumpBuffer", Units = "s", ClampMin = "0.05", ClampMax = "0.5"))
	float JumpBufferDuration = 0.125f;

	// Should gravity scale switch to a custom value after reaching jump apex?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Jump", meta = (InlineEditConditionToggle))
	bool bUseSeparateJumpGravity = true;

	// When jump reaches its' apex, should we switch to a separate gravity scale?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Jump", meta = (EditCondition = "bUseSeparateJumpGravity", Units = "Times", ClampMin = "1.0", ClampMax = "5.0"))
	float JumpGravityScale = 1.75f;

	// Exact 'World::TimeSeconds' that player last pressed jump input
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Jump|State", meta = (Units = "s"))
	double TimeJumpInputPressed = 0.f;

	// Gravity scale we will go back to once we have landed from a jump
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Jump|State", meta = (Units = "Times"))
	double StoredGravityScale = 0.f;

	// Which input modes should crouch allow?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Crouch")
	EGC_InputMode CrouchInput = EGC_InputMode::Both;

	// Should crouch input be allowed while we are mid-air?
	// If false, crouch state will be corrected when we land
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Crouch")
	bool bAllowCrouchWhileFalling = false;

	// How long should it take to enter/exit crouch?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Crouch", meta = (ClampMin = "0.1", ClampMax = "2.0", Units = "s"))
	float CrouchDuration = 0.3f;

	// Maps crouch time% (X) to height% (Y). Expected X range: [0..1], Y range [0..1]
	// If not set, defaults to linear interpolation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Crouch")
	UCurveFloat* EnterCrouchCurve;

	// Maps crouch time% (X) to height% (Y). Expected X range: [0..1], Y range [0..1]
	// If not set, defaults to linear interpolation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Crouch")
	UCurveFloat* ExitCrouchCurve;

	// The current crouch state
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Crouch|State")
	EGC_CrouchState CrouchState = EGC_CrouchState::Uncrouched;

	// Which input modes should sprint allow?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Sprint")
	EGC_InputMode SprintInput = EGC_InputMode::Both;

	// The maximum ground speed when sprinting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Sprint", meta = (ForceUnits = "cm/s", ClampMin = "50.0"))
	float SprintSpeed = 800.f;

	// Should sprint input be allowed while we are crouched?
	// This only affects if we will be sprinting when we un-crouch
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Sprint")
	bool bAllowSprintWhileCrouched = false;

	// Should sprint be forcefully cancelled upon entering slide?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Sprint")
	bool bCancelSprintWhenSliding = true;

	// The current sprint state
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Sprint|State")
	bool bIsSprinting = false;

	// The ground speed to go back to when sprint ends
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Sprint|State", meta = (ClampMin = "150.0", ForceUnits = "cm/s"))
	float StoredWalkSpeed = 0.f;

	// Braking friction during a slide
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide", meta = (ClampMin = "0"))
	float SlideGroundFriction = 0.25f;

	// Braking deceleration during a slide
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide", meta = (ClampMin = "0"))
	float SlideBrakingDeceleration = 256.f;

	// The eye height while sliding
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide", meta = (Units = "cm", ClampMin = "0"))
	float SlideEyeHeight = 24.f;

	// The capsule half height while sliding
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide", meta = (Units = "cm", ClampMin = "5"))
	float SlideHalfHeight = 32.f;

	// How long should slide enter interp last?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide|Enter", meta = (Units = "s", ClampMin = "0.1", ClampMax = "2"))
	float SlideEnterDuration = 0.2f;

	// Maps slide time% (X) to height% (Y). Expected X range: [0..1], Y range [0..1]
	// If not set, defaults to linear interpolation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide|Enter")
	UCurveFloat* SlideEnterCurve;

	// Instant speed boost when entering a slide, 0 disables any boost
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide|Enter", meta = (ForceUnits = "cm/s", ClampMin = "0"))
	float InitialSlideBoost = 200.f;

	// How long should slide exit interp to standing last?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide|Exit", meta = (Units = "s", ClampMin = "0.1", ClampMax = "2"))
	float SlideExitToStandingDuration = 0.25f;

	// Maps slide time% (X) to height% (Y). Expected X range: [0..1], Y range [0..1]
	// If not set, defaults to linear interpolation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide|Exit")
	UCurveFloat* SlideExitToStandingCurve;

	// How long should slide exit interp to crouched last?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide|Exit", meta = (Units = "s", ClampMin = "0.1", ClampMax = "2"))
	float SlideExitToCrouchedDuration = 0.15f;

	// Maps slide time% (X) to height% (Y). Expected X range: [0..1], Y range [0..1]
	// If not set, defaults to linear interpolation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide|Exit")
	UCurveFloat* SlideExitToCrouchedCurve;

	// At what speed will slide automatically end?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide|Exit", meta = (ForceUnits = "cm/s", ClampMin = "0"))
	float SlideAutoExitSpeed = 200.f;

	// How should the slide exit bounds check be debug drawn?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide|Exit")
	TEnumAsByte<EDrawDebugTrace::Type> DebugSlideExitCheck = EDrawDebugTrace::None;

	// How many times should we try to exit slide before calling 'OnFailedSlideExit'?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide|Exit", meta = (ClampMin = "1", ClampMax = "7"))
	int32 SlideMaxExitAttempts = 3;

	// The interval between exit attempts
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide|Exit", meta = (Units = "s", ClampMin = "0.05", ClampMax = "0.5"))
	float SlideExitAttemptInterval = 0.1f;

	// True = slide will only exit into crouched
	// False = slide will pick crouched/standing based on available headroom
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide|Exit")
	bool bSlideExitToCrouched = false;

	// Same as 'bSlideExitToCrouched' but for when slide auto-ends (i.e. low speed)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GenericCharacter|Slide|Exit")
	bool bSlideAutoExitToCrouched = true;

	// The current slide state
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Slide|State")
	EGC_SlideState SlideState = EGC_SlideState::NotSliding;

	// The current slide exit state, set to most suitable exit type when slide ends.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Slide|State")
	EGC_SlideExitType SlideExitState = EGC_SlideExitType::Invalid;

	// The current slide exit attempt number
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Slide|State")
	int32 SlideExitAttempt = 0;

	// Tracks whether slide was exited manually (via input) or auto-exited (i.e. low speed)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Slide|State")
	bool bWasSlideAutoExited = false;

	// 'UseSeparateBrakingFriction' state to go back to on slide exit
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Slide|State")
	bool bStoredUseSeparateBrakingFriction = false;

	// 'BrakingFriction' to go back to on slide exit
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Slide|State")
	float StoredBrakingFriction = 0.f;

	// 'BrakingDeceleration' to go back to on slide exit
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenericCharacter|Slide|State")
	float StoredBrakingDeceleration = 0.f;

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
