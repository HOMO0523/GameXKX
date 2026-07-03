#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameXXKMVPSubsystem.generated.h"

UCLASS(BlueprintType)
class GAMEXXK_API UGameXXKMVPSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UGameXXKMVPSubsystem();

	const FGameXXKRuntimeState& GetRuntimeState() const;
	FGameXXKRuntimeState& GetMutableRuntimeState();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	FGameXXKRuntimeState GetRuntimeStateCopy() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool StartGame();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool StartNewGame();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool StartGameFromSlot(FString SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ContinueGameFromSlot(FString SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool SaveCurrentGame(FString SlotName = TEXT(""), int32 UserIndex = 0);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	bool DoesSaveGameExist(FString SlotName = TEXT(""), int32 UserIndex = 0) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool DeleteSaveGame(FString SlotName = TEXT(""), int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool LoadGameFromSlot(FString SlotName = TEXT(""), int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool LoadOrCreateGame(FString SlotName = TEXT(""), int32 UserIndex = 0);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FString GetDefaultSaveSlotName();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static int32 GetManualSaveSlotCount();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	static FString GetManualSaveSlotName(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool OpenWorldMap();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool SelectWorldRegion(FName RegionId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	bool IsRegionUnlocked(FName RegionId) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool AcceptQuest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	void RecordQuestNpcLocation(FVector Location);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	void RecordPlayerLocation(FVector Location);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	bool CanEnterDungeon() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool OpenDungeonFromTownExit();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool SelectDungeonNode(EGameXXKNodeKind ExpectedNode);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool SelectRouteNodeById(int32 NodeId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ResolveBattleVictory(bool bBossBattle);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ResolveEventReward(bool bTakeGold);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ResolveCampReward(bool bHealNow);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool FailDungeonToTown();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool BuyItem(FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool SellItem(FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool UseHealingItem();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool EquipItem(FName ItemId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	int32 GetItemCount(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	TArray<FName> BuildTurnOrder(bool bBossBattle) const;

private:
	UPROPERTY()
	FGameXXKRuntimeState RuntimeState;
};
