#pragma once

#include "CoreMinimal.h"
#include "Dialogue/GameXXKDialogueTypes.h"
#include "Guide/GameXXKGuideAsset.h"
#include "GameXXKCardRunTypes.h"
#include "GameXXKEquipmentTypes.h"
#include "GameXXKMetaShopTypes.h"
#include "GameXXKTalentTypes.h"
#include "GameXXKTrainingRules.h"
#include "Narrative/GameXXKNarrativeSequenceTypes.h"
#include "Narrative/GameXXKNarrativeTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameXXKMVPRules.generated.h"

UENUM(BlueprintType)
enum class EGameXXKScreen : uint8
{
	MainMenu,
	WorldMap,
	Town,
	DungeonMap,
	RouteEvent,
	RouteCamp,
	RouteMerchant,
	Battle
};

UENUM(BlueprintType)
enum class EGameXXKTownPanelMode : uint8
{
	None,
	Inventory,
	Character,
	Trade
};

UENUM(BlueprintType)
enum class EGameXXKQuestState : uint8
{
	NotAccepted,
	Accepted,
	Completed
};

/** Independent onboarding quest state. It must never alias the legacy Qingshan main quest. */
UENUM(BlueprintType)
enum class EGameXXKTutorialQuestState : uint8
{
	NotStarted,
	Active,
	Completed
};

USTRUCT(BlueprintType)
struct FGameXXKTutorialQuestProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKTutorialQuestState State = EGameXXKTutorialQuestState::NotStarted;

	/** Stable step ID; the first implemented sequence begins at Tutorial.EnterTown. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName CurrentStepId = NAME_None;
};

UENUM(BlueprintType)
enum class EGameXXKTaskCategory : uint8
{
	Main,
	Side
};

UENUM(BlueprintType)
enum class EGameXXKNodeKind : uint8
{
	Start,
	Battle,
	Elite,
	Event,
	Camp,
	Chest,
	Merchant,
	Boss
};

UENUM(BlueprintType)
enum class EGameXXKItemKind : uint8
{
	Consumable,
	Weapon,
	Armor,
	Accessory,
	Material,
	Task
};

UENUM(BlueprintType)
enum class EGameXXKCodexCategory : uint8
{
	All,
	Hero,
	Spirit,
	Monster,
	Beast
};

USTRUCT(BlueprintType)
struct FGameXXKItemDef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Id;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKItemKind Kind = EGameXXKItemKind::Consumable;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 BuyPrice = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SellPrice = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 HealAmount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MPHealAmount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 AttackBonus = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 DefenseBonus = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MaxHPBonus = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MaxMPBonus = 0;
};

USTRUCT(BlueprintType)
struct FGameXXKCodexEntryDef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCodexCategory Category = EGameXXKCodexCategory::All;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText Description;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSoftObjectPath IconPath;
};

USTRUCT(BlueprintType)
struct FGameXXKCodexEntryView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCodexCategory Category = EGameXXKCodexCategory::All;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText Description;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSoftObjectPath IconPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsDiscovered = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsRead = false;
};

USTRUCT(BlueprintType)
struct FGameXXKTaskReward
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Gold = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Experience = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Token = 0;
};

USTRUCT(BlueprintType)
struct FGameXXKTaskView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Id;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKTaskCategory Category = EGameXXKTaskCategory::Main;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText Title;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText Description;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ProgressCurrent = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ProgressTarget = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameXXKTaskReward Reward;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bCanNavigate = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName NavigationTarget;
};

USTRUCT(BlueprintType)
struct FGameXXKBattleUnit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Id;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 HP = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Attack = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Defense = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Speed = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Weakness;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Shield = 1;
};

USTRUCT(BlueprintType)
struct FGameXXKBattleRuntimeUnit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Id;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 HP = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MaxHP = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MP = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MaxMP = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Attack = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Defense = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Speed = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName EnemyDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 BattleSlotNumber = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 CombatLevel = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Shield = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDefending = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bEnemy = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDefeated = false;
};

USTRUCT(BlueprintType)
struct FGameXXKRouteMapEdge
{
	GENERATED_BODY()

	FGameXXKRouteMapEdge() = default;

	FGameXXKRouteMapEdge(int32 InFromNodeId, int32 InToNodeId)
		: FromNodeId(InFromNodeId)
		, ToNodeId(InToNodeId)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 FromNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ToNodeId = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FGameXXKRouteMapNode
{
	GENERATED_BODY()

	FGameXXKRouteMapNode() = default;

	FGameXXKRouteMapNode(int32 InNodeId, int32 InLayerIndex, int32 InColumnIndex, EGameXXKNodeKind InNodeKind, FVector2D InNormalizedPosition, const TArray<int32>& InOutgoingNodeIds)
		: NodeId(InNodeId)
		, LayerIndex(InLayerIndex)
		, ColumnIndex(InColumnIndex)
		, NodeKind(InNodeKind)
		, NormalizedPosition(InNormalizedPosition)
		, OutgoingNodeIds(InOutgoingNodeIds)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 NodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 LayerIndex = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ColumnIndex = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKNodeKind NodeKind = EGameXXKNodeKind::Start;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector2D NormalizedPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<int32> OutgoingNodeIds;
};

/** Stable identity stored in one physical desktop backpack/warehouse cell. */
USTRUCT(BlueprintType)
struct FGameXXKDesktopInventoryEntryKey
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bEquipmentInstance = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName EntryId = NAME_None;

	bool IsValid() const
	{
		return !EntryId.IsNone();
	}

	bool operator==(const FGameXXKDesktopInventoryEntryKey& Other) const
	{
		return bEquipmentInstance == Other.bEquipmentInstance && EntryId == Other.EntryId;
	}

	bool operator!=(const FGameXXKDesktopInventoryEntryKey& Other) const
	{
		return !(*this == Other);
	}
};

