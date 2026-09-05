// Copyright © 2026 Ethan Dawkins. All rights reserved.


#include "GC_GenericCharacter.h"

// Unreal
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AGC_GenericCharacter::AGC_GenericCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	if (UCharacterMovementComponent* CMC = GetCharacterMovement(); IsValid(CMC))
	{
		CMC->AirControl = 0.5f;
		CMC->bCanWalkOffLedgesWhenCrouching = true;
		CMC->NavAgentProps.bCanCrouch = true;
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent(); IsValid(Capsule))
	{
		Capsule->SetCapsuleRadius(32.f);
	}
}

void AGC_GenericCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	CameraComponent = GetComponentByClass<UCameraComponent>();

	// Store initial data so that on first frame everything is correct
	{
		EyeHeightFromFeet = GetSimpleCollisionHalfHeight() + BaseEyeHeight;

		if (UCharacterMovementComponent* CMC = GetCharacterMovement(); IsValid(CMC))
		{
			StoredWalkSpeed = CMC->MaxWalkSpeed;
			StoredGravityScale = CMC->GravityScale;
		}
	}
}

#if WITH_EDITOR
void AGC_GenericCharacter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property) return;

	FName PropertyName = PropertyChangedEvent.Property->GetFName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AGC_GenericCharacter, JumpHeight))
	{
		SetJumpHeight(JumpHeight);
	}
}
#endif

void AGC_GenericCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Bind callbacks
	{
		OnCharacterMovementUpdated.AddDynamic(this, &AGC_GenericCharacter::OnCmcUpdated);
	}
}

void AGC_GenericCharacter::OnCmcUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	TickCrouchState(DeltaSeconds);
	TickSlideState(DeltaSeconds);

	TickEyeHeight(DeltaSeconds);

	TickCmc(DeltaSeconds, OldLocation, OldVelocity);

	if (bDebugEyeHeight)
	{
		const float Radius = GetSimpleCollisionRadius() * 0.75f;
		const float Height = 2.5f; // cm

		const FVector Extent = { Radius, Radius, Height };

		DrawDebugBox(GetWorld(), GetPawnViewLocation(), Extent, GetActorQuat(), FColor::Blue);
	}
}

void AGC_GenericCharacter::TickEyeHeight(float DeltaSeconds)
{
	// Ignore un-set values
	if (EyeHeightInterpDuration <= 0.f || TargetEyeHeightRange.IsZero())
	{
		return;
	}

	EyeHeightInterpTime += DeltaSeconds;

	// Default to time% (linear percentage)
	float EyeHeightPercent = FMath::Clamp(EyeHeightInterpTime / EyeHeightInterpDuration, 0.f, 1.f);

	// If curve is valid, re-map time% -> height%
	if (IsValid(EyeHeightInterpCurve))
	{
		EyeHeightPercent = EyeHeightInterpCurve->GetFloatValue(EyeHeightPercent);
	}
	
	EyeHeightFromFeet = FMath::Lerp(TargetEyeHeightRange.X, TargetEyeHeightRange.Y, EyeHeightPercent);

	// We have finished interp
	if (EyeHeightPercent == 1.f)
	{
		EyeHeightInterpTime = EyeHeightInterpDuration;
		EyeHeightInterpDuration = 0.f;

		// True one shot delegate
		OnReachedEyeHeightTarget.ExecuteIfBound();
		OnReachedEyeHeightTarget.Unbind();
	}
}

void AGC_GenericCharacter::SetEyeHeightTarget(FVector2D TargetRange, float Duration, UCurveFloat* TimeHeightCurve, FOnReachedEyeHeightTargetCallback ReachedTargetCallback)
{
	float CurrentHeightPercent = FMath::GetMappedRangeValueClamped(TargetRange, FVector2D(0.f, 1.f), EyeHeightFromFeet);

	EyeHeightInterpTime = (CurrentHeightPercent * Duration);
	TargetEyeHeightRange = TargetRange;
	EyeHeightInterpDuration = Duration;

	// This is allowed to be null, as we check it in 'TickEyeHeight' anyway
	EyeHeightInterpCurve = TimeHeightCurve;

	if (ReachedTargetCallback)
	{
		OnReachedEyeHeightTarget.BindUObject(this, ReachedTargetCallback);
	}
}

