// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"

class SQuick_RoadModePanel;
class UInteractiveTool;
class UInteractiveToolManager;

class FQuick_RoadEditorModeToolkit : public FModeToolkit
{
public:
	FQuick_RoadEditorModeToolkit();

	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	virtual void GetToolPaletteNames(TArray<FName>& PaletteNames) const override;
	virtual FText GetToolPaletteDisplayName(FName PaletteName) const override;
	virtual bool HasIntegratedToolPalettes() const override { return true; }
	virtual TSharedPtr<SWidget> GetInlineContent() const override;

	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;

protected:
	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;

private:
	void BuildModePanel();

	TSharedPtr<SQuick_RoadModePanel> ModePanel;
	TSharedPtr<SWidget> ToolkitWidget;
};
