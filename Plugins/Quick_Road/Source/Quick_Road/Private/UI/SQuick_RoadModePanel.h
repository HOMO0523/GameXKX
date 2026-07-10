// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Widgets/SCompoundWidget.h"

class FUICommandList;
class IDetailsView;
class UEdMode;

struct FQuick_RoadCategoryDef
{
	FName CategoryId;
	FText Label;
	const FSlateBrush* IconBrush = nullptr;
	TArray<TSharedPtr<FUICommandInfo>> ToolCommands;
};

class SQuick_RoadModePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SQuick_RoadModePanel) {}
		SLATE_ARGUMENT(TSharedPtr<FUICommandList>, CommandList)
		SLATE_ARGUMENT(TSharedPtr<IDetailsView>, DetailsView)
		SLATE_ARGUMENT(TWeakObjectPtr<UEdMode>, OwningMode)
		SLATE_ARGUMENT(FName, DefaultCategory)
		SLATE_ARGUMENT(TArray<FQuick_RoadCategoryDef>, Categories)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetDetailsVisibility(EVisibility InVisibility);
	void RefreshLocalizedUI();

	virtual ~SQuick_RoadModePanel() override;

private:
	FReply OnCategoryClicked(FName CategoryId);
	FReply OnToolClicked(TSharedPtr<FUICommandInfo> ToolCommand);
	EVisibility GetDetailsVisibility() const;
	bool IsCategorySelected(FName CategoryId) const;
	bool IsToolActive(TSharedPtr<FUICommandInfo> ToolCommand) const;
	void RefreshToolGrid();
	void ActivateDefaultToolForCategory(FName CategoryId);
	void HandleLocalizationChanged();

	TSharedRef<SWidget> BuildIconRail();
	TSharedRef<SWidget> BuildCategoryButton(const FQuick_RoadCategoryDef& Category);
	TSharedRef<SWidget> BuildToolGrid();
	TSharedRef<SWidget> BuildToolButton(TSharedPtr<FUICommandInfo> ToolCommand);

	TSharedPtr<FUICommandList> CommandList;
	TSharedPtr<IDetailsView> DetailsView;
	TWeakObjectPtr<UEdMode> OwningMode;
	TArray<FQuick_RoadCategoryDef> Categories;

	TSharedPtr<SBox> ToolGridContainer;
	TSharedPtr<SBox> IconRailContainer;
	FName ActiveCategory;
	EVisibility DetailsSectionVisibility = EVisibility::Collapsed;
	FDelegateHandle LocalizationChangedHandle;
};
