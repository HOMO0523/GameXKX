#include "UI/GameXXKStoryTaskDrawerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/AppStyle.h"

namespace GameXXKStoryTaskDrawerPrivate
{
	const FVector2D PanelSize(363.0f, 908.0f);
	const FVector4 CloseRect(315.0f, 31.0f, 44.0f, 44.0f);
	constexpr const TCHAR* PanelTallTexturePath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelTall.T_MasterV2_PanelTall");
	constexpr const TCHAR* CloseInkTexturePath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CloseInk.T_MasterV2_CloseInk");
	constexpr const TCHAR* TabNormalTexturePath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/003_tab_1.003_tab_1");
	constexpr const TCHAR* TabSelectedTexturePath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/004_tab_2.004_tab_2");
	const FLinearColor Ink(0.10f, 0.07f, 0.04f, 1.0f);
	const FLinearColor MutedInk(0.30f, 0.23f, 0.16f, 1.0f);
	const FLinearColor RedDotColor(0.82f, 0.08f, 0.06f, 1.0f);

	FSlateBrush MakeTextureBrush(UTexture2D* Texture, const FVector2D& Size, const ESlateBrushDrawType::Type DrawAs = ESlateBrushDrawType::Image)
	{
		FSlateBrush Brush = Texture ? FSlateBrush() : *FAppStyle::Get().GetBrush("WhiteBrush");
		Brush.SetResourceObject(Texture ? Texture : Brush.GetResourceObject());
		Brush.DrawAs = DrawAs;
		Brush.ImageSize = Size;
		if (!Texture)
		{
			Brush.TintColor = FSlateColor(FLinearColor(0.18f, 0.12f, 0.07f, 1.0f));
		}
		if (DrawAs == ESlateBrushDrawType::Box)
		{
			Brush.Margin = FMargin(0.08f);
		}
		return Brush;
	}

	FButtonStyle MakeButtonStyle(UTexture2D* Texture, const FVector2D& Size)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeTextureBrush(Texture, Size, ESlateBrushDrawType::Box));
		Style.SetHovered(MakeTextureBrush(Texture, Size, ESlateBrushDrawType::Box));
		Style.SetPressed(MakeTextureBrush(Texture, Size, ESlateBrushDrawType::Box));
		Style.SetDisabled(MakeTextureBrush(Texture, Size, ESlateBrushDrawType::Box));
		return Style;
	}

	void AddCanvas(UCanvasPanel* Canvas, UWidget* Child, const FVector2D& Position, const FVector2D& Size)
	{
		if (Canvas && Child)
		{
			if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Child))
			{
				Slot->SetPosition(Position);
				Slot->SetSize(Size);
			}
		}
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const FName Name, const FText& Text, const int32 FontSize, const FLinearColor& Color = Ink)
	{
		UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Result->SetText(Text);
		Result->SetColorAndOpacity(FSlateColor(Color));
		Result->SetAutoWrapText(true);
		FSlateFontInfo Font = Result->GetFont();
		Font.Size = FontSize;
		Result->SetFont(Font);
		return Result;
	}

	bool ContainsTask(const TArray<FGameXXKStoryTaskDrawerEntryView>& Entries, const FName TaskId)
	{
		return Entries.ContainsByPredicate([TaskId](const FGameXXKStoryTaskDrawerEntryView& Entry)
		{
			return Entry.TaskId == TaskId;
		});
	}

	FText StateLabel(const EGameXXKTaskState State)
	{
		switch (State)
		{
		case EGameXXKTaskState::Available: return FText::FromString(TEXT("可接"));
		case EGameXXKTaskState::Active: return FText::FromString(TEXT("进行中"));
		case EGameXXKTaskState::Completed: return FText::FromString(TEXT("待领取"));
		case EGameXXKTaskState::Rewarded: return FText::FromString(TEXT("已领取"));
		case EGameXXKTaskState::Locked:
		default: return FText::FromString(TEXT("未解锁"));
		}
	}

	FText RewardLabel(const FGameXXKNarrativeTaskRewardDefinition& Reward)
	{
		TArray<FString> Values;
		if (Reward.Gold > 0)
		{
			Values.Add(FString::Printf(TEXT("金币 %d"), Reward.Gold));
		}
		if (Reward.Experience > 0)
		{
			Values.Add(FString::Printf(TEXT("经验 %d"), Reward.Experience));
		}
		for (const TPair<FName, int32>& Item : Reward.Items)
		{
			Values.Add(FString::Printf(TEXT("%s ×%d"), *Item.Key.ToString(), Item.Value));
		}
		return FText::FromString(Values.IsEmpty() ? TEXT("奖励：无") : TEXT("奖励：") + FString::Join(Values, TEXT("  ")));
	}

	int32 CountNestedButtons(const UWidget* Widget)
	{
		int32 Count = Cast<UButton>(Widget) ? 1 : 0;
		const UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
		if (Panel)
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				Count += CountNestedButtons(Panel->GetChildAt(Index));
			}
		}
		return Count;
	}
}

