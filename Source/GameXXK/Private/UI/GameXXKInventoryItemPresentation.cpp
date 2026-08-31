#include "UI/GameXXKInventoryItemPresentation.h"

#include "GameXXKMVPRules.h"

namespace GameXXKInventoryItemPresentationPrivate
{
	const TCHAR* TutorialMapIcon =
		TEXT("/Game/GameXXK/UI/Relics/Icons/T_Relic_OldMap.T_Relic_OldMap");
	const TCHAR* TutorialMapInspection =
		TEXT("/Game/GameXXK/Narrative/Items/T_Tutorial_XuXiakeTravelRouteInspect.T_Tutorial_XuXiakeTravelRouteInspect");
}

FString FGameXXKInventoryItemPresentation::ResolveIconPath(const FName ItemId)
{
	return IsInspectable(ItemId)
		? FString(GameXXKInventoryItemPresentationPrivate::TutorialMapIcon)
		: FString();
}

bool FGameXXKInventoryItemPresentation::IsInspectable(const FName ItemId)
{
	return ItemId == UGameXXKMVPRules::ItemTutorialRiverMap();
}

FString FGameXXKInventoryItemPresentation::InspectTexturePath(const FName ItemId)
{
	return IsInspectable(ItemId)
		? FString(GameXXKInventoryItemPresentationPrivate::TutorialMapInspection)
		: FString();
}
