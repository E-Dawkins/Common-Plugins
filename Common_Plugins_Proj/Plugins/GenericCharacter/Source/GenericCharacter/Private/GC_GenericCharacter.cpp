// Copyright © 2026 Ethan Dawkins. All rights reserved.


#include "GC_GenericCharacter.h"

// Unreal
#include "Camera/CameraComponent.h"
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

	TickCmc(DeltaSeconds, OldLocation, OldVelocity);
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
	return CrouchState != EGC_CrouchState::Uncrouched;
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
	}
	else
	{
		if (CrouchState == EGC_CrouchState::InterpToCrouched)
		{
			// Mid-way through interp, just reverse direction
			CrouchState = EGC_CrouchState::InterpToUncrouched;
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
		case EGC_CrouchState::InterpToCrouched:
		{
			InterpCrouch(DeltaSeconds);

			break;
		}
		case EGC_CrouchState::InterpToUncrouched:
		{
			InterpCrouch(-DeltaSeconds);

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

void AGC_GenericCharacter::InterpCrouch(float DeltaSeconds)
{
	CrouchTime += DeltaSeconds;

	float CrouchPercent = FMath::Clamp(CrouchTime / CrouchDuration, 0.f, 1.f);

	if (CrouchPercent == 1.f)
	{
		CrouchState = EGC_CrouchState::Crouched;
		CrouchTime = CrouchDuration;
	}
	else if (CrouchPercent == 0.f)
	{
		CrouchState = EGC_CrouchState::Uncrouched;
		CrouchTime = 0.f;
	}

	// Interp eye height
	if (UCharacterMovementComponent* CMC = GetCharacterMovement(); IsValid(CMC))
	{
		float BaseEyeHeightFromFeet = GetDefaultHalfHeight() + BaseEyeHeight;
		float CrouchedEyeHeightFromFeet = CMC->CrouchedHalfHeight + CrouchedEyeHeight;

		// If valid, use curve to map time/height percentages
		if (CrouchState == EGC_CrouchState::InterpToCrouched && IsValid(EnterCrouchCurve))
		{
			CrouchPercent = EnterCrouchCurve->GetFloatValue(CrouchPercent);
		}
		else if (CrouchState == EGC_CrouchState::InterpToUncrouched && IsValid(ExitCrouchCurve))
		{
			CrouchPercent = ExitCrouchCurve->GetFloatValue(1.f - CrouchPercent);
		}

		EyeHeightFromFeet = FMath::Lerp(BaseEyeHeightFromFeet, CrouchedEyeHeightFromFeet, CrouchPercent);
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

