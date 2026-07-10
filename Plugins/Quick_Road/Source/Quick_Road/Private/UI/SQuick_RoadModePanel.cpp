// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SQuick_RoadModePanel.h"

#include "Quick_RoadLocalization.h"
#include "Tools/UEdMode.h"
#include "EdModeInteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "Framework/Commands/UICommandList.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

void SQuick_RoadModePanel::Construct(const FArguments& InArgs)
{
	CommandList = InArgs._CommandList;
	DetailsView = InArgs._DetailsView;
	OwningMode = InArgs._OwningMode;
	Categories = InArgs._Categories;
	ActiveCategory = InArgs._DefaultCategory;

	if (Categories.Num() > 0 && ActiveCategory.IsNone())
	{
		ActiveCategory = Categories[0].CategoryId;
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(0.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(IconRailContainer, SBox)
				[
					BuildIconRail()
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.Padding(2.f, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.f, 4.f, 6.f, 0.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(ToolGridContainer, SBox)
						[
							BuildToolGrid()
						]
					]
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				.Padding(0.f, 8.f, 0.f, 0.f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
					.Visibility(this, &SQuick_RoadModePanel::GetDetailsVisibility)
					.Padding(2.f)
					[
						DetailsView.ToSharedRef()
					]
				]
			]
		]
	];

	LocalizationChangedHandle = QuickRoadLocalization::OnLocalizationChanged().AddSP(this, &SQuick_RoadModePanel::HandleLocalizationChanged);

	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float)
	{
		ActivateDefaultToolForCategory(ActiveCategory);
		return EActiveTimerReturnType::Stop;
	}));
}

SQuick_RoadModePanel::~SQuick_RoadModePanel()
{
	QuickRoadLocalization::OnLocalizationChanged().Remove(LocalizationChangedHandle);
}

void SQuick_RoadModePanel::SetDetailsVisibility(EVisibility InVisibility)
{
	DetailsSectionVisibility = InVisibility;
	Invalidate(EInvalidateWidgetReason::Visibility);
}

void SQuick_RoadModePanel::RefreshLocalizedUI()
{
	if (IconRailContainer.IsValid())
	{
		IconRailContainer->SetContent(BuildIconRail());
	}
	RefreshToolGrid();
}

void SQuick_RoadModePanel::HandleLocalizationChanged()
{
	RefreshLocalizedUI();
}

TSharedRef<SWidget> SQuick_RoadModePanel::BuildIconRail()
{
	TSharedRef<SVerticalBox> RailBox = SNew(SVerticalBox);

	for (const FQuick_RoadCategoryDef& Category : Categories)
	{
		RailBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			BuildCategoryButton(Category)
		];
	}

	return SNew(SOverlay)

		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush("ToolPalette.DockingWell"))
		]

		+ SOverlay::Slot()
		.Padding(FMargin(4.f, 2.f))
		[
			SNew(SBox)
			.WidthOverride(64.f)
			[
				RailBox
			]
		];
}