FORCEINLINE uint32 GetTypeHash(const FGameXXKDesktopInventoryEntryKey& Key)
{
	return HashCombine(GetTypeHash(Key.bEquipmentInstance), GetTypeHash(Key.EntryId));
}

/**
 * Save-authoritative physical placement for the desktop backpack and warehouse.
 * Equipment remains in the validated equipment collection; the warehouse list
 * is a strict subset that partitions unequipped instances between both panels.
 */
USTRUCT(BlueprintType)
struct FGameXXKDesktopInventoryState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> WarehouseEquipmentInstanceIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TMap<FName, int32> WarehouseItems;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKDesktopInventoryEntryKey> BackpackSlots;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKDesktopInventoryEntryKey> WarehouseSlots;

	/** Stable equipment-instance locks shared by Backpack, Warehouse, and loadouts. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TSet<FName> LockedEquipmentInstanceIds;

	/** Whole-stack item-type locks shared by Backpack and Warehouse. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TSet<FName> LockedItemIds;

	/** Unique task items waiting for the first free Backpack/Warehouse cell. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TSet<FName> PendingTaskItemIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bToolAutoFillIncludesWarehouse = true;
};

USTRUCT(BlueprintType)
struct FGameXXKToolProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Level = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int64 Experience = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SelectedCraftingLevel = 1;
};

/**
 * Save-authoritative snapshot of the generated-route state immediately before
 * the player commits a Battle, Elite, or Boss node. It intentionally excludes
 * route economy already earned from completed nodes.
 */
