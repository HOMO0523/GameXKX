#include "UI/GameXXKCardTooltipPresentation.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

float GameXXKCardTooltipPresentation::CompactWidth(const FString& Title, const FString& Text)
{
	float Width = 236.0f;
	if (FSlateApplication::IsInitialized())
	{
		const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		Width = Measure->Measure(Title, FGameXXKInRunUiStyle::Font(28, true)).X;
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		for (const FString& Line : Lines)
		{
			Width = FMath::Max(Width, static_cast<float>(Measure->Measure(Line, FGameXXKInRunUiStyle::Font(20, true)).X));
		}
	}
	// PopulateBody reserves another 12px for composite-font overhang.
	return FMath::Clamp(FMath::CeilToFloat(Width) + 56.0f, 280.0f, 560.0f);
}

UWidget* GameXXKCardTooltipPresentation::BuildCompactTooltip(UWidgetTree* Tree, const FText& Title, const FString& Text)
{
	if (!Tree) return nullptr;
	UBorder* Paper = Tree->ConstructWidget<UBorder>();
	FSlateBrush Brush;
	Brush.SetResourceObject(LoadObject<UTexture2D>(nullptr, FGameXXKInRunUiStyle::SlotPath));
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.ImageSize = FVector2D(480, 320);
	Brush.Margin = FMargin(0.065f);
	Paper->SetBrush(Brush);
	Paper->SetPadding(FMargin(16, 12));
	Paper->SetVisibility(ESlateVisibility::HitTestInvisible);
	const float Width = CompactWidth(Title.ToString(), Text);
	USizeBox* Size = Tree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(Width - 32);
	Paper->SetContent(Size);
	UVerticalBox* Contents = Tree->ConstructWidget<UVerticalBox>();
	Size->SetContent(Contents);
	UTextBlock* TitleText = Tree->ConstructWidget<UTextBlock>();
	TitleText->SetText(Title);
	FSlateFontInfo Font = FGameXXKInRunUiStyle::Font(28, true);
	Font.OutlineSettings.OutlineSize = 1;
	Font.OutlineSettings.OutlineColor = FLinearColor(0.08f,0.06f,0.04f,1);
	TitleText->SetFont(Font);
	TitleText->SetColorAndOpacity(Font.OutlineSettings.OutlineColor);
	TitleText->SetLineHeightPercentage(0.85f);
	TitleText->SetApplyLineHeightToBottomLine(true);
	TitleText->SetAutoWrapText(true);
	Contents->AddChildToVerticalBox(TitleText);
	UVerticalBox* Body = Tree->ConstructWidget<UVerticalBox>();
	Contents->AddChildToVerticalBox(Body)->SetPadding(FMargin(0,6,0,0));
	FGameXXKCardTooltipPresentationStyle Style;
	Style.WrapWidth = Width - 44;
	Style.bDisplayBodyFont = true;
	const float BodyHeight = PopulateBody(Tree, Body, Title.ToString(), Text, Style);
	Size->SetMinDesiredHeight(BodyHeight + 46);
	return Paper;
}
