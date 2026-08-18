// Copyright © 2026 Ethan Dawkins. All rights reserved.


#include "GC_GenericCharacter.h"

// Unreal
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AGC_GenericCharacter::AGC_GenericCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;

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
}

void AGC_GenericCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	CameraComponent = GetComponentByClass<UCameraComponent>();
}

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
	// TODO: manipulate 'EyeHeight' here

	TickCmc(DeltaSeconds, OldLocation, OldVelocity);
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

