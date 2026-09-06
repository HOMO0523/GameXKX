#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"

/** Shared ink/paper treatment for route-owned pages and their readable text. */
class GAMEXXK_API FGameXXKInRunUiStyle
{
public:
	static FLinearColor Ink();
	static FLinearColor MutedInk();
	static FLinearColor Vermilion();
	static FLinearColor Jade();
	static FSlateFontInfo Font(int32 Size, bool bDisplay = false, bool bBold = false);
	static FSlateBrush Paper(const FVector2D& Size);
	static FButtonStyle Action(const FVector2D& Size, bool bPrimary = true);
	static FButtonStyle Choice(const FVector2D& Size, bool bSelected = false);
	static constexpr const TCHAR* PaperPath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge.T_MasterV2_PanelLarge");
	static constexpr const TCHAR* SlotPath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");
};