void UGameXXKStoryTaskDrawerRowButton::Configure(UGameXXKStoryTaskDrawerWidget* InOwner, const int32 InRowIndex)
{
	Owner = InOwner;
	RowIndex = InRowIndex;
	OnClicked.AddDynamic(this, &UGameXXKStoryTaskDrawerRowButton::HandleClicked);
}

void UGameXXKStoryTaskDrawerRowButton::HandleClicked()
{
	if (Owner)
	{
		Owner->SelectVisibleRow(RowIndex);
	}
}

TSharedRef<SWidget> UGameXXKStoryTaskDrawerWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKStoryTaskDrawerWidget::ApplySnapshot(
	const FGameXXKStoryTaskDrawerSnapshot& InSnapshot,
	const FGameXXKStoryTaskDrawerUiState& InUiState)
{
	Snapshot = InSnapshot;
	UiState = InUiState;
	NormalizeUiState();
	BuildProgrammaticLayout();
	RebuildRowsAndDetail();
}

void UGameXXKStoryTaskDrawerWidget::SetCloseRequested(FGameXXKStoryTaskDrawerCloseDelegate InDelegate)
{
	CloseRequestedDelegate = MoveTemp(InDelegate);
}

void UGameXXKStoryTaskDrawerWidget::SetPrimaryActionRequested(FGameXXKStoryTaskDrawerPrimaryActionDelegate InDelegate)
{
	PrimaryActionRequestedDelegate = MoveTemp(InDelegate);
}

bool UGameXXKStoryTaskDrawerWidget::OpenDrawer()
{
	BuildProgrammaticLayout();
	bDrawerOpen = true;
	SetVisibility(ESlateVisibility::Visible);
	return true;
}

bool UGameXXKStoryTaskDrawerWidget::CloseDrawer()
{
	if (!bDrawerOpen)
	{
		return false;
	}
	bDrawerOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
	CloseRequestedDelegate.ExecuteIfBound();
	return true;
}

FVector2D UGameXXKStoryTaskDrawerWidget::GetPanelFootprintForTest() const
{
	const UCanvasPanelSlot* CanvasSlot = PanelBorder ? Cast<UCanvasPanelSlot>(PanelBorder->Slot) : nullptr;
	return CanvasSlot ? CanvasSlot->GetSize() : FVector2D::ZeroVector;
}

FString UGameXXKStoryTaskDrawerWidget::GetPanelResourcePathForTest() const
{
	return PanelBorder && PanelBorder->Background.GetResourceObject()
		? PanelBorder->Background.GetResourceObject()->GetPathName()
		: FString();
}

FVector4 UGameXXKStoryTaskDrawerWidget::GetCloseRectForTest() const
{
	const UCanvasPanelSlot* CanvasSlot = CloseButton ? Cast<UCanvasPanelSlot>(CloseButton->Slot) : nullptr;
	return CanvasSlot ? FVector4(CanvasSlot->GetPosition().X, CanvasSlot->GetPosition().Y, CanvasSlot->GetSize().X, CanvasSlot->GetSize().Y) : FVector4();
}

EGameXXKStoryTaskDrawerTab UGameXXKStoryTaskDrawerWidget::GetActiveTabForTest() const
{
	return UiState.ActiveTab;
}

FName UGameXXKStoryTaskDrawerWidget::GetSelectedTaskIdForTest() const
{
	return UiState.ActiveTab == EGameXXKStoryTaskDrawerTab::Claimable
		? UiState.SelectedClaimableTaskId
		: UiState.SelectedActionableTaskId;
}

