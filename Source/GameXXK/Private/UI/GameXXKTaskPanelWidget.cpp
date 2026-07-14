#include "UI/GameXXKTaskPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	const FVector2D PanelSize(1040.0f, 660.0f);
	// The reference task panel supports three compact rows. Keep the row itself
	// fixed instead of filling all remaining list height when only one task is
	// available, so the paper frame and action keep their intended proportions.
	constexpr float TaskEntryHeight = 145.0f;
	constexpr float TaskEntryGap = 10.0f;
	const FString TextureRoot(TEXT("/Game/GameXXK/UI/Tasks/Textures/"));
	const FString PanelFrameTexturePath(TextureRoot + TEXT("T_TaskPanelFrame.T_TaskPanelFrame"));
	const FString BackArrowTexturePath(TextureRoot + TEXT("T_TaskPanelBackArrow.T_TaskPanelBackArrow"));
	const FString TitleTexturePath(TextureRoot + TEXT("T_TaskPanelTitle.T_TaskPanelTitle"));
	const FString TabSelectedTexturePath(TextureRoot + TEXT("T_TaskTabSelected.T_TaskTabSelected"));
	const FString TabNormalTexturePath(TextureRoot + TEXT("T_TaskTabNormal.T_TaskTabNormal"));
	const FString TabMainLabelTexturePath(TextureRoot + TEXT("T_TaskTabLabelMain.T_TaskTabLabelMain"));
	const FString TabSideLabelTexturePath(TextureRoot + TEXT("T_TaskTabLabelSide.T_TaskTabLabelSide"));
	const FString EntryFrameTexturePath(TextureRoot + TEXT("T_TaskEntryFrame.T_TaskEntryFrame"));
	const FString AcceptActionTexturePath(TextureRoot + TEXT("T_TaskActionTrack.T_TaskActionTrack"));
	const FString RewardCoinTexturePath(TextureRoot + TEXT("T_RewardCoin.T_RewardCoin"));
	const FString RewardExperienceTexturePath(TextureRoot + TEXT("T_RewardExp.T_RewardExp"));
	const FString RewardTokenTexturePath(TextureRoot + TEXT("T_RewardToken.T_RewardToken"));

	UTexture2D* LoadTexture(const FString& Path)
	{
		return Path.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *Path);
	}

	FSlateBrush MakeTextureBrush(const FString& Path, const FVector2D& ImageSize, const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(LoadTexture(Path));
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = ImageSize;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FSlateBrush MakeTextureBoxBrush(const FString& Path, const FVector2D& ImageSize, const FMargin& Margin, const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush = MakeTextureBrush(Path, ImageSize, Tint);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = Margin;
		return Brush;
	}

	FButtonStyle MakeTextureButtonStyle(const FString& Path, const FVector2D& ImageSize)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeTextureBrush(Path, ImageSize));
		Style.SetHovered(MakeTextureBrush(Path, ImageSize, FLinearColor(1.08f, 1.08f, 1.08f, 1.0f)));
		Style.SetPressed(MakeTextureBrush(Path, ImageSize, FLinearColor(0.82f, 0.82f, 0.82f, 1.0f)));
		Style.SetDisabled(MakeTextureBrush(Path, ImageSize, FLinearColor(0.5f, 0.5f, 0.5f, 0.72f)));
		return Style;
	}

	FButtonStyle MakeTextureBoxButtonStyle(const FString& Path, const FVector2D& ImageSize, const FMargin& Margin)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeTextureBoxBrush(Path, ImageSize, Margin));
		Style.SetHovered(MakeTextureBoxBrush(Path, ImageSize, Margin, FLinearColor(1.08f, 1.08f, 1.08f, 1.0f)));
		Style.SetPressed(MakeTextureBoxBrush(Path, ImageSize, Margin, FLinearColor(0.82f, 0.82f, 0.82f, 1.0f)));
		Style.SetDisabled(MakeTextureBoxBrush(Path, ImageSize, Margin, FLinearColor(0.5f, 0.5f, 0.5f, 0.72f)));
		return Style;
	}

	void AddCanvasChild(UCanvasPanel* Canvas, UWidget* Child, const FVector2D& Position, const FVector2D& Size, const FVector2D& Alignment = FVector2D::ZeroVector, const FAnchors& Anchors = FAnchors(0.0f, 0.0f))
	{
		if (!Canvas || !Child)
		{
			return;
		}
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Child))
		{
			Slot->SetAnchors(Anchors);
			Slot->SetAlignment(Alignment);
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
		}
	}

	UTextBlock* MakeText(UWidgetTree* WidgetTree, const FText& Text, int32 FontSize, const FLinearColor& Color = FLinearColor(0.12f, 0.09f, 0.06f, 1.0f))
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetAutoWrapText(true);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		return TextBlock;
	}

	UImage* MakeImage(UWidgetTree* WidgetTree, const FString& TexturePath, const FVector2D& Size)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Image->SetBrush(MakeTextureBrush(TexturePath, Size));
		return Image;
	}

	void AddReward(UWidgetTree* WidgetTree, UHorizontalBox* Row, const FString& TexturePath, const FString& Value)
	{
		if (!WidgetTree || !Row)
		{
			return;
		}
		if (UImage* Icon = MakeImage(WidgetTree, TexturePath, FVector2D(27.0f, 27.0f)))
		{
			if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Icon))
			{
				Slot->SetPadding(FMargin(0.0f, 0.0f, 5.0f, 0.0f));
				Slot->SetVerticalAlignment(VAlign_Center);
			}
		}
		if (UTextBlock* Text = MakeText(WidgetTree, FText::FromString(Value), 17))
		{
			if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Text))
			{
				Slot->SetPadding(FMargin(0.0f, 0.0f, 22.0f, 0.0f));
				Slot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}
}

