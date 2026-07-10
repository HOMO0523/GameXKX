// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/LegacyEdModeWidgetHelpers.h"
#include "Quick_RoadEditorMode.generated.h"

UCLASS()
class UQuick_RoadEditorMode : public UBaseLegacyWidgetEdMode
{
	GENERATED_BODY()

public:
	const static FEditorModeID EM_Quick_RoadEditorModeId;

	static FString GenerateTerrainToolName;
	static FString HeightfieldNoiseToolName;
	static FString AutoErosionToolName;

	static FString CityScopeToolName;
	static FString RoadToolName;
	static FString RiverToolName;

	static FString BakeToolName;

	UQuick_RoadEditorMode();
	virtual ~UQuick_RoadEditorMode();

	virtual void Enter() override;
	virtual void ActorSelectionChangeNotify() override;
	virtual bool ShouldDrawWidget() const override;
	virtual void CreateToolkit() override;
	virtual TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetModeCommands() const override;
	virtual bool GetCursor(EMouseCursor::Type& OutCursor) const override;
};
