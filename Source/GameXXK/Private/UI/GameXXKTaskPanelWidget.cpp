#include "UI/GameXXKTaskPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	const FVector2D PanelSize(1040.0f, 660.0f);
	const FString TextureRoot(TEXT("/Game/GameXXK/UI/Town/Textures/Task/"));
	const FString BackArrowTexturePath(TextureRoot + TEXT("T_TownTask_BackArrow.T_TownTask_BackArrow"));
	const FString TitleTexturePath(TextureRoot + TEXT("T_TownTask_Title.T_TownTask_Title"));
	const FString TabMainTexturePath(TextureRoot + TEXT("T_TownTask_TabMain.T_TownTask_TabMain"));
	const FString TabSideTexturePath(TextureRoot + TEXT("T_TownTask_TabSide.T_TownTask_TabSide"));
	const FString TabDailyTexturePath(TextureRoot + TEXT("T_TownTask_TabDaily.T_TownTask_TabDaily"));
	const FString TabAdventureTexturePath(TextureRoot + TEXT("T_TownTask_TabAdventure.T_TownTask_TabAdventure"));
	const FString ActionTrackTexturePath(TextureRoot + TEXT("T_TownTask_ActionTrack.T_TownTask_ActionTrack"));
	const FString ActionGoTexturePath(TextureRoot + TEXT("T_TownTask_ActionGo.T_TownTask_ActionGo"));
	const FString RewardCoinTexturePath(TextureRoot + TEXT("T_TownTask_RewardCoin.T_TownTask_RewardCoin"));
	const FString RewardExperienceTexturePath(TextureRoot + TEXT("T_TownTask_RewardExp.T_TownTask_RewardExp"));
	const FString RewardTokenTexturePath(TextureRoot + TEXT("T_TownTask_RewardToken.T_TownTask_RewardToken"));

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

	FButtonStyle MakeTextureButtonStyle(const FString& Path, const FVector2D& ImageSize)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeTextureBrush(Path, ImageSize));
		Style.SetHovered(MakeTextureBrush(Path, ImageSize, FLinearColor(1.08f, 1.08f, 1.08f, 1.0f)));
		Style.SetPressed(MakeTextureBrush(Path, ImageSize, FLinearColor(0.82f, 0.82f, 0.82f, 1.0f)));
		Style.SetDisabled(MakeTextureBrush(Path, ImageSize, FLinearColor(0.5f, 0.5f, 0.5f, 0.72f)));
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
		CloseTaskPanel();
		return;
	}
	if (IsTaskPanelOpenForTest())
	{
		RebuildTaskList();
	}
}