FText UGameXXKStoryTaskDrawerWidget::GetPrimaryActionLabelForTest() const
{
	return PrimaryActionLabelText ? PrimaryActionLabelText->GetText() : FText::GetEmpty();
}

bool UGameXXKStoryTaskDrawerWidget::IsPrimaryActionEnabledForTest() const
{
	return PrimaryActionButton && PrimaryActionButton->GetIsEnabled();
}

bool UGameXXKStoryTaskDrawerWidget::IsClaimableRedDotVisibleForTest() const
{
	return ClaimableRedDot
		? ClaimableRedDot->GetVisibility() != ESlateVisibility::Collapsed
		: Snapshot.bHasClaimableRedDot;
}

int32 UGameXXKStoryTaskDrawerWidget::GetRowCountForTest() const
{
	return StoryTaskRows ? StoryTaskRows->GetChildrenCount() : 0;
}

int32 UGameXXKStoryTaskDrawerWidget::CountRowActionButtonsForTest() const
{
	int32 Count = 0;
	for (const UGameXXKStoryTaskDrawerRowButton* Row : RowButtons)
	{
		Count += Row ? GameXXKStoryTaskDrawerPrivate::CountNestedButtons(Row->GetContent()) : 0;
	}
	return Count;
}

FText UGameXXKStoryTaskDrawerWidget::GetRowSummaryForTest(const int32 RowIndex) const
{
	return RowSummaries.IsValidIndex(RowIndex) && RowSummaries[RowIndex] ? RowSummaries[RowIndex]->GetText() : FText::GetEmpty();
}

FGameXXKStoryTaskDrawerUiState UGameXXKStoryTaskDrawerWidget::GetUiStateForTest() const
{
	return UiState;
}

UWidget* UGameXXKStoryTaskDrawerWidget::FindNamedControlForTest(const FName Name) const
{
	return WidgetTree ? WidgetTree->FindWidget(Name) : nullptr;
}

bool UGameXXKStoryTaskDrawerWidget::SimulateSelectTabForTest(const EGameXXKStoryTaskDrawerTab Tab)
{
	return SelectTab(Tab);
}

bool UGameXXKStoryTaskDrawerWidget::SimulateSelectRowForTest(const int32 RowIndex)
{
	if (!GetVisibleEntries().IsValidIndex(RowIndex))
	{
		return false;
	}
	SelectVisibleRow(RowIndex);
	return true;
}

bool UGameXXKStoryTaskDrawerWidget::SimulateCloseForTest()
{
	HandleCloseClicked();
	return true;
}

bool UGameXXKStoryTaskDrawerWidget::SimulatePrimaryActionForTest()
{
	if (!GetSelectedEntry())
	{
		return false;
	}
	EmitPrimaryAction();
	return true;
}

void UGameXXKStoryTaskDrawerWidget::SelectVisibleRow(const int32 RowIndex)
{
	if (!VisibleRowTaskIds.IsValidIndex(RowIndex))
	{
		return;
	}
	GetSelectedTaskIdRef() = VisibleRowTaskIds[RowIndex];
	RefreshRowSelectionVisuals();
	RefreshDetail();
}

void UGameXXKStoryTaskDrawerWidget::ResolveCachedResources()
{
	using namespace GameXXKStoryTaskDrawerPrivate;
	if (PanelTexture)
	{
		return;
	}
	const auto Resolve = [](const TCHAR* Path, const TCHAR* Label) -> UTexture2D*
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path);
		if (!Texture)
		{
			UE_LOG(LogTemp, Warning, TEXT("StoryTaskDrawer missing %s resource: %s"), Label, Path);
		}
		return Texture;
	};
	PanelTexture = Resolve(PanelTallTexturePath, TEXT("panel"));
	CloseTexture = Resolve(CloseInkTexturePath, TEXT("close"));
	NormalTabTexture = Resolve(TabNormalTexturePath, TEXT("normal tab"));
	SelectedTabTexture = Resolve(TabSelectedTexturePath, TEXT("selected tab"));
	PanelBrush = MakeTextureBrush(PanelTexture, PanelSize, ESlateBrushDrawType::Box);
	CloseButtonStyle = MakeButtonStyle(CloseTexture, FVector2D(CloseRect.Z, CloseRect.W));
	TabNormalButtonStyle = MakeButtonStyle(NormalTabTexture, FVector2D(142.0f, 42.0f));
	TabSelectedButtonStyle = MakeButtonStyle(SelectedTabTexture, FVector2D(142.0f, 42.0f));
	RowNormalButtonStyle = MakeButtonStyle(NormalTabTexture, FVector2D(303.0f, 70.0f));
	RowSelectedButtonStyle = MakeButtonStyle(SelectedTabTexture, FVector2D(303.0f, 70.0f));
	PrimaryButtonStyle = MakeButtonStyle(SelectedTabTexture, FVector2D(305.0f, 52.0f));
}

