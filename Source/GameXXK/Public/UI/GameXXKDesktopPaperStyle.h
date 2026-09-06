#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

class UBorder;
class UWidget;
class UWidgetTree;

/** The authored backpack paper is the reference for every desktop tab. */
namespace GameXXKDesktopPaperStyle
{
	inline constexpr const TCHAR* TexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge.T_MasterV2_PanelLarge");
	inline constexpr float SliceMargin = 0.065f;
	inline const FVector2D BackpackReferenceSize(1450.0f, 849.0f);
	inline const FVector2D BackpackWidgetOffset(-311.0f, -173.0f);
	inline const FVector2D BackpackPaperPosition(274.75f, 151.775f);
	inline const FVector2D BackpackPaperSize(1522.5f, 891.45f);

	GAMEXXK_API FSlateBrush MakeBrush(const FVector2D& Size);
	GAMEXXK_API float GetBackpackScale(const FVector2D& HostSize);
	GAMEXXK_API FMargin GetBackpackOutsets(const FVector2D& HostSize);
	GAMEXXK_API UBorder* MakePanel(UWidgetTree* Tree, FName Name, const FVector2D& BackpackHostSize,
		const FLinearColor& FallbackColor, bool bUseBackpackEdges = false);
	GAMEXXK_API void SetPanelContent(UBorder* Panel, UWidget* Content, const FMargin& Padding);
}
