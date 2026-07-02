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
	Battle
};

UENUM(BlueprintType)
enum class EGameXXKQuestState : uint8
{
	NotAccepted,
	Accepted,
	Completed
};

UENUM(BlueprintType)
enum class EGameXXKNodeKind : uint8
{
	Start,
	Battle,
	Event,
	Camp,
	Boss
};

UENUM(BlueprintType)
enum class EGameXXKItemKind : uint8
{
	Consumable,
	Weapon,
	Armor
};

USTRUCT(BlueprintType)
struct FGameXXKItemDef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Id;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKItemKind Kind = EGameXXKItemKind::Consumable;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 BuyPrice = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SellPrice = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 HealAmount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 AttackBonus = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 DefenseBonus = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MaxHPBonus = 0;
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
struct FGameXXKRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKScreen Screen = EGameXXKScreen::MainMenu;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKQuestState QuestState = EGameXXKQuestState::NotAccepted;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName CurrentRegion;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerLevel = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerXP = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerGold = 30;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerHP = 100;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerMaxHP = 100;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerAttack = 12;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerDefense = 5;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PlayerSpeed = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bFollowerJoined = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDungeonActive = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 DungeonNodeIndex = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName EquippedWeapon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName EquippedArmor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSet<FName> UnlockedRegions;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FName, int32> Inventory;
};

USTRUCT(BlueprintType)
struct FGameXXKSaveState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKQuestState QuestState = EGameXXKQuestState::NotAccepted;

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
	static FGameXXKRuntimeState CreateNewGame();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool OpenWorldMap(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool EnterWorldRegion(UPARAM(ref) FGameXXKRuntimeState& State, FName RegionId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool AcceptTownQuest(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static bool CanEnterDungeon(const FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool EnterDungeon(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool AdvanceDungeonNode(UPARAM(ref) FGameXXKRuntimeState& State, EGameXXKNodeKind ExpectedNode);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ResolveBattleVictory(UPARAM(ref) FGameXXKRuntimeState& State, bool bBossBattle);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ResolveEventReward(UPARAM(ref) FGameXXKRuntimeState& State, bool bTakeGold);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool ResolveCampReward(UPARAM(ref) FGameXXKRuntimeState& State, bool bHealNow);

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

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool EquipItem(UPARAM(ref) FGameXXKRuntimeState& State, FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static bool UseHealingItem(UPARAM(ref) FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static TArray<FName> BuildTurnOrder(const FGameXXKRuntimeState& State, bool bBossBattle);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FGameXXKSaveState MakeSaveState(const FGameXXKRuntimeState& State);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FGameXXKRuntimeState RestoreFromSaveState(const FGameXXKSaveState& SaveState);
};