void UGameXXKStoryTaskDrawerWidget::BuildProgrammaticLayout()
{
	using namespace GameXXKStoryTaskDrawerPrivate;
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("StoryTaskDrawerWidgetTree"));
	}
	if (!WidgetTree || RootCanvas)
	{
		return;
	}
	ResolveCachedResources();

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StoryTaskDrawerRoot"));
	WidgetTree->RootWidget = RootCanvas;
	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StoryTaskDrawerPanel"));
	PanelBorder->SetPadding(FMargin(0.0f));
	PanelBorder->SetBrush(PanelBrush);
	PanelBorder->SetBrushColor(FLinearColor::White);
	AddCanvas(RootCanvas, PanelBorder, FVector2D::ZeroVector, PanelSize);

	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StoryTaskDrawerContent"));
	PanelBorder->SetContent(Content);
	UTextBlock* Title = MakeText(WidgetTree, TEXT("StoryTaskDrawerTitle"), FText::FromString(TEXT("剧情任务")), 24);
	AddCanvas(Content, Title, FVector2D(30.0f, 34.0f), FVector2D(250.0f, 36.0f));

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StoryTaskDrawerClose"));
	CloseButton->SetStyle(CloseButtonStyle);
	CloseButton->SetBackgroundColor(FLinearColor::White);
	UTextBlock* CloseFallbackLabel = MakeText(WidgetTree, TEXT("StoryTaskDrawerCloseFallbackLabel"), FText::FromString(TEXT("×")), 22, Ink);
	CloseFallbackLabel->SetJustification(ETextJustify::Center);
	CloseButton->SetContent(CloseFallbackLabel);
	CloseButton->OnClicked.AddDynamic(this, &UGameXXKStoryTaskDrawerWidget::HandleCloseClicked);
	AddCanvas(Content, CloseButton, FVector2D(CloseRect.X, CloseRect.Y), FVector2D(CloseRect.Z, CloseRect.W));

	const auto MakeTab = [this, Content](const FName Name, const FText& Label, const FVector2D Position)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetContent(GameXXKStoryTaskDrawerPrivate::MakeText(WidgetTree, *FString::Printf(TEXT("%sLabel"), *Name.ToString()), Label, 17));
		AddCanvas(Content, Button, Position, FVector2D(142.0f, 42.0f));
		return Button;
	};
	ActionableTabButton = MakeTab(TEXT("StoryTaskDrawerActionableTab"), FText::FromString(TEXT("可进行")), FVector2D(28.0f, 84.0f));
	ClaimableTabButton = MakeTab(TEXT("StoryTaskDrawerClaimableTab"), FText::FromString(TEXT("待领取")), FVector2D(185.0f, 84.0f));
	ActionableTabButton->OnClicked.AddDynamic(this, &UGameXXKStoryTaskDrawerWidget::HandleActionableTabClicked);
	ClaimableTabButton->OnClicked.AddDynamic(this, &UGameXXKStoryTaskDrawerWidget::HandleClaimableTabClicked);

	ClaimableRedDot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StoryTaskDrawerClaimableRedDot"));
	ClaimableRedDot->SetBrush(MakeTextureBrush(SelectedTabTexture, FVector2D(12.0f, 12.0f)));
	ClaimableRedDot->SetColorAndOpacity(RedDotColor);
	AddCanvas(Content, ClaimableRedDot, FVector2D(306.0f, 87.0f), FVector2D(12.0f, 12.0f));

	StoryTaskList = WidgetTree->ConstructWidget<UGameXXKStoryTaskDrawerScrollBox>(UGameXXKStoryTaskDrawerScrollBox::StaticClass(), TEXT("StoryTaskDrawerList"));
	StoryTaskList->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	StoryTaskList->OnUserScrolled.AddDynamic(this, &UGameXXKStoryTaskDrawerWidget::HandleListScrolled);
	AddCanvas(Content, StoryTaskList, FVector2D(27.0f, 139.0f), FVector2D(309.0f, 405.0f));

	DetailTitleText = MakeText(WidgetTree, TEXT("StoryTaskDrawerDetailTitle"), FText::GetEmpty(), 19);
	DetailDescriptionText = MakeText(WidgetTree, TEXT("StoryTaskDrawerDetailDescription"), FText::GetEmpty(), 14, MutedInk);
	DetailStateText = MakeText(WidgetTree, TEXT("StoryTaskDrawerDetailState"), FText::GetEmpty(), 14);
	DetailRewardText = MakeText(WidgetTree, TEXT("StoryTaskDrawerDetailReward"), FText::GetEmpty(), 14);
	AddCanvas(Content, DetailTitleText, FVector2D(28.0f, 570.0f), FVector2D(305.0f, 30.0f));
	AddCanvas(Content, DetailDescriptionText, FVector2D(28.0f, 604.0f), FVector2D(305.0f, 82.0f));
	AddCanvas(Content, DetailStateText, FVector2D(28.0f, 691.0f), FVector2D(305.0f, 22.0f));
	AddCanvas(Content, DetailRewardText, FVector2D(28.0f, 718.0f), FVector2D(305.0f, 46.0f));
	PrimaryActionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StoryTaskDrawerPrimaryAction"));
	PrimaryActionButton->SetStyle(PrimaryButtonStyle);
	PrimaryActionButton->OnClicked.AddDynamic(this, &UGameXXKStoryTaskDrawerWidget::HandlePrimaryActionClicked);
	PrimaryActionLabelText = MakeText(WidgetTree, TEXT("StoryTaskDrawerPrimaryActionLabel"), FText::GetEmpty(), 18);
	PrimaryActionLabelText->SetJustification(ETextJustify::Center);
	PrimaryActionButton->SetContent(PrimaryActionLabelText);
	AddCanvas(Content, PrimaryActionButton, FVector2D(28.0f, 826.0f), FVector2D(305.0f, 52.0f));
	bDrawerOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
	RebuildRowsAndDetail();
}