FVector AGC_GenericCharacter::GetPawnViewLocation() const
{
	const float EyeHeightFromCenter = EyeHeightFromFeet - GetSimpleCollisionHalfHeight();
	return GetActorLocation() + (GetActorUpVector() * EyeHeightFromCenter);
}

void AGC_GenericCharacter::RecalculateBaseEyeHeight()
{
	// Do not recalculate, as we manually track eye height
}

void AGC_GenericCharacter::OnWalkingOffLedge_Implementation(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta)
{
	Super::OnWalkingOffLedge_Implementation(PreviousFloorImpactNormal, PreviousFloorContactNormal, PreviousLocation, TimeDelta);

	if (UWorld* World = GetWorld(); IsValid(World))
	{
		TimeWalkedOffLedge = World->TimeSeconds; // stored mainly for coyote time logic
	}

	if (IsInSlideState())
	{
		bDidSlideOffLedge = true;
	}
}

void AGC_GenericCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if (UCharacterMovementComponent* CMC = GetCharacterMovement(); IsValid(CMC))
	{
		// Falling -> Walking = Landed. We do not use 'Landed' event as
		// that happens while we are still in the 'Falling' state.
		if (PrevMovementMode == EMovementMode::MOVE_Falling && CMC->MovementMode == EMovementMode::MOVE_Walking)
		{
			if (IsInJumpBufferWindow())
			{
				Jump();
			}

			if (bUseSeparateJumpGravity)
			{
				CMC->GravityScale = StoredGravityScale;
			}
		}

		// Walking -> Falling
		if (PrevMovementMode == EMovementMode::MOVE_Walking && CMC->MovementMode == EMovementMode::MOVE_Falling)
		{
			if (bPressedJump)
			{
				CMC->bNotifyApex = true; // we want to use jump apex callbacks
			}
		}
	}
}

bool AGC_GenericCharacter::CheckMovementCapability(EGC_MovementCapability CapabilityToCheck) const
{
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!IsValid(CMC))
	{
		return false;
	}

	switch (CapabilityToCheck)
	{
		case EGC_MovementCapability::Crouch: return CMC->CanEverCrouch();
		case EGC_MovementCapability::Jump: return CMC->CanEverJump();
		case EGC_MovementCapability::Walk: return CMC->CanEverMoveOnGround();
		case EGC_MovementCapability::Swim: return CMC->CanEverSwim();
		case EGC_MovementCapability::Fly: return CMC->CanEverFly();
		default:
		{
			return false;
		}
	}
}

bool AGC_GenericCharacter::IsCharacterFalling() const
{
	if (UCharacterMovementComponent* CMC = GetCharacterMovement(); IsValid(CMC))
	{
		return CMC->IsFalling();
	}

	return false;
}

void AGC_GenericCharacter::SetCapsuleHalfHeight(float HalfHeight, bool bScaleFromBottom)
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent(); IsValid(Capsule))
	{
		float CapsuleRadius = Capsule->GetUnscaledCapsuleRadius();
		if (HalfHeight < CapsuleRadius)
		{
			PrintDebugMessage(FString::Format(TEXT("Tried to set capsule half height to '{0}' but was clamped to capsule radius '{1}'!"), { HalfHeight, CapsuleRadius }));
		}

		float PreviousHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
		Capsule->SetCapsuleHalfHeight(HalfHeight);

		// Scaling happens from capsule center, so we offset it
		// to appear to scale from the capsules' lowest point
		if (bScaleFromBottom)
		{
			float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
			float DeltaHalfHeight = CurrentHalfHeight - PreviousHalfHeight; // sign of this tells us up/down

			Capsule->AddRelativeLocation(GetActorUpVector() * DeltaHalfHeight, true);
		}
	}
}