TSharedRef<SWidget> UGameXXKTaskPanelWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKTaskPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	CloseTaskPanel();
}

void UGameXXKTaskPanelWidget::RefreshFromState()
{
	BuildProgrammaticLayout();
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Town)
	{
		// Route state-driven closure through the controller so its modal input lock is
		// released together with the visual panel.
		if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
		{
			PlayerController->CloseTaskPanel();
		}
		else
		{
			CloseTaskPanel();
		}
		return;
	}
	if (IsTaskPanelOpenForTest())
	{
		RebuildTaskList();
	}
}

bool UGameXXKTaskPanelWidget::OpenTaskPanel()
{
	bShowingTaskOffers = false;
	// Q/HUD always presents the accepted main-task list first.  Do not retain an
	// old Side selection, because the MVP currently has no accepted side task.
	ActiveCategory = EGameXXKTaskCategory::Main;
	BuildProgrammaticLayout();
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Town)
	{
		return false;
	}
	SetVisibility(ESlateVisibility::Visible);
	RebuildTaskList();
	return true;
}

bool UGameXXKTaskPanelWidget::OpenTaskOfferPanel()
{
	bShowingTaskOffers = true;
	// Every F interaction starts from the NPC's available main-task offer,
	// rather than retaining a category selected during a previous interaction.
	ActiveCategory = EGameXXKTaskCategory::Main;
	BuildProgrammaticLayout();
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Town)
	{
		return false;
	}
	SetVisibility(ESlateVisibility::Visible);
	RebuildTaskList();
	return true;
}

bool UGameXXKTaskPanelWidget::CloseTaskPanel()
{
	SetVisibility(ESlateVisibility::Collapsed);
	return true;
}

bool UGameXXKTaskPanelWidget::IsTaskPanelOpenForTest() const
{
	return GetVisibility() == ESlateVisibility::Visible || GetVisibility() == ESlateVisibility::SelfHitTestInvisible;
}

bool UGameXXKTaskPanelWidget::HasAllTaskFiltersForTest() const
{
	return HasMainAndSideTaskFiltersForTest();
}

bool UGameXXKTaskPanelWidget::HasMainAndSideTaskFiltersForTest() const
{
	return MainTabButton && SideTabButton;
}

bool UGameXXKTaskPanelWidget::IsShowingTaskOffersForTest() const
{
	return bShowingTaskOffers;
}

bool UGameXXKTaskPanelWidget::IsShowingAcceptedTasksForTest() const
{
	return !bShowingTaskOffers;
}

bool UGameXXKTaskPanelWidget::HasPrimaryTaskActionForTest() const
{
	return bHasPrimaryTaskAction;
}

