#pragma once

#include "CoreMinimal.h"

class UVerticalBox;
class UWidgetTree;
class UWidget;

namespace GameXXKCardTooltipPresentation
{
	inline constexpr float MinimumWidth = 520.0f;
	inline constexpr float MaximumWidth = 800.0f;
	/** Short relic/reward prose follows actual glyph width, independently of full card rules. */
	GAMEXXK_API float CompactWidth(const FString& Title, const FString& Text);
	GAMEXXK_API UWidget* BuildCompactTooltip(UWidgetTree* Tree, const FText& Title, const FString& Text);
	inline float PreferredWidth(const FString& Text)
	{
		return Text.Len() >= 320 ? 760.0f : Text.Len() >= 220 ? 680.0f : Text.Len() >= 140 ? 600.0f : MinimumWidth;
	}
}

/** Shared readable sizing for card Tooltip prose and keyword/status Pills. */
struct GAMEXXK_API FGameXXKCardTooltipPresentationStyle
{
	float WrapWidth = GameXXKCardTooltipPresentation::MinimumWidth - 44.0f;
	float RowHeight = 32.0f;
	float BodyFontSize = 20.0f;
	float TargetFontSize = 20.0f;
	float KeywordPillFontSize = 17.0f;
	float StatusPillFontSize = 16.0f;
	FMargin PillPadding = FMargin(5.0f, 2.0f, 5.0f, 2.0f);
	/** Explanations keep their body as prose instead of creating a second set of nested pills. */
	bool bPillHelp = false;
	/** Short battle hints use display lettering; long card prose keeps its reading font. */
	bool bDisplayBodyFont = false;
};

namespace GameXXKCardTooltipPresentation
{
	/** Populates one fixed-width body with wrapped prose and keyword/status Pills. */
	GAMEXXK_API float PopulateBody(
		UWidgetTree* WidgetTree,
		UVerticalBox* BodyBox,
		const FString& Title,
		const FString& Text,
		const FGameXXKCardTooltipPresentationStyle& Style = FGameXXKCardTooltipPresentationStyle());

	/** Appends one authoritative rule line for every status Pill mentioned by full card text. */
	GAMEXXK_API FString AppendStatusPillExplanations(const FString& Text);
}