void UGameXXKStoryTaskDrawerWidget::RebuildRowsAndDetail()
{
	if (!StoryTaskList || !WidgetTree)
	{
		return;
	}
	StoryTaskList->ClearChildren();
	VisibleRowTaskIds.Reset();
	RowButtons.Reset();
	RowMarkers.Reset();
	RowTitles.Reset();
	RowSummaries.Reset();
	StoryTaskRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StoryTaskDrawerRows"));
	StoryTaskList->AddChild(StoryTaskRows);
	const TArray<FGameXXKStoryTaskDrawerEntryView>& Entries = GetVisibleEntries();
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FGameXXKStoryTaskDrawerEntryView& Entry = Entries[Index];
		UGameXXKStoryTaskDrawerRowButton* Row = WidgetTree->ConstructWidget<UGameXXKStoryTaskDrawerRowButton>(
			UGameXXKStoryTaskDrawerRowButton::StaticClass(), *FString::Printf(TEXT("StoryTaskDrawerRow_%d"), Index));
		Row->Configure(this, Index);
		UVerticalBox* RowContents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Row->SetContent(RowContents);
		UHorizontalBox* FirstLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("StoryTaskDrawerRowFirstLine_%d"), Index));
		UTextBlock* Marker = GameXXKStoryTaskDrawerPrivate::MakeText(WidgetTree, *FString::Printf(TEXT("StoryTaskDrawerRowMarker_%d"), Index), GameXXKStoryTaskDrawerPrivate::StateLabel(Entry.State), 12, GameXXKStoryTaskDrawerPrivate::MutedInk);
		UTextBlock* RowTitle = GameXXKStoryTaskDrawerPrivate::MakeText(WidgetTree, *FString::Printf(TEXT("StoryTaskDrawerRowTitle_%d"), Index), Entry.Title, 16);
		UTextBlock* Summary = GameXXKStoryTaskDrawerPrivate::MakeText(WidgetTree, *FString::Printf(TEXT("StoryTaskDrawerRowSummary_%d"), Index), Entry.Summary, 12, GameXXKStoryTaskDrawerPrivate::MutedInk);
		RowTitle->SetAutoWrapText(false);
		RowTitle->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Summary->SetAutoWrapText(false);
		Summary->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		if (UHorizontalBoxSlot* MarkerSlot = FirstLine->AddChildToHorizontalBox(Marker))
		{
			MarkerSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		}
		if (UHorizontalBoxSlot* TitleSlot = FirstLine->AddChildToHorizontalBox(RowTitle))
		{
			TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		RowContents->AddChildToVerticalBox(FirstLine);
		RowContents->AddChildToVerticalBox(Summary);
		if (UVerticalBoxSlot* RowSlot = StoryTaskRows->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(1.0f, 0.0f, 1.0f, 7.0f));
		}
		VisibleRowTaskIds.Add(Entry.TaskId);
		RowButtons.Add(Row);
		RowMarkers.Add(Marker);
		RowTitles.Add(RowTitle);
		RowSummaries.Add(Summary);
	}
	bRestoringListScroll = true;
	StoryTaskList->SetScrollOffset(GetScrollOffsetRef());
	bRestoringListScroll = false;
	RefreshTabVisuals();
	RefreshRowSelectionVisuals();
	RefreshDetail();
}

