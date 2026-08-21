// Copyright © 2026 Ethan Dawkins. All rights reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FGenericCharacterEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};