void AGC_GenericCharacter::CheckCapsuleHeight(FHitResult& OutHit, float HalfHeight, bool bScaleHeight, bool bCheckFromBottom, EDrawDebugTrace::Type DrawDebug)
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!IsValid(Capsule))
	{
		PrintDebugMessage(TEXT("'AGC_GenericCharacter::CheckCapsuleHeight' capsule component is invalid!"));
		return;
	}

	if (HalfHeight <= 0.f)
	{
		PrintDebugMessage(FString::Format(TEXT("'AGC_GenericCharacter::CheckCapsuleHeight' input invalid! HalfHeight: {0}"), { HalfHeight }));
		return;
	}

	UWorld* WorldObj = GetWorld();
	if (!IsValid(WorldObj))
	{
		PrintDebugMessage(TEXT("'AGC_GenericCharacter::CheckCapsuleHeight' world is invalid!"));
		return;
	}

	// Apply current component scale to input height
	if (bScaleHeight)
	{
		FVector CurrentScale = Capsule->GetComponentTransform().GetScale3D();
		HalfHeight *= CurrentScale.Z;
	}

	// Offset capsule so bounds are pivoted from our feet
	FVector CapsuleCenter = GetActorLocation();
	if (bCheckFromBottom)
	{
		float DeltaHalfHeight = HalfHeight - Capsule->GetScaledCapsuleHalfHeight();
		CapsuleCenter += GetActorUpVector() * DeltaHalfHeight;
	}

	// Set params to ignore this actor in the bounds check
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	// Perform bounds check
	WorldObj->SweepSingleByChannel(
		OutHit,
		CapsuleCenter, CapsuleCenter,	// static sweep
		Capsule->GetComponentQuat(),	// rotate sweep to be aligned with capsule component
		ECC_Visibility,
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), HalfHeight),
		Params
	);

	// Finally, should we draw a debug shape?
	if (DrawDebug != EDrawDebugTrace::None)
	{
		FColor DebugColor = (OutHit.bBlockingHit ? FColor::Red : FColor::Green);
		bool bPersistentLines = (DrawDebug == EDrawDebugTrace::Persistent);
		float DebugLifeTime = (DrawDebug == EDrawDebugTrace::ForDuration ? 3.f : -1.f);

		DrawDebugCapsule(WorldObj, CapsuleCenter, HalfHeight, Capsule->GetScaledCapsuleRadius(), Capsule->GetComponentQuat(), DebugColor, bPersistentLines, DebugLifeTime);
	}
}

void AGC_GenericCharacter::OnMove_Implementation(const FVector2D& MoveDirection)
{
	CHECK_VALID(CameraComponent);

	FVector FlattenedRight = FVector::VectorPlaneProject(CameraComponent->GetRightVector(), FVector::UpVector);
	FVector FlattenedForward = FVector::VectorPlaneProject(CameraComponent->GetForwardVector(), FVector::UpVector);

	// Normalize these, otherwise camera pitch affects move speed
	FlattenedRight.Normalize();
	FlattenedForward.Normalize();

	AddMovementInput(FlattenedRight, MoveDirection.X);
	AddMovementInput(FlattenedForward, MoveDirection.Y);
}

void AGC_GenericCharacter::OnLook_Implementation(FVector2D LookDirection)
{
	LookDirection *= SensitivityMultiplier;
	LookDirection.Y *= (bInvertY ? -1.f : 1.f);

	AddControllerYawInput(LookDirection.X);
	AddControllerPitchInput(LookDirection.Y);
}

void AGC_GenericCharacter::OnJump_Implementation()
{
	Jump();

	if (UWorld* World = GetWorld(); IsValid(World))
	{
		TimeJumpInputPressed = World->TimeSeconds; // stored mainly for jump buffer logic
	}
}

void AGC_GenericCharacter::SetJumpHeight(float NewHeight)
{
	JumpHeight = NewHeight;

	UCharacterMovementComponent* CMC = GetCharacterMovement();
	CHECK_VALID(CMC);

	// jumpForce = sqrt(2*g*h)
	CMC->JumpZVelocity = FMath::Sqrt(FMath::Abs(2.f * CMC->GetGravityZ() * JumpHeight));
}

bool AGC_GenericCharacter::IsInCoyoteTimeWindow() const
{
	if (!bUseCoyoteTime) // not using coyote time
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World)) // world is somehow invalid
	{
		return false;
	}

	// Actual coyote time check
	double TimeSinceWalkedOffLedge = World->TimeSeconds - TimeWalkedOffLedge;
	return (TimeSinceWalkedOffLedge <= CoyoteTimeDuration);
}