void UGameXXKStoryTaskDrawerWidget::RefreshRowSelectionVisuals()
{
	const FName SelectedTaskId = GetSelectedTaskIdForTest();
	for (int32 Index = 0; Index < RowButtons.Num(); ++Index)
	{
		const bool bSelected = VisibleRowTaskIds.IsValidIndex(Index) && VisibleRowTaskIds[Index] == SelectedTaskId;
		if (UGameXXKStoryTaskDrawerRowButton* Row = RowButtons[Index])
		{
			Row->SetStyle(bSelected ? RowSelectedButtonStyle : RowNormalButtonStyle);
			Row->SetBackgroundColor(bSelected ? FLinearColor::White : FLinearColor(0.85f, 0.85f, 0.85f, 1.0f));
		}
		if (UTextBlock* Marker = RowMarkers.IsValidIndex(Index) ? RowMarkers[Index] : nullptr)
		{
			Marker->SetColorAndOpacity(FSlateColor(bSelected ? GameXXKStoryTaskDrawerPrivate::Ink : GameXXKStoryTaskDrawerPrivate::MutedInk));
		}
	}
}

void UGameXXKStoryTaskDrawerWidget::RefreshTabVisuals()
{
	using namespace GameXXKStoryTaskDrawerPrivate;
	if (ActionableTabButton)
	{
		ActionableTabButton->SetStyle(UiState.ActiveTab == EGameXXKStoryTaskDrawerTab::Actionable ? TabSelectedButtonStyle : TabNormalButtonStyle);
	}
	if (ClaimableTabButton)
	{
		ClaimableTabButton->SetStyle(UiState.ActiveTab == EGameXXKStoryTaskDrawerTab::Claimable ? TabSelectedButtonStyle : TabNormalButtonStyle);
	}
	if (ClaimableRedDot)
	{
		ClaimableRedDot->SetVisibility(Snapshot.bHasClaimableRedDot ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UGameXXKStoryTaskDrawerWidget::RefreshDetail()
{
	const FGameXXKStoryTaskDrawerEntryView* Entry = GetSelectedEntry();
	if (DetailTitleText)
	{
		DetailTitleText->SetText(Entry ? Entry->Title : FText::FromString(TEXT("暂无任务")));
	}
	if (DetailDescriptionText)
	{
		DetailDescriptionText->SetText(Entry ? Entry->Description : FText::FromString(TEXT("当前页没有可查看的任务。")));
	}
	if (DetailStateText)
	{
		DetailStateText->SetText(Entry ? FText::FromString(TEXT("状态：") + GameXXKStoryTaskDrawerPrivate::StateLabel(Entry->State).ToString()) : FText::GetEmpty());
	}
	if (DetailRewardText)
	{
		DetailRewardText->SetText(Entry ? GameXXKStoryTaskDrawerPrivate::RewardLabel(Entry->MaterialReward) : FText::GetEmpty());
	}
	if (PrimaryActionLabelText)
	{
		PrimaryActionLabelText->SetText(Entry ? Entry->ActionLabel : FText::GetEmpty());
	}
	if (PrimaryActionButton)
	{
		PrimaryActionButton->SetIsEnabled(Entry != nullptr && !Entry->ActionLabel.IsEmpty());
	}
}

void UGameXXKStoryTaskDrawerWidget::NormalizeUiState()
{
	using namespace GameXXKStoryTaskDrawerPrivate;
	if (UiState.ActiveTab != EGameXXKStoryTaskDrawerTab::Actionable && UiState.ActiveTab != EGameXXKStoryTaskDrawerTab::Claimable)
	{
		UiState.ActiveTab = EGameXXKStoryTaskDrawerTab::Actionable;
	}
	if (!ContainsTask(Snapshot.Actionable, UiState.SelectedActionableTaskId))
	{
		UiState.SelectedActionableTaskId = ContainsTask(Snapshot.Actionable, Snapshot.SelectedActionableTaskId)
			? Snapshot.SelectedActionableTaskId
			: (Snapshot.Actionable.IsEmpty() ? NAME_None : Snapshot.Actionable[0].TaskId);
	}
	if (!ContainsTask(Snapshot.Claimable, UiState.SelectedClaimableTaskId))
	{
		UiState.SelectedClaimableTaskId = ContainsTask(Snapshot.Claimable, Snapshot.SelectedClaimableTaskId)
			? Snapshot.SelectedClaimableTaskId
			: (Snapshot.Claimable.IsEmpty() ? NAME_None : Snapshot.Claimable[0].TaskId);
	}
}

const TArray<FGameXXKStoryTaskDrawerEntryView>& UGameXXKStoryTaskDrawerWidget::GetVisibleEntries() const
{
	return UiState.ActiveTab == EGameXXKStoryTaskDrawerTab::Claimable ? Snapshot.Claimable : Snapshot.Actionable;
}

FName& UGameXXKStoryTaskDrawerWidget::GetSelectedTaskIdRef()
{
	return UiState.ActiveTab == EGameXXKStoryTaskDrawerTab::Claimable
		? UiState.SelectedClaimableTaskId
		: UiState.SelectedActionableTaskId;
}

float& UGameXXKStoryTaskDrawerWidget::GetScrollOffsetRef()
{
	return UiState.ActiveTab == EGameXXKStoryTaskDrawerTab::Claimable
		? UiState.ClaimableScrollOffset
		: UiState.ActionableScrollOffset;
}

const FGameXXKStoryTaskDrawerEntryView* UGameXXKStoryTaskDrawerWidget::GetSelectedEntry() const
{
	const FName SelectedTaskId = GetSelectedTaskIdForTest();
	return GetVisibleEntries().FindByPredicate([SelectedTaskId](const FGameXXKStoryTaskDrawerEntryView& Entry)
	{
		return Entry.TaskId == SelectedTaskId;
	});
}

bool UGameXXKStoryTaskDrawerWidget::SelectTab(const EGameXXKStoryTaskDrawerTab Tab)
{
	if (Tab != EGameXXKStoryTaskDrawerTab::Actionable && Tab != EGameXXKStoryTaskDrawerTab::Claimable)
	{
		return false;
	}
	UiState.ActiveTab = Tab;
	RebuildRowsAndDetail();
	return true;
}

void UGameXXKStoryTaskDrawerWidget::EmitPrimaryAction()
{
	if (const FGameXXKStoryTaskDrawerEntryView* Entry = GetSelectedEntry())
	{
		PrimaryActionRequestedDelegate.ExecuteIfBound(Entry->TaskId, Entry->State, Entry->Continuation);
	}
}

void UGameXXKStoryTaskDrawerWidget::HandleCloseClicked()
{
	CloseDrawer();
}

void UGameXXKStoryTaskDrawerWidget::HandleActionableTabClicked()
{
	SelectTab(EGameXXKStoryTaskDrawerTab::Actionable);
}

void UGameXXKStoryTaskDrawerWidget::HandleClaimableTabClicked()
{
	SelectTab(EGameXXKStoryTaskDrawerTab::Claimable);
}

void UGameXXKStoryTaskDrawerWidget::HandlePrimaryActionClicked()
{
	EmitPrimaryAction();
}

void UGameXXKStoryTaskDrawerWidget::HandleListScrolled(const float CurrentOffset)
{
	if (!bRestoringListScroll)
	{
		GetScrollOffsetRef() = CurrentOffset;
	}
}