bool UGameXXKTaskPanelWidget::OpenTaskPanel()
{
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
	return MainTabButton && SideTabButton && DailyTabButton && AdventureTabButton;
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
	PanelBorder->SetPadding(FMargin(28.0f));
	PanelBorder->SetBrushColor(FLinearColor(0.955f, 0.915f, 0.83f, 0.985f));
	AddCanvasChild(RootCanvas, PanelBorder, FVector2D::ZeroVector, PanelSize, FVector2D(0.5f, 0.5f), FAnchors(0.5f, 0.5f));

	UCanvasPanel* ContentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TownTaskPanelContent"));
	PanelBorder->SetContent(ContentCanvas);

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TownTaskPanelClose"));
	CloseButton->SetStyle(MakeTextureButtonStyle(BackArrowTexturePath, FVector2D(42.0f, 38.0f)));
	CloseButton->OnClicked.AddDynamic(this, &UGameXXKTaskPanelWidget::HandleCloseClicked);
	AddCanvasChild(ContentCanvas, CloseButton, FVector2D(12.0f, 12.0f), FVector2D(42.0f, 38.0f));

	if (UImage* TitleImage = MakeImage(WidgetTree, TitleTexturePath, FVector2D(145.0f, 50.0f)))
	{
		AddCanvasChild(ContentCanvas, TitleImage, FVector2D(70.0f, 8.0f), FVector2D(145.0f, 50.0f));
	}

	auto MakeTabButton = [this, ContentCanvas](const FName Name, const FString& TexturePath, const FVector2D& Position) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetStyle(MakeTextureButtonStyle(TexturePath, FVector2D(150.0f, 62.0f)));
		AddCanvasChild(ContentCanvas, Button, Position, FVector2D(150.0f, 62.0f));
		return Button;
	};

	MainTabButton = MakeTabButton(TEXT("TownTaskTabMain"), TabMainTexturePath, FVector2D(14.0f, 95.0f));
	SideTabButton = MakeTabButton(TEXT("TownTaskTabSide"), TabSideTexturePath, FVector2D(14.0f, 177.0f));
	DailyTabButton = MakeTabButton(TEXT("TownTaskTabDaily"), TabDailyTexturePath, FVector2D(14.0f, 259.0f));
	AdventureTabButton = MakeTabButton(TEXT("TownTaskTabAdventure"), TabAdventureTexturePath, FVector2D(14.0f, 341.0f));
	if (MainTabButton)
	{
		MainTabButton->OnClicked.AddDynamic(this, &UGameXXKTaskPanelWidget::HandleMainTabClicked);
	}
	if (SideTabButton)
	{
		SideTabButton->OnClicked.AddDynamic(this, &UGameXXKTaskPanelWidget::HandleSideTabClicked);
	}
	if (DailyTabButton)
	{
		DailyTabButton->OnClicked.AddDynamic(this, &UGameXXKTaskPanelWidget::HandleDailyTabClicked);
	}
	if (AdventureTabButton)
	{
		AdventureTabButton->OnClicked.AddDynamic(this, &UGameXXKTaskPanelWidget::HandleAdventureTabClicked);
	}

	TaskListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TownTaskList"));
	AddCanvasChild(ContentCanvas, TaskListBox, FVector2D(190.0f, 82.0f), FVector2D(790.0f, 520.0f));
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
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return;
	}
	VisibleTasks = UGameXXKMVPRules::BuildTaskViews(Subsystem->GetRuntimeState(), ActiveCategory);
	RefreshTabVisuals();

	if (VisibleTasks.IsEmpty())
	{
		EmptyStateText = MakeText(WidgetTree, NSLOCTEXT("GameXXKTaskPanel", "NoTasks", "当前分类暂无任务"), 23, FLinearColor(0.28f, 0.24f, 0.19f, 1.0f));
		if (EmptyStateText)
		{
			EmptyStateText->SetJustification(ETextJustify::Center);
			if (UVerticalBoxSlot* Slot = TaskListBox->AddChildToVerticalBox(EmptyStateText))
			{
				Slot->SetPadding(FMargin(0.0f, 170.0f, 0.0f, 0.0f));
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
		return;
	}

	for (int32 TaskIndex = 0; TaskIndex < VisibleTasks.Num() && TaskIndex < 3; ++TaskIndex)
	{
		const FGameXXKTaskView& Task = VisibleTasks[TaskIndex];
		UBorder* EntryBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		EntryBorder->SetPadding(FMargin(22.0f, 15.0f));
		EntryBorder->SetBrushColor(FLinearColor(0.87f, 0.82f, 0.71f, 0.92f));
		if (UVerticalBoxSlot* Slot = TaskListBox->AddChildToVerticalBox(EntryBorder))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 15.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
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
			if (UVerticalBoxSlot* Slot = EntryBox->AddChildToVerticalBox(Description))
			{
				Slot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 4.0f));
			}
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

		const bool bTracked = Subsystem->GetRuntimeState().TrackedTaskId == Task.Id;
		const FString& ActionPath = Task.bCanNavigate && !bTracked ? ActionGoTexturePath : ActionTrackTexturePath;
		UButton* ActionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		ActionButton->SetStyle(MakeTextureButtonStyle(ActionPath, FVector2D(105.0f, 52.0f)));
		if (UHorizontalBoxSlot* ActionSlot = BottomRow->AddChildToHorizontalBox(ActionButton))
		{
			ActionSlot->SetVerticalAlignment(VAlign_Center);
		}
		switch (TaskIndex)
		{
		case 0:
			ActionButton->OnClicked.AddDynamic(this, &UGameXXKTaskPanelWidget::HandleTaskAction0Clicked);
			break;
		case 1:
			ActionButton->OnClicked.AddDynamic(this, &UGameXXKTaskPanelWidget::HandleTaskAction1Clicked);
			break;
		case 2:
			ActionButton->OnClicked.AddDynamic(this, &UGameXXKTaskPanelWidget::HandleTaskAction2Clicked);
			break;
		default:
			break;
		}
	}
}

bool UGameXXKTaskPanelWidget::SelectTaskCategory(EGameXXKTaskCategory Category)
{
	switch (Category)
	{
	case EGameXXKTaskCategory::Main:
	case EGameXXKTaskCategory::Side:
	case EGameXXKTaskCategory::Daily:
	case EGameXXKTaskCategory::Adventure:
		break;
	default:
		return false;
	}
	ActiveCategory = Category;
	RebuildTaskList();
	return true;
}

bool UGameXXKTaskPanelWidget::TriggerTaskAction(int32 TaskIndex)
{
	if (!VisibleTasks.IsValidIndex(TaskIndex))
	{
		return false;
	}
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !UGameXXKMVPRules::ToggleTrackedTask(Subsystem->GetMutableRuntimeState(), VisibleTasks[TaskIndex].Id))
	{
		return false;
	}
	if (!NotifyPlayerFlowStateChanged())
	{
		RefreshFromState();
	}
	return true;
}

void UGameXXKTaskPanelWidget::RefreshTabVisuals()
{
	if (MainTabButton)
	{
		MainTabButton->SetRenderOpacity(ActiveCategory == EGameXXKTaskCategory::Main ? 1.0f : 0.65f);
	}
	if (SideTabButton)
	{
		SideTabButton->SetRenderOpacity(ActiveCategory == EGameXXKTaskCategory::Side ? 1.0f : 0.65f);
	}
	if (DailyTabButton)
	{
		DailyTabButton->SetRenderOpacity(ActiveCategory == EGameXXKTaskCategory::Daily ? 1.0f : 0.65f);
	}
	if (AdventureTabButton)
	{
		AdventureTabButton->SetRenderOpacity(ActiveCategory == EGameXXKTaskCategory::Adventure ? 1.0f : 0.65f);
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

void UGameXXKTaskPanelWidget::HandleDailyTabClicked()
{
	SelectTaskCategory(EGameXXKTaskCategory::Daily);
}

void UGameXXKTaskPanelWidget::HandleAdventureTabClicked()
{
	SelectTaskCategory(EGameXXKTaskCategory::Adventure);
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
