// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quick_RoadModule.h"
#include "Quick_RoadEditorModeCommands.h"
#include "Quick_RoadLocalization.h"
#include "Quick_RoadLog.h"

#define LOCTEXT_NAMESPACE "Quick_RoadModule"

DEFINE_LOG_CATEGORY(LogQuickRoad);

void FQuick_RoadModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FQuick_RoadEditorModeCommands::Register();
	QuickRoadLocalization::Initialize();
}

void FQuick_RoadModule::ShutdownModule()
{
	QuickRoadLocalization::Shutdown();

	FQuick_RoadEditorModeCommands::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FQuick_RoadModule, Quick_Road)