USTRUCT(BlueprintType)
struct FGameXXKBattleEntryCheckpoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bValid = false;

	/** Selected encounter node. It matches PendingRouteNodeId while the battle is active. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SourceNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PreviousCurrentRouteNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PreviousDungeonNodeIndex = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PreviousPlayerHP = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PreviousPlayerMP = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<int32> PreviousVisitedRouteNodeIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<int32> PreviousReachableRouteNodeIds;
};

USTRUCT(BlueprintType)
struct FGameXXKRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKScreen Screen = EGameXXKScreen::MainMenu;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKQuestState QuestState = EGameXXKQuestState::NotAccepted;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName TrackedTaskId = NAME_None;

	/** v27+ tutorial flow, deliberately separate from QuestState/TrackedTaskId. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKTutorialQuestProgress TutorialQuest;

	/** v28+ save-resumable blocking dialogue state. World Actor pointers never enter this structure. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKDialogueSessionState DialogueSession;

	/** v29+ one active replaceable-scene narrative sequence. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKNarrativeSequenceSessionState NarrativeSequenceSession;

	/** v29+ concurrent Story/Task progress and the independently tracked task. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKNarrativeProgress NarrativeProgress;

	/** v29+ first-entry preference and resumable combat-guide progress. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKGuideProgress GuideProgress;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName CurrentRegion;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName CurrentMapId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bHasPlayerLocation = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerLevel = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerXP = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerGold = 10000;

	// Legacy HUD mirror of Item.EnhancementStone. Rules keep this synchronized for saved games and existing UI.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 EnhancementMaterial = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerHP = 100;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerMaxHP = 100;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerMP = 30;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerMaxMP = 30;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerAttack = 15;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerDefense = 8;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerSpeed = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bFollowerJoined = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bHasQuestNpcLocation = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector QuestNpcLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDungeonActive = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 DungeonNodeIndex = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bHasGeneratedRouteMap = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 RouteSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 CurrentRouteNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PendingRouteNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FGameXXKRouteMapNode> RouteMapNodes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FGameXXKRouteMapEdge> RouteMapEdges;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<int32> VisitedRouteNodeIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<int32> ReachableRouteNodeIds;

	/** v23+ exact rollback source for player-confirmed encounter retreat. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKBattleEntryCheckpoint BattleEntryCheckpoint;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bHasActiveBattle = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ActiveBattleNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FGameXXKBattleRuntimeUnit> ActiveBattleEnemies;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FGameXXKBattleRuntimeUnit> ActiveBattleParty;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName EquippedWeapon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName EquippedArmor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName EquippedAccessory;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKTownPanelMode TownPanelMode = EGameXXKTownPanelMode::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSet<FName> UnlockedRegions;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FName, int32> Inventory;

	/** v21+ physical backpack/warehouse partition used by the pure-2D desktop shell. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKDesktopInventoryState DesktopInventory;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKToolProgress ToolProgress;

	/** Permanent shared talent ranks; aggregate bonuses are always derived. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKTalentProgress Talents;

	// Enhancement levels belong to the item definition and are only applied while that item is equipped.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FName, int32> ItemEnhancementLevels;

	/** Save-authoritative equipment instances, warehouse order, and six-slot character loadouts. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKEquipmentCollectionState EquipmentCollection;

	/** Save-authoritative deterministic sequence for the permanent town meta shop. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKMetaShopState MetaShop;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSet<FName> DiscoveredCodexEntryIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSet<FName> ReadCodexEntryIds;

	/** Save-authoritative shared deck, companion roster, temporary NPC, reward and battle session state. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKCardRunState CardRun;

	/** v18+ save-authoritative pure-2D desktop Training progress. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKTrainingProgress Training;
};

USTRUCT(BlueprintType)
struct FGameXXKSaveState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SaveVersion = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameXXKRuntimeState RuntimeState;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bHasPlayerLocation = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKQuestState QuestState = EGameXXKQuestState::NotAccepted;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bFollowerJoined = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bHasQuestNpcLocation = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector QuestNpcLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerLevel = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerXP = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerGold = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSet<FName> UnlockedRegions;
};

UCLASS()
class GAMEXXK_API UGameXXKMVPRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName RegionQingshan();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName RegionHuangshan();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName RegionTanjiang();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName ItemHealingPowder();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName ItemEnhancementStone();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName ItemRefinementSand();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName ItemQingshanRouteSeal();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName ItemTrainingNormalChest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName ItemTrainingAdvancedChest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName ItemTutorialRiverMap();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName ItemIronSword();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName ItemClothArmor();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName ItemWoodenSword();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName ItemStarterClothArmor();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName ItemClothTalisman();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FName TaskQingshanMain();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FGameXXKItemDef GetItemDef(FName ItemId, bool& bFound);

	/** Shared reward/inventory predicate: known non-equipment definitions are stackable. */
	static bool IsStackableInventoryItem(FName ItemId);

	static int32 GetPlayerExperienceRequiredForNextLevel(int32 CurrentLevel);
	static void ApplyPlayerExperience(FGameXXKRuntimeState& InOutState, int32 ExperienceAmount);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static TArray<FName> GetKnownItemIds();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static TArray<FName> GetShopItemIds();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static TArray<FGameXXKCodexEntryDef> GetCodexEntryDefs();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FGameXXKCodexEntryDef GetCodexEntryDef(FName EntryId, bool& bFound);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static TArray<FGameXXKCodexEntryView> BuildCodexEntryViews(const FGameXXKRuntimeState& State, EGameXXKCodexCategory Category);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static int32 GetCodexEntryCount(EGameXXKCodexCategory Category);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static int32 GetDiscoveredCodexEntryCount(const FGameXXKRuntimeState& State, EGameXXKCodexCategory Category);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static bool HasUnreadCodexEntries(const FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool DiscoverCodexEntry(UPARAM(ref) FGameXXKRuntimeState& State, FName EntryId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool MarkCodexEntryRead(UPARAM(ref) FGameXXKRuntimeState& State, FName EntryId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FGameXXKRuntimeState CreateNewGame();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static int32 GetCurrentSaveVersion();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static TArray<FGameXXKTaskView> BuildAvailableTaskViews(const FGameXXKRuntimeState& State, EGameXXKTaskCategory Category);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static TArray<FGameXXKTaskView> BuildAcceptedTaskViews(const FGameXXKRuntimeState& State, EGameXXKTaskCategory Category);

	// Compatibility view for existing Blueprints. It follows runtime quest state;
	// the task UI uses the explicit available/accepted lists above to keep offer
	// and accepted-task panels distinct.
	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static TArray<FGameXXKTaskView> BuildTaskViews(const FGameXXKRuntimeState& State, EGameXXKTaskCategory Category);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool OpenWorldMap(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool EnterWorldRegion(UPARAM(ref) FGameXXKRuntimeState& State, FName RegionId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool AcceptTownQuest(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static bool CanEnterDungeon(const FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static void GenerateRouteMapForSeed(UPARAM(ref) FGameXXKRuntimeState& State, int32 Seed);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool EnterDungeon(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool AdvanceDungeonNode(UPARAM(ref) FGameXXKRuntimeState& State, EGameXXKNodeKind ExpectedNode);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool SelectRouteNodeById(UPARAM(ref) FGameXXKRuntimeState& State, int32 NodeId);

	/** Atomically discards the current generated-route encounter and restores its saved pre-click state. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool RetreatCurrentBattleToRoute(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ResolveBattleVictory(UPARAM(ref) FGameXXKRuntimeState& State, bool bBossBattle);

	/** Atomically skips the pending route-card reward and finishes the gated battle victory settlement. */
	static bool SkipPendingRouteRewardAndFinish(
		FGameXXKRuntimeState& State,
		FString* OutError = nullptr);

	/** Atomically commits one tiered battle reward option and the gated battle victory settlement. */
	static bool ResolvePendingBattleRewardChoiceAndFinish(
		FGameXXKRuntimeState& State,
		int32 OptionIndex,
		FName ReplacementEntryId = NAME_None,
		FString* OutError = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ExecuteBattleBasicAttack(UPARAM(ref) FGameXXKRuntimeState& State, int32 PartyIndex, int32 EnemyIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ExecuteBattleCraneWingSlash(UPARAM(ref) FGameXXKRuntimeState& State, int32 PartyIndex, int32 EnemyIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ExecuteBattleGuiyuanArt(UPARAM(ref) FGameXXKRuntimeState& State, int32 PartyIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ExecuteBattleDefend(UPARAM(ref) FGameXXKRuntimeState& State, int32 PartyIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ExecuteBattleHealingPowder(UPARAM(ref) FGameXXKRuntimeState& State, int32 PartyIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ResolveEventReward(UPARAM(ref) FGameXXKRuntimeState& State, bool bTakeGold);

	/** Resolves one visible choice from the deterministic positive event or three-relic treasure offer. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ResolveRouteEncounterChoice(UPARAM(ref) FGameXXKRuntimeState& State, int32 ChoiceIndex);

	/** Deprecated v29 Blueprint compatibility facade. Always returns false without mutation. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool AcceptRouteEventNpcSupport(UPARAM(ref) FGameXXKRuntimeState& State);

	/** Compatibility facade. The legacy bHealNow pin is preserved: true now means charm; false means 100 route money. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ResolveCampReward(UPARAM(ref) FGameXXKRuntimeState& State, bool bHealNow);

	/** Ensures the pending merchant owns one stable persisted four-card upgrade snapshot. */
	static bool EnsureRouteMerchantStock(FGameXXKRuntimeState& State, FString* OutError = nullptr);

	/** Normalizes legacy stock when needed, then builds the dedicated route-merchant read model. */
	static bool GetRouteMerchantView(
		FGameXXKRuntimeState& State,
		FGameXXKRouteMerchantView& OutView,
		FString* OutError = nullptr);

	/** Atomically pays ordinary gold and rerolls only eligible unsold card offers. */
	static bool RefreshRouteMerchant(FGameXXKRuntimeState& State, FString* OutError = nullptr);

	/** Pure carried-card upgrade preview keyed by stable OfferId. */
	static bool PreviewRouteMerchantPurchase(
		const FGameXXKRuntimeState& State,
		FName OfferId,
		FName ReplacementEntryId,
		FGameXXKRouteMerchantPurchasePreview& OutPreview,
		FString* OutError = nullptr);

	/** Atomically commits one authoritative carried-card quality upgrade. */
	static bool PurchaseRouteMerchant(
		FGameXXKRuntimeState& State,
		FName OfferId,
		FName ReplacementEntryId,
		FGameXXKRouteMerchantPurchaseResult& OutResult);

	/** Clears only legacy replacement metadata and never spends either currency. */
	static bool CancelPendingRouteMerchantPurchase(FGameXXKRuntimeState& State, FString* OutError = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ResolveMerchantRouteNode(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool FailDungeonToTown(UPARAM(ref) FGameXXKRuntimeState& State);

	/** Player-initiated route exit. It preserves the accepted quest but uses the locked abandoned settlement ratios. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool AbandonDungeonToTown(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ResolveBossClear(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static TArray<EGameXXKNodeKind> GetFixedDungeonNodes(int32 Seed);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool AddItem(UPARAM(ref) FGameXXKRuntimeState& State, FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool RemoveItem(UPARAM(ref) FGameXXKRuntimeState& State, FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static int32 GetItemCount(const FGameXXKRuntimeState& State, FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool BuyItem(UPARAM(ref) FGameXXKRuntimeState& State, FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool SellItem(UPARAM(ref) FGameXXKRuntimeState& State, FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static bool CanSellItem(const FGameXXKRuntimeState& State, FName ItemId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static int32 GetMaxItemEnhancementLevel();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static int32 GetItemEnhancementLevel(const FGameXXKRuntimeState& State, FName ItemId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static bool CanEnhanceItem(const FGameXXKRuntimeState& State, FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool EnhanceItem(UPARAM(ref) FGameXXKRuntimeState& State, FName ItemId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static bool CanDecomposeItem(const FGameXXKRuntimeState& State, FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool DecomposeItem(UPARAM(ref) FGameXXKRuntimeState& State, FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool EquipItem(UPARAM(ref) FGameXXKRuntimeState& State, FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool UnequipItem(UPARAM(ref) FGameXXKRuntimeState& State, FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool UseHealingItem(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool UseItem(UPARAM(ref) FGameXXKRuntimeState& State, FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool OpenTownPanel(UPARAM(ref) FGameXXKRuntimeState& State, EGameXXKTownPanelMode PanelMode);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool CloseTownPanel(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static void RecalculatePlayerStatsFromEquipment(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static TArray<FName> BuildTurnOrder(const FGameXXKRuntimeState& State, bool bBossBattle);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FGameXXKSaveState MakeSaveState(const FGameXXKRuntimeState& State);

	/** Compatibility-only restore facade. Returns a neutral runtime on rejection; new load/preview callers must use FGameXXKSaveMigration for an explicit report. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP", meta = (DeprecatedFunction, DeprecationMessage = "Use FGameXXKSaveMigration::TryRestoreRuntimeState so load rejection is explicit."))
	static FGameXXKRuntimeState RestoreFromSaveState(const FGameXXKSaveState& SaveState);
};
