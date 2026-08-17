// Copyright © 2026 Ethan Dawkins. All rights reserved.


#include "GC_GenericCharacter.h"

// Unreal
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Kismet/KismetSystemLibrary.h"

// GC
#include "GC_InputActions.h"

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

void AGC_GenericCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	CameraComponent = GetComponentByClass<UCameraComponent>();
	CHECK_VALID(CameraComponent);
}

void AGC_GenericCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Setup default mapping context
	{
		APlayerController* PlayerController = Cast<APlayerController>(GetController());
		CHECK_VALID(PlayerController);

		UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
		CHECK_VALID(Subsystem);

		CHECK_VALID(DefaultImc);
		Subsystem->ClearAllMappings();
		Subsystem->AddMappingContext(DefaultImc, 0);
	}

	// Bind input actions to events
	{
		UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
		CHECK_VALID(EnhancedInputComponent);

		CHECK_VALID(InputActions);
		EnhancedInputComponent->BindAction(InputActions->Move, ETriggerEvent::Triggered, this, &AGC_GenericCharacter::Input_Move_Native);
	}
}

void AGC_GenericCharacter::Input_Move_Native(const FInputActionValue& Value)
{
	const FVector VectorValue = Value.Get<FVector>();

	AddMovementInput(GetActorRightVector(), VectorValue.X);
	AddMovementInput(GetActorForwardVector(), VectorValue.Y);

	Input_Move_BP(Value);
}

