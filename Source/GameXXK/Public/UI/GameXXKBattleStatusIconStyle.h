#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"
#include "UObject/SoftObjectPath.h"

/**
 * Pure presentation data for one battle-foot HUD status.  The texture paths
 * intentionally point at the deterministic future imports; callers must keep
 * the native paper-and-ink fallback visible while a texture is unavailable.
 */
struct GAMEXXK_API FGameXXKBattleStatusIconStyle
{
	FName IconId = NAME_None;
	FSoftObjectPath TexturePath;
	FString DisplayName;
	/** One concise player-facing rule line. The title and live layer count are added by DescribeStatusTooltip. */
	FString Tooltip;
	FLinearColor Tint = FLinearColor::White;
	int32 Priority = 0;
	/** Native simple-glyph presentation used before a matching Texture2D is imported. */
	FString FallbackGlyph;
	bool bUsesPaperInkFallback = true;
	bool bFallback = false;

	static FGameXXKBattleStatusIconStyle ResolveArmorIconStyle();
	static FGameXXKBattleStatusIconStyle ResolveStatusIconStyle(EGameXXKCardStatus Status);
	static FString DescribeStatusTooltip(const FGameXXKBattleStatusIconStyle& Style, int32 Stacks);
};

/** One visible, numeric paper badge.  It is a UI projection only and never owns combat state. */
struct GAMEXXK_API FGameXXKBattleStatusBadgeModel
{
	FGameXXKBattleStatusIconStyle Style;
	int32 Stacks = 0;
	FString Tooltip;
};
