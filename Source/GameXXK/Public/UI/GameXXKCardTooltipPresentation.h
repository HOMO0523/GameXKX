#pragma once

#include "CoreMinimal.h"

class UVerticalBox;
class UWidgetTree;

/** Shared readable sizing for card Tooltip prose and keyword/status Pills. */
struct GAMEXXK_API FGameXXKCardTooltipPresentationStyle
{
	float WrapWidth = 328.0f;
	float RowHeight = 22.0f;
	float BodyFontSize = 13.0f;
	float TargetFontSize = 14.0f;
	float KeywordPillFontSize = 12.0f;
	float StatusPillFontSize = 11.0f;
	FMargin PillPadding = FMargin(5.0f, 2.0f, 5.0f, 2.0f);
	/** Explanations keep their body as prose instead of creating a second set of nested pills. */
	bool bPillHelp = false;
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
