#include "UI/GameXXKDialogueHistoryWidget.h"
#include "UI/GameXXKPartyDeckUiStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

namespace GameXXKDialogueHistoryPrivate
{
	constexpr int32 MaximumEntries = 100;
	constexpr const TCHAR* PaperTexturePath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelTall.T_MasterV2_PanelTall");

	FSlateBrush PaperBrush()
	{
		FSlateBrush Brush;
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, PaperTexturePath))
		{
			Brush.SetResourceObject(Texture);
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.ImageSize = FVector2D(720.0f, 760.0f);
			Brush.Margin = FMargin(0.065f);
		}
		return Brush;
	}
}

TSharedRef<SWidget> UGameXXKDialogueHistoryWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKDialogueHistoryWidget::PresentHistory(
	const TArray<FGameXXKDialogueHistoryEntry>& Entries)
{
	BuildProgrammaticLayout();
	const int32 StartIndex = FMath::Max(0, Entries.Num() - GameXXKDialogueHistoryPrivate::MaximumEntries);
	VisibleEntries.Reset();
	for (int32 Index = StartIndex; Index < Entries.Num(); ++Index)
	{
		VisibleEntries.Add(Entries[Index]);
	}
	if (HistoryRows)
	{
		HistoryRows->ClearChildren();
		for (int32 Index = 0; Index < VisibleEntries.Num(); ++Index)
		{
			const FGameXXKDialogueHistoryEntry& Entry = VisibleEntries[Index];
			UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(),
				*FString::Printf(TEXT("DialogueHistoryRow%d"), Index));
			const FString Speaker = Entry.SpeakerId.IsNone()
				? FString()
				: Entry.SpeakerId.ToString() + TEXT("：");
			Row->SetText(FText::FromString(Speaker + Entry.Text.ToString()));
			Row->SetAutoWrapText(true);
			Row->SetColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.085f, 0.045f, 1.0f)));
			FSlateFontInfo Font = Row->GetFont();
			Font.Size = 17;
			Row->SetFont(Font);
			if (UVerticalBoxSlot* RowSlot = HistoryRows->AddChildToVerticalBox(Row))
			{
				RowSlot->SetPadding(FMargin(4.0f, 6.0f));
			}
		}
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UGameXXKDialogueHistoryWidget::HideHistory()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

int32 UGameXXKDialogueHistoryWidget::GetHistoryCountForTest() const
{
	return VisibleEntries.Num();
}

FGameXXKDialogueHistoryEntry UGameXXKDialogueHistoryWidget::GetHistoryEntryForTest(const int32 Index) const
{
	return VisibleEntries.IsValidIndex(Index)
		? VisibleEntries[Index]
		: FGameXXKDialogueHistoryEntry();
}

bool UGameXXKDialogueHistoryWidget::IsReadOnlyForTest() const
{
	return GetVisibility() == ESlateVisibility::HitTestInvisible;
}

void UGameXXKDialogueHistoryWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("DialogueHistoryWidgetTree"));
	}
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	PaperFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogueHistoryPaper"));
	PaperFrame->SetBrush(GameXXKDialogueHistoryPrivate::PaperBrush());
	PaperFrame->SetPadding(FMargin(28.0f));
	PaperFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = PaperFrame;
	HistoryScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("DialogueHistoryScroll"));
	FGameXXKPartyDeckUiStyle::ApplyBackpackInkScrollBar(HistoryScroll);
	HistoryScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	HistoryScroll->SetVisibility(ESlateVisibility::Visible);
	PaperFrame->SetContent(HistoryScroll);
	HistoryRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogueHistoryRows"));
	HistoryRows->SetVisibility(ESlateVisibility::HitTestInvisible);
	HistoryScroll->AddChild(HistoryRows);
	HideHistory();
}