EGameXXKTaskCategory UGameXXKTaskPanelWidget::GetActiveCategoryForTest() const
{
	return ActiveCategory;
}

int32 UGameXXKTaskPanelWidget::GetVisibleTaskCountForTest() const
{
	return VisibleTasks.Num();
}

bool UGameXXKTaskPanelWidget::SelectTaskCategoryForTest(EGameXXKTaskCategory Category)
{
	return SelectTaskCategory(Category);
}

bool UGameXXKTaskPanelWidget::TriggerPrimaryTaskActionForTest()
{
	return TriggerTaskAction(0);
}

bool UGameXXKTaskPanelWidget::HasVisibleTaskOffer(FName TaskId) const
{
	if (!bShowingTaskOffers || TaskId.IsNone())
	{
		return false;
	}

	for (int32 TaskIndex = 0; TaskIndex < VisibleTasks.Num() && TaskIndex < 3; ++TaskIndex)
	{
		const FGameXXKTaskView& Task = VisibleTasks[TaskIndex];
		if (Task.Id == TaskId)
		{
			return true;
		}
	}
	return false;
}

void UGameXXKTaskPanelWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("TaskPanelWidgetTree"));
	}
	if (!WidgetTree || RootCanvas)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TownTaskPanelRoot"));
	WidgetTree->RootWidget = RootCanvas;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TownTaskPanelPaper"));
	PanelBorder->SetPadding(FMargin(34.0f));
	PanelBorder->SetBrush(MakeTextureBoxBrush(PanelFrameTexturePath, PanelSize, FMargin(0.045f)));
	AddCanvasChild(RootCanvas, PanelBorder, FVector2D::ZeroVector, PanelSize, FVector2D(0.5f, 0.5f), FAnchors(0.5f, 0.5f));

	UCanvasPanel* ContentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TownTaskPanelContent"));
	PanelBorder->SetContent(ContentCanvas);

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TownTaskPanelClose"));
	CloseButton->SetStyle(MakeTextureButtonStyle(BackArrowTexturePath, FVector2D(50.0f, 27.0f)));
	CloseButton->OnClicked.AddDynamic(this, &UGameXXKTaskPanelWidget::HandleCloseClicked);
	AddCanvasChild(ContentCanvas, CloseButton, FVector2D(12.0f, 25.0f), FVector2D(50.0f, 27.0f));

	if (UImage* TitleImage = MakeImage(WidgetTree, TitleTexturePath, FVector2D(110.0f, 56.0f)))
	{
		AddCanvasChild(ContentCanvas, TitleImage, FVector2D(72.0f, 10.0f), FVector2D(110.0f, 56.0f));
	}

	auto MakeTabButton = [this, ContentCanvas](const FName Name, const FString& LabelTexturePath, const FVector2D& Position) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		if (UImage* LabelImage = MakeImage(WidgetTree, LabelTexturePath, FVector2D(160.0f, 80.0f)))
		{
			Button->SetContent(LabelImage);
		}
		AddCanvasChild(ContentCanvas, Button, Position, FVector2D(160.0f, 80.0f));
		return Button;
	};

	MainTabButton = MakeTabButton(TEXT("TownTaskTabMain"), TabMainLabelTexturePath, FVector2D(12.0f, 110.0f));
	SideTabButton = MakeTabButton(TEXT("TownTaskTabSide"), TabSideLabelTexturePath, FVector2D(12.0f, 204.0f));
	if (MainTabButton)
	{
		MainTabButton->OnClicked.AddDynamic(this, &UGameXXKTaskPanelWidget::HandleMainTabClicked);
	}
	if (SideTabButton)
	{
		SideTabButton->OnClicked.AddDynamic(this, &UGameXXKTaskPanelWidget::HandleSideTabClicked);
	}

	TaskListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TownTaskList"));
	AddCanvasChild(ContentCanvas, TaskListBox, FVector2D(200.0f, 92.0f), FVector2D(770.0f, 510.0f));
	RefreshTabVisuals();
}

