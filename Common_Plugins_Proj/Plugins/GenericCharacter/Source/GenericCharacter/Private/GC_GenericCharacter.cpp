// Copyright © 2026 Ethan Dawkins. All rights reserved.


#include "GC_GenericCharacter.h"

AGC_GenericCharacter::AGC_GenericCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

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
	
}

void AGC_GenericCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AGC_GenericCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

