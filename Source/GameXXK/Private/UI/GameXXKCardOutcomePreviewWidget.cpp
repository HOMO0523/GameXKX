#include "UI/GameXXKCardOutcomePreviewWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr int32 MaxOutcomeLines = 3;
	constexpr int32 OutcomeFontSize = 18;
	constexpr const TCHAR* OutcomeTooltipPaperTexturePath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_TooltipPaper.T_MasterV2_TooltipPaper");
	const FVector2D OutcomeTooltipPaperImageSize(520.0f, 240.0f);
	const FMargin OutcomeTooltipPaperMargin(12.0f / 520.0f, 10.0f / 240.0f);
	const FMargin OutcomeTooltipPaperPadding(10.0f, 6.0f);

	FLinearColor ResolveSegmentColor(const EGameXXKCardOutcomeTone Tone)
	{
		switch (Tone)
		{
		case EGameXXKCardOutcomeTone::Damage:
			return FLinearColor(0.66f, 0.24f, 0.20f, 1.0f);
		case EGameXXKCardOutcomeTone::Dot:
			return FLinearColor(0.25f, 0.48f, 0.31f, 1.0f);
		case EGameXXKCardOutcomeTone::Medicine:
			return FLinearColor(0.58f, 0.39f, 0.20f, 1.0f);
		case EGameXXKCardOutcomeTone::Healing:
			return FLinearColor(0.24f, 0.55f, 0.46f, 1.0f);
		case EGameXXKCardOutcomeTone::Armor:
			return FLinearColor(0.34f, 0.45f, 0.55f, 1.0f);
		case EGameXXKCardOutcomeTone::Lethal:
			return FLinearColor(0.82f, 0.34f, 0.26f, 1.0f);
		case EGameXXKCardOutcomeTone::Neutral:
		default:
			return FLinearColor(0.79f, 0.75f, 0.66f, 1.0f);
		}
	}

	void ConfigureSegmentText(UTextBlock* TextBlock, const FGameXXKCardOutcomeTextSegment& Segment)
	{
		if (!TextBlock)
		{
			return;
		}

		TextBlock->SetText(Segment.Text);
		TextBlock->SetColorAndOpacity(FSlateColor(ResolveSegmentColor(Segment.Tone)));
		TextBlock->SetJustification(ETextJustify::Left);
		TextBlock->SetAutoWrapText(false);
		TextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.36f));
		TextBlock->SetShadowOffset(FVector2D(0.5f, 0.5f));
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), OutcomeFontSize);
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.07f, 0.055f, 0.04f, 0.78f);
		TextBlock->SetFont(Font);
		TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UGameXXKCardOutcomePreviewWidget::SetLines(const TArray<FGameXXKCardOutcomeTextLine>& InLines)
{
	Lines.Reset(FMath::Min(InLines.Num(), MaxOutcomeLines));
	for (int32 LineIndex = 0; LineIndex < InLines.Num() && LineIndex < MaxOutcomeLines; ++LineIndex)
	{
		Lines.Add(InLines[LineIndex]);
	}

	SetVisibility(Lines.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	RefreshLines();
}

void UGameXXKCardOutcomePreviewWidget::Clear()
{
	Lines.Reset();
	if (LineBox)
	{
		LineBox->ClearChildren();
		LineBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

int32 UGameXXKCardOutcomePreviewWidget::GetVisibleLineCountForTest() const
{
	return Lines.Num();
}

FString UGameXXKCardOutcomePreviewWidget::GetPlainLineForTest(const int32 LineIndex) const
{
	if (!Lines.IsValidIndex(LineIndex))
	{
		return FString();
	}

	FString PlainLine;
	for (const FGameXXKCardOutcomeTextSegment& Segment : Lines[LineIndex].Segments)
	{
		PlainLine += Segment.Text.ToString();
	}
	return PlainLine;
}

FLinearColor UGameXXKCardOutcomePreviewWidget::GetSegmentColorForTest(
	const int32 LineIndex,
	const int32 SegmentIndex) const
{
	if (!Lines.IsValidIndex(LineIndex) || !Lines[LineIndex].Segments.IsValidIndex(SegmentIndex))
	{
		return FLinearColor::Transparent;
	}
	return ResolveSegmentColor(Lines[LineIndex].Segments[SegmentIndex].Tone);
}

FString UGameXXKCardOutcomePreviewWidget::GetBackgroundResourcePathForTest() const
{
	return BackgroundTexture ? BackgroundTexture->GetPathName() : FString();
}

TSharedRef<SWidget> UGameXXKCardOutcomePreviewWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("CardOutcomePreviewWidgetTree"));
	}
	if (WidgetTree && (!BackgroundBorder || !LineBox))
	{
		BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("CardOutcomePreviewPaper"));
		LineBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			TEXT("CardOutcomePreviewLines"));
		if (BackgroundBorder && LineBox)
		{
			if (!BackgroundTexture)
			{
				BackgroundTexture = LoadObject<UTexture2D>(nullptr, OutcomeTooltipPaperTexturePath);
			}
			FSlateBrush BackgroundBrush;
			BackgroundBrush.SetResourceObject(BackgroundTexture);
			BackgroundBrush.ImageSize = OutcomeTooltipPaperImageSize;
			BackgroundBrush.DrawAs = ESlateBrushDrawType::Box;
			BackgroundBrush.Margin = OutcomeTooltipPaperMargin;
			BackgroundBrush.TintColor = FSlateColor(FLinearColor::White);
			BackgroundBorder->SetBrush(BackgroundBrush);
			BackgroundBorder->SetBrushColor(FLinearColor::White);
			BackgroundBorder->SetPadding(OutcomeTooltipPaperPadding);
			BackgroundBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
			LineBox->SetVisibility(ESlateVisibility::HitTestInvisible);
			BackgroundBorder->SetContent(LineBox);
			WidgetTree->RootWidget = BackgroundBorder;
		}
	}
	RefreshLines();
	return Super::RebuildWidget();
}

void UGameXXKCardOutcomePreviewWidget::RefreshLines()
{
	if (!LineBox || !WidgetTree)
	{
		return;
	}

	LineBox->ClearChildren();
	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UVerticalBoxSlot* RowSlot = LineBox->AddChildToVerticalBox(Row))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Left);
			RowSlot->SetVerticalAlignment(VAlign_Center);
		}

		const FGameXXKCardOutcomeTextLine& Line = Lines[LineIndex];
		for (int32 SegmentIndex = 0; SegmentIndex < Line.Segments.Num(); ++SegmentIndex)
		{
			UTextBlock* SegmentText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			ConfigureSegmentText(SegmentText, Line.Segments[SegmentIndex]);
			if (UHorizontalBoxSlot* SegmentSlot = Row->AddChildToHorizontalBox(SegmentText))
			{
				SegmentSlot->SetPadding(FMargin(SegmentIndex > 0 ? 6.0f : 0.0f, 0.0f, 0.0f, 0.0f));
				SegmentSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}
}