bool AGC_GenericCharacter::IsInJumpBufferWindow() const
{
	if (!bUseJumpBuffer) // not using jump buffer
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World)) // world is somehow invalid
	{
		return false;
	}

	// Actual jump buffer check
	double TimeSinceWalkedOffLedge = World->TimeSeconds - TimeJumpInputPressed;
	return (TimeSinceWalkedOffLedge <= JumpBufferDuration);
}

bool AGC_GenericCharacter::CanJumpInternal_Implementation() const
{
	// Is jump allowed while we are crouched? If not crouched, this is always true.
	bool bJumpWhileCrouched = IsInCrouchedState()
		? bAllowJumpWhileCrouched
		: true;

	// Offset default Unreal behaviour, as first jump usually does JumpCurrentCount += 2
	bool bWithinJumpCount = (bAllowFirstJumpWhileFalling || IsInCoyoteTimeWindow())
		? (JumpCurrentCountPreJump < JumpMaxCount)
		: (JumpCurrentCount < JumpMaxCount);

	bool bCustomJumpRestrictions = (bJumpWhileCrouched && bWithinJumpCount);

	// If our custom restrictions fail, default to normal Unreal checks
	return bCustomJumpRestrictions || JumpIsAllowedInternal();
}

void AGC_GenericCharacter::CheckJumpInput(float DeltaTime)
{
	Super::CheckJumpInput(DeltaTime);

	// Offset default Unreal behaviour, as first jump usually does JumpCurrentCount += 2
	if (bAllowFirstJumpWhileFalling || IsInCoyoteTimeWindow())
	{
		if (JumpCurrentCount - JumpCurrentCountPreJump > 1)
		{
			JumpCurrentCount--;
		}
	}
}

void AGC_GenericCharacter::NotifyJumpApex()
{
	Super::NotifyJumpApex();

	UCharacterMovementComponent* CMC = GetCharacterMovement();
	CHECK_VALID(CMC);

	if (bUseSeparateJumpGravity)
	{
		StoredGravityScale = CMC->GravityScale;
		CMC->GravityScale = JumpGravityScale;
	}
}

void AGC_GenericCharacter::OnStartCrouch_Implementation()
{
	if (CrouchInput != EGC_InputMode::Toggle)
	{
		SetCrouched(true);
	}
}

void AGC_GenericCharacter::OnEndCrouch_Implementation()
{
	if (CrouchInput != EGC_InputMode::Toggle)
	{
		SetCrouched(false);
	}
}

void AGC_GenericCharacter::OnToggleCrouch_Implementation()
{
	// Check toggle input is allowed
	if (CrouchInput == EGC_InputMode::Hold)
	{
		return;
	}

	switch (CrouchState)
	{
		case EGC_CrouchState::Uncrouched:
		case EGC_CrouchState::InterpToUncrouched:
		case EGC_CrouchState::FallingRequestUncrouched:
		{
			SetCrouched(true);
			break;
		}
		case EGC_CrouchState::Crouched:
		case EGC_CrouchState::InterpToCrouched:
		case EGC_CrouchState::FallingRequestCrouched:
		{
			SetCrouched(false);
			break;
		}
	}
}

bool AGC_GenericCharacter::IsInCrouchedState() const
{
	// We check our own crouched state first, but also need to check Unreal's
	// crouched state in the case that the player is trying to uncrouch.
	return (CrouchState != EGC_CrouchState::Uncrouched) || IsCrouched();
}

void AGC_GenericCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// Unreal scales the capsule slightly differently when mid-air,
	// so we add an offset to actor location to account for this
	if (IsCharacterFalling())
	{
		AddActorLocalOffset(-GetActorUpVector() * ScaledHalfHeightAdjust, true);
	}
}

