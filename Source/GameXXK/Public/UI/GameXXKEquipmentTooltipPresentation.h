#pragma once
#include "CoreMinimal.h"
#include "GameXXKEquipmentTypes.h"
class UGameXXKMVPSubsystem;
class UWidgetTree;
class UWidget;
class UBorder;
class UButton;
class UImage;
namespace GameXXKEquipmentTooltipPresentation
{
	GAMEXXK_API FText GetModifierLabel(EGameXXKEquipmentModifierKind Kind);
	GAMEXXK_API FText BuildDetail(const UGameXXKMVPSubsystem* Subsystem,FName InstanceId);
	GAMEXXK_API FString AffixLine(const FGameXXKEquipmentAffixRoll& Roll);
	GAMEXXK_API UWidget* Build(UWidgetTree* Tree,const UGameXXKMVPSubsystem* Subsystem,FName InstanceId,FName CompareCharacterId=NAME_None);
	GAMEXXK_API void Populate(UBorder* Frame,UWidgetTree* Tree,const UGameXXKMVPSubsystem* Subsystem,FName InstanceId,FName CompareCharacterId=NAME_None);
	/** Stable binding with a rounded native popup silhouette matching the UI mask. */
	GAMEXXK_API void Bind(UWidget* Owner,UWidget* Content);
	GAMEXXK_API UWidget* BuildGem(UWidgetTree* Tree,FName ItemId);
	/** Reuses equipment quality layers and the stable rounded tooltip for an item gem. */
	GAMEXXK_API bool ApplyGem(UWidgetTree* Tree,UButton* Button,FName ItemId,FVector2D Size,UImage* Icon=nullptr);
}
