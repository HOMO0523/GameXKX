#pragma once

#include "CoreMinimal.h"
#include "GameXXKEquipmentToolRules.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "GameXXKDesktopWorkbenchSessionState.generated.h"

UENUM(BlueprintType)
enum class EGameXXKDesktopTrainingNav : uint8
{
	None,
	Warehouse,
	Formation,
	Talents,
	Tools,
	Training
};

UENUM(BlueprintType)
enum class EGameXXKDesktopTrainingCenterPage : uint8
{
	Backpack,
	Formation,
	Talents
};

UENUM(BlueprintType)
enum class EGameXXKDesktopTrainingRightPanel : uint8
{
	None,
	TrainingMap,
	Tools
};

UENUM(BlueprintType)
enum class EGameXXKDesktopTrainingCharacterRoster : uint8
{
	Hero,
	Companions,
	Npcs
};

UENUM(BlueprintType)
enum class EGameXXKDesktopHudPresentationMode : uint8
{
	DesktopWindow,
	TownViewport
};

UENUM(BlueprintType)
enum class EGameXXKDesktopToolMode : uint8
{
	Dismantle,
	Combine,
	Enhance,
	Reforge,
	Socket
};

UENUM(BlueprintType)
enum class EGameXXKDesktopNoticeCategory : uint8
{
	ChestAcquired,
	ChestOpenResult,
	StageCleared,
	StageFailed,
	CharacterLevelUp,
	CharacterDeath,
	CharacterRevive,
	EquipmentCombine,
	EnhanceReforge,
	Socket,
	System
};

struct GAMEXXK_API FGameXXKDesktopWorkbenchSessionState
{
	bool bValid = false;
	bool bBackpackExpanded = false;
	bool bWarehousePanelOpen = false;
	int32 WarehousePageIndex = 0;
	EGameXXKDesktopTrainingNav ActiveNav = EGameXXKDesktopTrainingNav::None;
	EGameXXKDesktopTrainingCenterPage ActiveCenterPage =
		EGameXXKDesktopTrainingCenterPage::Backpack;
	EGameXXKDesktopTrainingRightPanel RightPanel =
		EGameXXKDesktopTrainingRightPanel::None;
	EGameXXKDesktopToolMode ActiveToolMode = EGameXXKDesktopToolMode::Dismantle;
	EGameXXKToolCombineKind ActiveToolCombineKind = EGameXXKToolCombineKind::Equipment;
	int32 SelectedToolSocketIndex = 0;
	EGameXXKDesktopTrainingCharacterRoster ActiveCharacterRoster =
		EGameXXKDesktopTrainingCharacterRoster::Hero;
	EGameXXKDesktopTrainingCharacterRoster ActiveFormationRoster =
		EGameXXKDesktopTrainingCharacterRoster::Companions;
	FName SelectedStageId = NAME_None;
	int32 ActiveTrainingDifficultyIndex = 0;
	int32 ActiveTrainingChapter = 1;
	FName ActiveBackpackCharacterId = NAME_None;
	FName LastCompanionBackpackCharacterId = NAME_None;
	FName LastNpcBackpackCharacterId = NAME_None;
	FName FormationCandidateCharacterId = NAME_None;
	FName FormationDeckCharacterId = NAME_None;
	bool bFormationPickerOpen = false;
	int32 FormationPickerPageIndex = 0;
	bool bCharacterRosterMembersExpanded = false;
	bool bSettingsPanelOpen = false;
	FGameXXKEmbeddedInventorySessionState EmbeddedInventory;

	bool operator==(const FGameXXKDesktopWorkbenchSessionState& Other) const
	{
		return bValid == Other.bValid
			&& bBackpackExpanded == Other.bBackpackExpanded
			&& bWarehousePanelOpen == Other.bWarehousePanelOpen
			&& WarehousePageIndex == Other.WarehousePageIndex
			&& ActiveNav == Other.ActiveNav
			&& ActiveCenterPage == Other.ActiveCenterPage
			&& RightPanel == Other.RightPanel
			&& ActiveToolMode == Other.ActiveToolMode
			&& ActiveToolCombineKind == Other.ActiveToolCombineKind
			&& SelectedToolSocketIndex == Other.SelectedToolSocketIndex
			&& ActiveCharacterRoster == Other.ActiveCharacterRoster
			&& ActiveFormationRoster == Other.ActiveFormationRoster
			&& SelectedStageId == Other.SelectedStageId
			&& ActiveTrainingDifficultyIndex == Other.ActiveTrainingDifficultyIndex
			&& ActiveTrainingChapter == Other.ActiveTrainingChapter
			&& ActiveBackpackCharacterId == Other.ActiveBackpackCharacterId
			&& LastCompanionBackpackCharacterId == Other.LastCompanionBackpackCharacterId
			&& LastNpcBackpackCharacterId == Other.LastNpcBackpackCharacterId
			&& FormationCandidateCharacterId == Other.FormationCandidateCharacterId
			&& FormationDeckCharacterId == Other.FormationDeckCharacterId
			&& bFormationPickerOpen == Other.bFormationPickerOpen
			&& FormationPickerPageIndex == Other.FormationPickerPageIndex
			&& bCharacterRosterMembersExpanded == Other.bCharacterRosterMembersExpanded
			&& bSettingsPanelOpen == Other.bSettingsPanelOpen
			&& EmbeddedInventory.CharacterId == Other.EmbeddedInventory.CharacterId
			&& EmbeddedInventory.ActiveInventoryFilter == Other.EmbeddedInventory.ActiveInventoryFilter
			&& EmbeddedInventory.ActiveCharacterTab == Other.EmbeddedInventory.ActiveCharacterTab
			&& EmbeddedInventory.bBackpackSorted == Other.EmbeddedInventory.bBackpackSorted
			&& EmbeddedInventory.DeckColumns == Other.EmbeddedInventory.DeckColumns
			&& EmbeddedInventory.bDeckExpanded == Other.EmbeddedInventory.bDeckExpanded
			&& EmbeddedInventory.bDeckDraftInitialized == Other.EmbeddedInventory.bDeckDraftInitialized
			&& FMath::IsNearlyEqual(
				EmbeddedInventory.BackpackScrollOffset,
				Other.EmbeddedInventory.BackpackScrollOffset)
			&& EmbeddedInventory.PendingDeckIds == Other.EmbeddedInventory.PendingDeckIds;
	}
};