void AGC_GenericCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// Unreal scales the capsule slightly differently when mid-air,
	// so we add an offset to actor location to account for this
	if (IsCharacterFalling())
	{
		AddActorLocalOffset(GetActorUpVector() * ScaledHalfHeightAdjust, true);
	}

	// Interp after Unreal has scaled our capsule to standing height.
	// This avoids any possible eye height clipping issues
	CrouchState = EGC_CrouchState::InterpToUncrouched;

	UCharacterMovementComponent* CMC = GetCharacterMovement();
	SetEyeHeightTarget({ CMC->CrouchedHalfHeight + CrouchedEyeHeight, GetDefaultHalfHeight() + BaseEyeHeight }, CrouchDuration, ExitCrouchCurve, &AGC_GenericCharacter::OnFinishInterpCrouch);
}

void AGC_GenericCharacter::SetCrouched(bool bNewState)
{
	// Double-check crouching is enabled
	if (!CheckMovementCapability(EGC_MovementCapability::Crouch))
	{
		return;
	}

	// We are falling, but crouch is blocked while falling
	if (IsCharacterFalling() && !bAllowCrouchWhileFalling)
	{
		// Instead put crouch in a 'request' state, so that when
		// we land we trigger the correct crouch interp state
		if (bNewState)
		{
			CrouchState = EGC_CrouchState::FallingRequestCrouched;
		}
		else
		{
			CrouchState = EGC_CrouchState::FallingRequestUncrouched;
		}

		return;
	}

	if (bNewState)
	{
		// Interp before Unreal has scaled our capsule to crouched height.
		// This avoids any possible eye height clipping issues
		CrouchState = EGC_CrouchState::InterpToCrouched;

		UCharacterMovementComponent* CMC = GetCharacterMovement();
		SetEyeHeightTarget({ GetDefaultHalfHeight() + BaseEyeHeight, CMC->CrouchedHalfHeight + CrouchedEyeHeight }, CrouchDuration, EnterCrouchCurve, &AGC_GenericCharacter::OnFinishInterpCrouch);
	}
	else
	{
		if (CrouchState == EGC_CrouchState::InterpToCrouched)
		{
			// Mid-way through interp, just reverse direction
			CrouchState = EGC_CrouchState::InterpToUncrouched;

			UCharacterMovementComponent* CMC = GetCharacterMovement();
			SetEyeHeightTarget({ CMC->CrouchedHalfHeight + CrouchedEyeHeight, GetDefaultHalfHeight() + BaseEyeHeight }, CrouchDuration, ExitCrouchCurve, &AGC_GenericCharacter::OnFinishInterpCrouch);
		}
		else
		{
			// Use Unreal 'OnEndCrouch' to defer 'InterpToUncrouched' logic
			CrouchState = EGC_CrouchState::Uncrouched;
		}
	}
}

void AGC_GenericCharacter::TickCrouchState(float DeltaSeconds)
{
	switch (CrouchState)
	{
		case EGC_CrouchState::Uncrouched:
		{
			if (UCharacterMovementComponent* CMC = GetCharacterMovement(); IsValid(CMC))
			{
				CMC->bWantsToCrouch = false;
			}

			break;
		}
		case EGC_CrouchState::Crouched:
		{
			if (UCharacterMovementComponent* CMC = GetCharacterMovement(); IsValid(CMC))
			{
				CMC->bWantsToCrouch = true;
			}

			break;
		}
		case EGC_CrouchState::FallingRequestCrouched:
		{
			if (!IsCharacterFalling())
			{
				SetCrouched(true);
			}

			break;
		}
		case EGC_CrouchState::FallingRequestUncrouched:
		{
			if (!IsCharacterFalling())
			{
				SetCrouched(false);
			}

			break;
		}
	}
}

void AGC_GenericCharacter::OnFinishInterpCrouch()
{
	if (CrouchState == EGC_CrouchState::InterpToCrouched)
	{
		// This is an edge case where player exited slide -> start crouch
		// But we still need slide to reset friction values
		if (IsInSlideState())
		{
			SlideExitState = EGC_SlideExitType::Invalid;
			FinishInterpExitSlide();
		}

		CrouchState = EGC_CrouchState::Crouched;
	}
	else if (CrouchState == EGC_CrouchState::InterpToUncrouched)
	{
		CrouchState = EGC_CrouchState::Uncrouched;
	}
	else
	{
		PrintDebugMessage("'OnFinishInterpCrouch' called while in a non-interp crouch state!");
	}
}