void UGameXXKTaskPanelWidget::RebuildTaskList()
{
	if (!TaskListBox)
	{
		return;
	}

	TaskListBox->ClearChildren();
	VisibleTasks.Reset();
	bHasPrimaryTaskAction = false;
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return;
	}
	VisibleTasks = bShowingTaskOffers
		? UGameXXKMVPRules::BuildAvailableTaskViews(Subsystem->GetRuntimeState(), ActiveCategory)
		: UGameXXKMVPRules::BuildAcceptedTaskViews(Subsystem->GetRuntimeState(), ActiveCategory);
	RefreshTabVisuals();

	if (VisibleTasks.IsEmpty())
	{
		const FText EmptyText = bShowingTaskOffers
			? NSLOCTEXT("GameXXKTaskPanel", "NoAvailableTasks", "当前分类暂无可接任务")
			: NSLOCTEXT("GameXXKTaskPanel", "NoAcceptedTasks", "当前分类暂无已接任务");
		EmptyStateText = MakeText(WidgetTree, EmptyText, 23, FLinearColor(0.28f, 0.24f, 0.19f, 1.0f));
		if (EmptyStateText)
		{
			EmptyStateText->SetJustification(ETextJustify::Center);
			if (UVerticalBoxSlot* EmptySlot = TaskListBox->AddChildToVerticalBox(EmptyStateText))
			{
				EmptySlot->SetPadding(FMargin(0.0f, 170.0f, 0.0f, 0.0f));
				EmptySlot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
		return;
	}

	for (int32 TaskIndex = 0; TaskIndex < VisibleTasks.Num() && TaskIndex < 3; ++TaskIndex)
	{
		const FGameXXKTaskView& Task = VisibleTasks[TaskIndex];
		USizeBox* EntrySizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		EntrySizeBox->SetWidthOverride(770.0f);
		EntrySizeBox->SetHeightOverride(TaskEntryHeight);
		UBorder* EntryBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		EntryBorder->SetPadding(FMargin(28.0f, 12.0f));
		EntryBorder->SetBrush(MakeTextureBoxBrush(EntryFrameTexturePath, FVector2D(770.0f, TaskEntryHeight), FMargin(0.055f, 0.18f)));
		EntrySizeBox->SetContent(EntryBorder);
		if (UVerticalBoxSlot* EntrySlot = TaskListBox->AddChildToVerticalBox(EntrySizeBox))
		{
			EntrySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, TaskEntryGap));
			EntrySlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		UVerticalBox* EntryBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		EntryBorder->SetContent(EntryBox);
		if (UTextBlock* Title = MakeText(WidgetTree, Task.Title, 25))
		{
			FSlateFontInfo Font = Title->GetFont();
			Font.TypefaceFontName = TEXT("Bold");
			Title->SetFont(Font);
			EntryBox->AddChildToVerticalBox(Title);
		}
		if (UTextBlock* Description = MakeText(WidgetTree, Task.Description, 18, FLinearColor(0.28f, 0.24f, 0.19f, 1.0f)))
		{
			if (UVerticalBoxSlot* DescriptionSlot = EntryBox->AddChildToVerticalBox(Description))
			{
				DescriptionSlot->SetPadding(FMargin(0.0f, 2.0f));
			}
		}
		USpacer* ContentSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		ContentSpacer->SetSize(FVector2D(1.0f, 1.0f));
		if (UVerticalBoxSlot* SpacerSlot = EntryBox->AddChildToVerticalBox(ContentSpacer))
		{
			SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		UHorizontalBox* BottomRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		EntryBox->AddChildToVerticalBox(BottomRow);
		UHorizontalBox* Rewards = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* RewardSlot = BottomRow->AddChildToHorizontalBox(Rewards))
		{
			RewardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			RewardSlot->SetVerticalAlignment(VAlign_Center);
		}
		AddReward(WidgetTree, Rewards, RewardCoinTexturePath, FString::FromInt(Task.Reward.Gold));
		AddReward(WidgetTree, Rewards, RewardExperienceTexturePath, FString::FromInt(Task.Reward.Experience));
		AddReward(WidgetTree, Rewards, RewardTokenTexturePath, FString::FromInt(Task.Reward.Token));

		if (UTextBlock* Progress = MakeText(WidgetTree, FText::FromString(FString::Printf(TEXT("%d/%d"), Task.ProgressCurrent, Task.ProgressTarget)), 18))
		{
			if (UHorizontalBoxSlot* ProgressSlot = BottomRow->AddChildToHorizontalBox(Progress))
			{
				ProgressSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
				ProgressSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		if (bShowingTaskOffers)
		{
			UButton* AcceptButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			AcceptButton->SetStyle(MakeTextureBoxButtonStyle(AcceptActionTexturePath, FVector2D(120.0f, 56.0f), FMargin(0.09f, 0.18f)));
			if (UTextBlock* AcceptText = MakeText(WidgetTree, NSLOCTEXT("GameXXKTaskPanel", "AcceptTask", "接取"), 20, FLinearColor(0.13f, 0.09f, 0.05f, 1.0f)))
			{
				AcceptText->SetJustification(ETextJustify::Center);
				AcceptButton->SetContent(AcceptText);
			}
			if (UHorizontalBoxSlot* ActionSlot = BottomRow->AddChildToHorizontalBox(AcceptButton))
			{
				ActionSlot->SetVerticalAlignment(VAlign_Center);
			}
			switch (TaskIndex)
			{
			case 0:
				AcceptButton->OnClicked.AddDynamic(this, &UGameXXKTaskPanelWidget::HandleTaskAction0Clicked);
				break;
			case 1:
				AcceptButton->OnClicked.AddDynamic(this, &UGameXXKTaskPanelWidget::HandleTaskAction1Clicked);
				break;
			case 2:
				AcceptButton->OnClicked.AddDynamic(this, &UGameXXKTaskPanelWidget::HandleTaskAction2Clicked);
				break;
			default:
				break;
			}
			bHasPrimaryTaskAction = true;
		}
	}
}

bool UGameXXKTaskPanelWidget::SelectTaskCategory(EGameXXKTaskCategory Category)
{
	if (Category != EGameXXKTaskCategory::Main && Category != EGameXXKTaskCategory::Side)
	{
		return false;
	}
	ActiveCategory = Category;
	RebuildTaskList();
	return true;
}

bool UGameXXKTaskPanelWidget::TriggerTaskAction(int32 TaskIndex)
{
	if (!bShowingTaskOffers || !VisibleTasks.IsValidIndex(TaskIndex))
	{
		return false;
	}
	const FName TaskId = VisibleTasks[TaskIndex].Id;
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		return PlayerController->AcceptTaskOfferById(TaskId);
	}
	return false;
}

void UGameXXKTaskPanelWidget::RefreshTabVisuals()
{
	if (MainTabButton)
	{
		const bool bSelected = ActiveCategory == EGameXXKTaskCategory::Main;
		MainTabButton->SetStyle(MakeTextureButtonStyle(
			bSelected ? TabSelectedTexturePath : TabNormalTexturePath,
			FVector2D(160.0f, 80.0f)));
		MainTabButton->SetRenderOpacity(bSelected ? 1.0f : 0.85f);
	}
	if (SideTabButton)
	{
		const bool bSelected = ActiveCategory == EGameXXKTaskCategory::Side;
		SideTabButton->SetStyle(MakeTextureButtonStyle(
			bSelected ? TabSelectedTexturePath : TabNormalTexturePath,
			FVector2D(160.0f, 80.0f)));
		SideTabButton->SetRenderOpacity(bSelected ? 1.0f : 0.85f);
	}
}

void UGameXXKTaskPanelWidget::HandleCloseClicked()
{
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		PlayerController->CloseTaskPanel();
		return;
	}
	CloseTaskPanel();
}

void UGameXXKTaskPanelWidget::HandleMainTabClicked()
{
	SelectTaskCategory(EGameXXKTaskCategory::Main);
}

void UGameXXKTaskPanelWidget::HandleSideTabClicked()
{
	SelectTaskCategory(EGameXXKTaskCategory::Side);
}

void UGameXXKTaskPanelWidget::HandleTaskAction0Clicked()
{
	TriggerTaskAction(0);
}

void UGameXXKTaskPanelWidget::HandleTaskAction1Clicked()
{
	TriggerTaskAction(1);
}

void UGameXXKTaskPanelWidget::HandleTaskAction2Clicked()
{
	TriggerTaskAction(2);
}
