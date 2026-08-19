// Copyright © 2026 Ethan Dawkins. All rights reserved.


#include "GC_GenericCharacter.h"

// Unreal
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AGC_GenericCharacter::AGC_GenericCharacter()
{
#if WITH_EDITOR
	// Add a custom section filter to property window. Note this only shows
	// up if there is at least 1 property in one of the supported categories.
    FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

    TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(
		"GC_GenericCharacter",					// class name
		"Generic Character",					// section internal name
		FText::FromString("Generic Character")	// section display name
	);

	// Add supported categories to section filter. Note this does not
	// allow sub-categories via '|', so only specify top-level categories.
    Section->AddCategory("GenericCharacter");
#endif

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

	// Store this initially so that 'GetPawnViewLocation' is corrected for first frame
	EyeHeightFromFeet = GetSimpleCollisionHalfHeight() + BaseEyeHeight;
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

void AGC_GenericCharacter::SetJumpHeight(float NewHeight)
{
	JumpHeight = NewHeight;

	UCharacterMovementComponent* CMC = GetCharacterMovement();
	CHECK_VALID(CMC);

	// jumpForce = sqrt(2*g*h)
	CMC->JumpZVelocity = FMath::Sqrt(FMath::Abs(2.f * CMC->GetGravityZ() * JumpHeight));
}

void AGC_GenericCharacter::TickCrouchState(float DeltaSeconds)
{
	switch (CrouchState)
	{
		case EGC_CrouchState::Crouched:
		{
			if (!bIsCrouched)
			{
				Crouch();
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

void AGC_GenericCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// Interp after Unreal has scaled our capsule to standing height.
	// This avoids any possible eye height clipping issues
	CrouchState = EGC_CrouchState::InterpToUncrouched;
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
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	CHECK_VALID(CMC);

	// Custom jump logic, as standard Unreal jump has some annoying edge cases
	if (JumpCurrentCount < MaxJumpCount)
	{
		LaunchCharacter(GetActorUpVector() * CMC->JumpZVelocity, false, true);

		JumpCurrentCount++;
	}
}

void AGC_GenericCharacter::OnStartCrouch_Implementation()
{
	// Double-check crouching is enabled
	if (UCharacterMovementComponent* CMC = GetCharacterMovement(); IsValid(CMC))
	{
		if (!CMC->CanEverCrouch())
		{
			return;
		}
	}

	CanCrouch();

	// Interp before Unreal has scaled our capsule to crouched height.
	// This avoids any possible eye height clipping issues
	CrouchState = EGC_CrouchState::InterpToCrouched;
}

void AGC_GenericCharacter::OnEndCrouch_Implementation()
{
	// Double-check crouching is enabled
	if (UCharacterMovementComponent* CMC = GetCharacterMovement(); IsValid(CMC))
	{
		if (!CMC->CanEverCrouch())
		{
			return;
		}
	}

	// Because of needing crouch interp order to be exact,
	// if we are mid-way crouched do not call 'UnCrouch' yet
	if (CrouchState == EGC_CrouchState::Crouched)
	{
		UnCrouch();
	}
	else
	{
		CrouchState = EGC_CrouchState::InterpToUncrouched;
	}
}

void AGC_GenericCharacter::OnToggleCrouch_Implementation()
{
	switch (CrouchState)
	{
		case EGC_CrouchState::Uncrouched:
		case EGC_CrouchState::InterpToUncrouched:
		{
			OnStartCrouch();
			break;
		}
		case EGC_CrouchState::Crouched:
		case EGC_CrouchState::InterpToCrouched:
		{
			OnEndCrouch();
			break;
		}
	}
}