void AGC_GenericCharacter::OnStartSprint_Implementation()
{
	if (SprintInput != EGC_InputMode::Toggle)
	{
		SetSprintState(true);
	}
}

void AGC_GenericCharacter::OnEndSprint_Implementation()
{
	if (SprintInput != EGC_InputMode::Toggle)
	{
		SetSprintState(false);
	}
}

void AGC_GenericCharacter::OnToggleSprint_Implementation()
{
	if (SprintInput != EGC_InputMode::Hold)
	{
		SetSprintState(!bIsSprinting);
	}
}

void AGC_GenericCharacter::SetSprintState(bool bNewState)
{
	// Already in the correct sprint state
	if (bIsSprinting == bNewState)
	{
		return;
	}

	// We are crouched, but sprint is blocked while crouching
	if (IsInCrouchedState() && !bAllowSprintWhileCrouched)
	{
		return;
	}

	bIsSprinting = bNewState;

	if (UCharacterMovementComponent* CMC = GetCharacterMovement(); IsValid(CMC))
	{
		if (bIsSprinting) // enter sprint
		{
			StoredWalkSpeed = CMC->MaxWalkSpeed;
			CMC->MaxWalkSpeed = SprintSpeed;
		}
		else // exit sprint
		{
			CMC->MaxWalkSpeed = StoredWalkSpeed;
		}
	}
}

void AGC_GenericCharacter::OnStartSlide_Implementation()
{
	// Already sliding, can not enter slide
	if (SlideState <= EGC_SlideState::Sliding)
	{
		return;
	}

	// Some condition failed, can not enter slide
	if (!CanSlide())
	{
		return;
	}

	// Not interping, run start logic
	if (SlideState == EGC_SlideState::NotSliding)
	{
		// This is where CMC data gets stored, so only run it when we first enter slide
		StartInterpEnterSlide();
	}

	SlideState = EGC_SlideState::InterpEnter;

	SetEyeHeightTarget({ GetDefaultHalfHeight() + BaseEyeHeight, SlideHalfHeight + SlideEyeHeight }, SlideEnterDuration, SlideEnterCurve, &AGC_GenericCharacter::OnFinishInterpSlide);
}

void AGC_GenericCharacter::OnEndSlide_Implementation()
{
	// Already not sliding, can not exit slide
	if (SlideState == EGC_SlideState::NotSliding)
	{
		// Edge case where slide was previously auto-exited via falling off a ledge.
		// In this case, player wants to run manual exit slide logic... skip early return.
		bool bPreviouslyFellOffLedge = (IsCharacterFalling() && bWasSlideAutoExited);

		if (!bPreviouslyFellOffLedge)
		{
			return;
		}
	}

	// Attempt logic is contained within the 'AttemptSlideExit' function
	GetWorldTimerManager().SetTimerForNextTick(this, &AGC_GenericCharacter::AttemptSlideExit);

	bWasSlideAutoExited = false;
}

void AGC_GenericCharacter::OnFailedSlideExit_Implementation(const FVector& DepenetrationVector)
{
	// A depenetration vector has been computed, move out of collision
	if (!DepenetrationVector.IsNearlyZero())
	{
		PrintDebugMessage("'OnFailedSlideExit' called with non-zero depenetration vector, trying to move out of collision.");

		FHitResult SweepResult;
		AddActorWorldOffset(DepenetrationVector, true, &SweepResult);

		if (!SweepResult.bBlockingHit)
		{
			return;
		}
	}

	PrintDebugMessage("'OnFailedSlideExit' failed to move out of collision, slide exit has completely failed!");
}

bool AGC_GenericCharacter::CanSlide_Implementation() const
{
	if (IsCharacterFalling()) // no sliding mid-air
	{
		return false;
	}

	float FlattenedSpeed = FVector::VectorPlaneProject(GetVelocity(), GetActorUpVector()).Length();
	if (FlattenedSpeed < SlideAutoExitSpeed) // below min slide speed
	{
		return false;
	}

	return true;
}

bool AGC_GenericCharacter::IsInSlideState() const
{
	return SlideState != EGC_SlideState::NotSliding;
}

