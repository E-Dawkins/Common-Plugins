// Copyright © 2026 Ethan Dawkins. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GC_GenericCharacter.generated.h"

UCLASS(meta = (DisplayName = "Generic Character"))
class GENERICCHARACTER_API AGC_GenericCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGC_GenericCharacter();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:

};
