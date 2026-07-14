#pragma once

#include "CoreMinimal.h"
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
	Accessory
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
	int32 PlayerGold = 50;

	// Used exclusively by equipment enhancement. New games and migrated saves begin with ten.
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

	// Enhancement levels belong to the item definition and are only applied while that item is equipped.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FName, int32> ItemEnhancementLevels;
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

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static TArray<FName> GetKnownItemIds();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static TArray<FName> GetShopItemIds();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FGameXXKRuntimeState CreateNewGame();

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

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ResolveBattleVictory(UPARAM(ref) FGameXXKRuntimeState& State, bool bBossBattle);

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

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ResolveCampReward(UPARAM(ref) FGameXXKRuntimeState& State, bool bHealNow);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ResolveMerchantRouteNode(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool FailDungeonToTown(UPARAM(ref) FGameXXKRuntimeState& State);

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
	static int32 GetMaxItemEnhancementLevel();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static int32 GetItemEnhancementLevel(const FGameXXKRuntimeState& State, FName ItemId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static bool CanEnhanceItem(const FGameXXKRuntimeState& State, FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool EnhanceItem(UPARAM(ref) FGameXXKRuntimeState& State, FName ItemId);

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

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FGameXXKRuntimeState RestoreFromSaveState(const FGameXXKSaveState& SaveState);
};