void AGC_GenericCharacter::TickSlideState(float DeltaSeconds)
{
	// We are currently sliding, but some slide condition failed, force exit slide
	if (SlideState <= EGC_SlideState::Sliding && !CanSlide())
	{
		OnEndSlide();
		bWasSlideAutoExited = true; // override 'OnEndSlide' setting this to 'false'
	}
}

void AGC_GenericCharacter::StartInterpEnterSlide()
{
	SlideState = EGC_SlideState::InterpEnter;

	// Store CMC variables and set them to sliding variables
	if (UCharacterMovementComponent* CMC = GetCharacterMovement(); IsValid(CMC))
	{
		bStoredUseSeparateBrakingFriction = CMC->bUseSeparateBrakingFriction;
		StoredBrakingFriction = CMC->BrakingFriction;
		StoredBrakingDeceleration = CMC->BrakingDecelerationWalking;

		CMC->bUseSeparateBrakingFriction = true;
		CMC->BrakingFriction = SlideGroundFriction;
		CMC->BrakingDecelerationWalking = SlideBrakingDeceleration;

		// Add an optional boost when we enter slide.
		// This has to happen when we start interp, otherwise spamming gives infinite boost.
		if (InitialSlideBoost > 0.f)
		{
			CMC->AddImpulse(GetActorForwardVector() * InitialSlideBoost, true);
		}
	}

	// Ignore move input while sliding
	// We need to explicitly check the 'ignore move input' state as it is an integer
	// that is incremented/decremented but we need it to behave like a boolean (0/1)
	if (IsValid(Controller) && !Controller->IsMoveInputIgnored())
	{
		Controller->SetIgnoreMoveInput(true);
	}

	// Cancel sprint
	if (bCancelSprintWhenSliding)
	{
		SetSprintState(false);
	}
}

void AGC_GenericCharacter::FinishInterpEnterSlide()
{
	SlideState = EGC_SlideState::Sliding;

	// We set capsule height after eye height interp,
	// this avoids any eye height clipping issues
	SetCapsuleHalfHeight(SlideHalfHeight, true);
}

void AGC_GenericCharacter::StartInterpExitSlide()
{
	SlideState = EGC_SlideState::InterpExit;

	// We set capsule height before eye height interp,
	// this avoids any eye height clipping issues

	if (SlideExitState == EGC_SlideExitType::IntoCrouched)
	{
		if (UCharacterMovementComponent* CMC = GetCharacterMovement(); IsValid(CMC))
		{
			SetCapsuleHalfHeight(CMC->CrouchedHalfHeight, true);
			SetEyeHeightTarget({ SlideHalfHeight + SlideEyeHeight, CMC->CrouchedHalfHeight + CrouchedEyeHeight }, SlideExitToCrouchedDuration, SlideExitToCrouchedCurve, &AGC_GenericCharacter::OnFinishInterpSlide);

			return;
		}
	}
	
	// Default to 'standing' height
	SetCapsuleHalfHeight(GetDefaultHalfHeight(), true);
	SetEyeHeightTarget({ SlideHalfHeight + SlideEyeHeight, GetDefaultHalfHeight() + BaseEyeHeight }, SlideExitToStandingDuration, SlideExitToStandingCurve, &AGC_GenericCharacter::OnFinishInterpSlide);
}

void AGC_GenericCharacter::FinishInterpExitSlide()
{
	SlideState = EGC_SlideState::NotSliding;

	// Restore CMC variables
	if (UCharacterMovementComponent* CMC = GetCharacterMovement(); IsValid(CMC))
	{
		CMC->bUseSeparateBrakingFriction = bStoredUseSeparateBrakingFriction;
		CMC->BrakingFriction = StoredBrakingFriction;
		CMC->BrakingDecelerationWalking = StoredBrakingDeceleration;
	}

	// Re-enable move input
	// We need to explicitly check the 'ignore move input' state as it is an integer
	// that is incremented/decremented but we need it to behave like a boolean (0/1)
	if (IsValid(Controller) && Controller->IsMoveInputIgnored())
	{
		Controller->SetIgnoreMoveInput(false);
	}

	// Since slide runs its' own eye height interp, we must manually manage crouch state
	if (SlideExitState == EGC_SlideExitType::IntoCrouched)
	{
		bool bShouldExitToCrouched = SlideShouldExitToCrouched();

		// Player has slid off of a ledge, but we are not allowed to 'uncrouch' while mid-air
		if (bDidSlideOffLedge && !bShouldExitToCrouched)
		{
			Crouch();
			CrouchState = EGC_CrouchState::FallingRequestUncrouched;
		}
		else
		{
			// Either player wants to enter crouch, or they are still holding crouch input
			if (bShouldExitToCrouched || bWasSlideAutoExited)
			{
				CrouchState = EGC_CrouchState::Crouched;
			}
			else
			{
				// If we reach this point, player doesn't want to crouch but was forced to (under a ledge)
				Crouch();
				CrouchState = EGC_CrouchState::Uncrouched;
			}
		}
	}
}

