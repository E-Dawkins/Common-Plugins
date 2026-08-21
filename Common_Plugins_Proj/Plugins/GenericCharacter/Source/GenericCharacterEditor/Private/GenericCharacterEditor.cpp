// Copyright © 2026 Ethan Dawkins. All rights reserved.

#include "GenericCharacterEditor.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FGenericCharacterEditorModule"

static TSharedRef<FPropertySection> AddSection(FPropertyEditorModule& PropertyModule, const FName& ClassName, const FName& SectionName)
{
	// Add a custom section filter to property window. Note this only shows
	// up if there is at least 1 property in one of the supported categories.
	TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(
		ClassName,						// class name
		SectionName,					// section internal name
		FText::FromName(SectionName)	// section display name
	);

	return Section;
}

static void AddCategory(TSharedRef<FPropertySection> Section, const FName& CategoryName)
{
	// Add a new supported category to section filter. Note this does not
	// allow sub-categories via '|', so only specify top-level categories.
	Section->AddCategory(CategoryName);
}

void FGenericCharacterEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	{
		TSharedRef<FPropertySection> Section = AddSection(PropertyModule, "GC_GenericCharacter", "Generic Character");
		AddCategory(Section, "GenericCharacter");
	}
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FGenericCharacterEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGenericCharacterEditorModule, GenericCharacterEditor)