TSharedRef<SWidget> SQuick_RoadModePanel::BuildCategoryButton(const FQuick_RoadCategoryDef& Category)
{
	const FSlateBrush* IconBrush = Category.IconBrush ? Category.IconBrush : FAppStyle::GetBrush("Icons.Box");

	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "FVerticalToolBar.ToggleButton")
		.HAlign(HAlign_Fill)
		.IsChecked_Lambda([this, CategoryId = Category.CategoryId]()
		{
			return IsCategorySelected(CategoryId) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([this, CategoryId = Category.CategoryId](ECheckBoxState NewState)
		{
			if (NewState == ECheckBoxState::Checked)
			{
				OnCategoryClicked(CategoryId);
			}
			else
			{
				Invalidate(EInvalidateWidgetReason::Layout);
			}
		})
		.ToolTipText_Lambda([CategoryId = Category.CategoryId]()
		{
			return QuickRoadLocalization::GetCategoryLabel(CategoryId);
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.AutoHeight()
			[
				SNew(SImage)
				.Image(IconBrush)
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([CategoryId = Category.CategoryId]()
				{
					return QuickRoadLocalization::GetCategoryLabel(CategoryId);
				})
				.Font(FAppStyle::GetFontStyle("SmallText"))
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
}

TSharedRef<SWidget> SQuick_RoadModePanel::BuildToolGrid()
{
	const FQuick_RoadCategoryDef* ActiveCategoryDef = Categories.FindByPredicate(
		[this](const FQuick_RoadCategoryDef& Category)
		{
			return Category.CategoryId == ActiveCategory;
		});

	if (!ActiveCategoryDef)
	{
		return SNullWidget::NullWidget;
	}

	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel)
		.SlotPadding(FMargin(0.f))
		.MinDesiredSlotWidth(150.f)
		.MinDesiredSlotHeight(33.f);

	int32 Column = 0;
	int32 Row = 0;
	for (const TSharedPtr<FUICommandInfo>& ToolCommand : ActiveCategoryDef->ToolCommands)
	{
		Grid->AddSlot(Column, Row)
		[
			BuildToolButton(ToolCommand)
		];

		Column++;
		if (Column >= 2)
		{
			Column = 0;
			Row++;
		}
	}

	return Grid;
}

TSharedRef<SWidget> SQuick_RoadModePanel::BuildToolButton(TSharedPtr<FUICommandInfo> ToolCommand)
{
	const FSlateIcon ToolIcon = ToolCommand->GetIcon();
	const FName CommandName = ToolCommand->GetCommandName();

	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "SlimPaletteToolBarStyle.ToggleButton")
		.HAlign(HAlign_Fill)
		.IsEnabled_Lambda([this, ToolCommand]()
		{
			return CommandList.IsValid() && CommandList->CanExecuteAction(ToolCommand.ToSharedRef());
		})
		.IsChecked_Lambda([this, ToolCommand]()
		{
		 return IsToolActive(ToolCommand) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([this, ToolCommand](ECheckBoxState)
		{
			OnToolClicked(ToolCommand);
		})
		.ToolTipText_Lambda([CommandName]()
		{
			return QuickRoadLocalization::GetToolTooltip(CommandName);
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 4.f, 8.f, 4.f)
			[
				SNew(SImage)
				.Image(ToolIcon.GetIcon())
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 8.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Lambda([CommandName]()
				{
					return QuickRoadLocalization::GetToolLabel(CommandName);
				})
				.Font(FAppStyle::GetFontStyle("NormalText"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
}

FReply SQuick_RoadModePanel::OnCategoryClicked(FName CategoryId)
{
	ActiveCategory = CategoryId;
	RefreshToolGrid();
	ActivateDefaultToolForCategory(CategoryId);
	RefreshToolGrid();
	return FReply::Handled();
}

void SQuick_RoadModePanel::ActivateDefaultToolForCategory(FName CategoryId)
{
	const FQuick_RoadCategoryDef* CategoryDef = Categories.FindByPredicate(
		[CategoryId](const FQuick_RoadCategoryDef& Category)
		{
			return Category.CategoryId == CategoryId;
		});

	if (!CategoryDef || CategoryDef->ToolCommands.Num() == 0 || !CommandList.IsValid())
	{
		return;
	}

	const TSharedPtr<FUICommandInfo>& DefaultTool = CategoryDef->ToolCommands[0];
	if (CommandList->CanExecuteAction(DefaultTool.ToSharedRef()))
	{
		CommandList->ExecuteAction(DefaultTool.ToSharedRef());
	}
}

FReply SQuick_RoadModePanel::OnToolClicked(TSharedPtr<FUICommandInfo> ToolCommand)
{
	if (CommandList.IsValid())
	{
		CommandList->ExecuteAction(ToolCommand.ToSharedRef());
		RefreshToolGrid();
	}
	return FReply::Handled();
}

void SQuick_RoadModePanel::RefreshToolGrid()
{
	if (ToolGridContainer.IsValid())
	{
		ToolGridContainer->SetContent(BuildToolGrid());
	}
}

EVisibility SQuick_RoadModePanel::GetDetailsVisibility() const
{
	return DetailsSectionVisibility;
}

bool SQuick_RoadModePanel::IsCategorySelected(FName CategoryId) const
{
	return ActiveCategory == CategoryId;
}

bool SQuick_RoadModePanel::IsToolActive(TSharedPtr<FUICommandInfo> ToolCommand) const
{
	if (CommandList.IsValid())
	{
		return CommandList->GetCheckState(ToolCommand.ToSharedRef()) == ECheckBoxState::Checked;
	}
	return false;
}