EGC_SlideExitType AGC_GenericCharacter::FindSuitableSlideExitType(FVector& OutDepenetrationOffset)
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	UCharacterMovementComponent* CMC = GetCharacterMovement();

	if (!IsValid(Capsule) || !IsValid(CMC))
	{
		PrintDebugMessage("Invalid capsule / movement component in 'AGC_GenericCharacter::FindSuitableSlideExitType'!");
		return EGC_SlideExitType::Invalid;
	}

	// Standing
	FHitResult HitResult;
	CheckCapsuleHeight(HitResult, GetDefaultHalfHeight(), false, true, DebugSlideExitCheck);

	// Note: if falling, favor crouched exit
	if (!(IsCharacterFalling() || SlideShouldExitToCrouched() || HitResult.bBlockingHit))
	{
		return EGC_SlideExitType::IntoStanding;
	}
	
	// Add a little offset so that player will definitely be out of collision
	HitResult.PenetrationDepth += 5.f;

	// Use standing bounds check for depenetration vector, as the crouching bounds
	// check usually outputs a downwards depenetration vector, but we want sideways only.
	OutDepenetrationOffset = FVector::VectorPlaneProject(HitResult.PenetrationDepth * HitResult.Normal, GetActorUpVector());

	// Crouching
	CheckCapsuleHeight(HitResult, CMC->CrouchedHalfHeight, true, true, DebugSlideExitCheck);
	if (!HitResult.bBlockingHit)
	{
		return EGC_SlideExitType::IntoCrouched;
	}

	return EGC_SlideExitType::Invalid;
}

void AGC_GenericCharacter::AttemptSlideExit()
{
	SlideExitAttempt++;

	FVector DepenetrationVector;
	SlideExitState = FindSuitableSlideExitType(DepenetrationVector);

	// Failed to exit slide
	if (SlideExitState == EGC_SlideExitType::Invalid)
	{
		// Try to exit slide after interval, X times
		if (SlideExitAttempt < SlideMaxExitAttempts)
		{
			FTimerHandle Handle;
			GetWorldTimerManager().SetTimer(Handle, this, &AGC_GenericCharacter::AttemptSlideExit, SlideExitAttemptInterval);

			return;
		}
		// Still can not exit slide after X attempts
		else
		{
			OnFailedSlideExit(DepenetrationVector);

			// Set default slide exit type
			SlideExitState = (SlideShouldExitToCrouched() ? EGC_SlideExitType::IntoCrouched : EGC_SlideExitType::IntoStanding);
		}
	}

	// Reset attempt count
	SlideExitAttempt = 0;

	StartInterpExitSlide();
}

bool AGC_GenericCharacter::SlideShouldExitToCrouched() const
{
	if (bWasSlideAutoExited)
	{
		return bSlideAutoExitToCrouched;
	}

	return bSlideExitToCrouched;
}

void AGC_GenericCharacter::OnFinishInterpSlide()
{
	if (SlideState == EGC_SlideState::InterpEnter)
	{
		FinishInterpEnterSlide();
	}
	else if (SlideState == EGC_SlideState::InterpExit)
	{
		FinishInterpExitSlide();
	}
	else
	{
		PrintDebugMessage("'OnFinishInterpSlide' called while in a non-interp slide state!");
	}

	// This must happen last so we are able to check its'
	// state in one of the finish interp functions.
	bDidSlideOffLedge = false;
}

