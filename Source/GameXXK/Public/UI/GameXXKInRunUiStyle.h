#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"

/**
 * Which of the two project fonts a piece of text belongs to.
 *
 * The split is a user decision (2026-09-12): titles keep the brush face, every
 * other string is drawn with the readable body face. Never infer the role from
 * size alone - pass the role the widget actually plays.
 */
enum class EGameXXKFontRole : uint8
{
	/** Page/panel headings and proper names: card, equipment, intent, stage, speaker, seal. */
	Title,
	/** Buttons, tabs, labels, prose, numbers, hints, pills, costs and status names. */
	Body,
};

/** Shared ink/paper treatment for route-owned pages and their readable text. */
class GAMEXXK_API FGameXXKInRunUiStyle
{
public:
	static FLinearColor Ink();
	static FLinearColor MutedInk();
	static FLinearColor Vermilion();
	static FLinearColor Jade();
	/** Project font for the requested role. bBold only affects the engine fallback. */
	static FSlateFontInfo Font(EGameXXKFontRole Role, int32 Size, bool bBold = false);
	static FSlateFontInfo TitleFont(int32 Size, bool bBold = false);
	static FSlateFontInfo BodyFont(int32 Size, bool bBold = false);
	static FSlateFontInfo OutlinedFont(EGameXXKFontRole Role, int32 Size, int32 OutlineSize = 2);
	static FSlateFontInfo OutlinedTitleFont(int32 Size, int32 OutlineSize = 2);
	static FSlateFontInfo OutlinedBodyFont(int32 Size, int32 OutlineSize = 2);
	/** Runtime font asset path for a role, also used by font-role tests and audits. */
	static const TCHAR* FontPath(EGameXXKFontRole Role);
	static FSlateBrush Paper(const FVector2D& Size);
	static FButtonStyle Action(const FVector2D& Size, bool bPrimary = true);
	static FButtonStyle Choice(const FVector2D& Size, bool bSelected = false);
	static constexpr const TCHAR* TitleFontPath =
		TEXT("/Game/GameXXK/UI/Fonts/Trial/FF_Trial_ZhHans_JiangHuGuFeng_Font.FF_Trial_ZhHans_JiangHuGuFeng_Font");
	static constexpr const TCHAR* BodyFontPath =
		TEXT("/Game/GameXXK/UI/Fonts/Body/FF_Body_KeinannMaruPOP_Font.FF_Body_KeinannMaruPOP_Font");
	static constexpr const TCHAR* PaperPath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge.T_MasterV2_PanelLarge");
	static constexpr const TCHAR* SlotPath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");
};
