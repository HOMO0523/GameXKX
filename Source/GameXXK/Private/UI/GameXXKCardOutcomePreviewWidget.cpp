#include "UI/GameXXKCardOutcomePreviewWidget.h"
#include "UI/GameXXKInRunUiStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

namespace
{
	constexpr int32 MaxOutcomeLines = 8;
	constexpr int32 OutcomeFontSize = 20;
	// Same approved MasterV2 item-slot paper as the pending-choice panel:
	// straight, regular warm border drawn at the texture's authored size with
	// the project's fixed 0.065 nine-slice margin.
	constexpr const TCHAR* OutcomeTooltipPaperTexturePath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");
	const FVector2D OutcomeTooltipPaperImageSize(100.0f, 101.0f);
	const FMargin OutcomeTooltipPaperMargin(0.065f);
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
		FSlateFontInfo Font = FGameXXKInRunUiStyle::BodyFont(OutcomeFontSize);
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.07f, 0.055f, 0.04f, 0.78f);
		TextBlock->SetFont(Font);
		TextBlock->SetLineHeightPercentage(0.85f);
		TextBlock->SetApplyLineHeightToBottomLine(true);
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
	if (!Lines.IsEmpty())
		if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(Slot)) PanelSlot->SetSize(GetPreferredPanelSize());
	RefreshLines();
}

FVector2D UGameXXKCardOutcomePreviewWidget::GetPreferredPanelSize() const
{
	const FSlateFontInfo Font = FGameXXKInRunUiStyle::BodyFont(OutcomeFontSize);
	const auto Measure = [&](const FText& Text)
	{
		return FSlateApplication::IsInitialized()
			? FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text,Font).X
			: Text.ToString().Len()*static_cast<float>(OutcomeFontSize);
	};
	float Longest = 0;
	for (const auto& Line : Lines)
	{
		float Width = 0;
		for (int32 Index=0; Index<Line.Segments.Num(); ++Index) Width += Measure(Line.Segments[Index].Text) + (Index>0 ? 6 : 0);
		Longest = FMath::Max(Longest,Width);
	}
	const float Width = FMath::Clamp(Longest+28,180.0f,600.0f);
	int32 Rows = 0;
	for (const auto& Line : Lines)
	{
		float Used = 0; ++Rows;
		for (const auto& Segment : Line.Segments)
		{
			const float Cell = Measure(Segment.Text);
			if (Used>0 && Used+6+Cell>Width-20) { ++Rows; Used=0; }
			Used += Cell + (Used>0 ? 6 : 0);
		}
	}
	const float LineHeight = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(TEXT("国"),Font).Y*0.85f : 28.0f;
	return FVector2D(Width,FMath::Max(40.0f,Rows*(LineHeight+2)+12));
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
		UWrapBox* Row = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		Row->SetVisibility(ESlateVisibility::HitTestInvisible);
		Row->SetExplicitWrapSize(true); Row->SetWrapSize(GetPreferredPanelSize().X-20);
		Row->SetInnerSlotPadding(FVector2D(6,0));
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
			if (UWrapBoxSlot* SegmentSlot = Row->AddChildToWrapBox(SegmentText))
			{
				SegmentSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}
}
