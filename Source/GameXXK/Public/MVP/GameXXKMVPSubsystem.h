#pragma once

#include "CoreMinimal.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Misc/Optional.h"
#include "GameXXKMVPSubsystem.generated.h"

class USaveGame;
struct FGameXXKEquipmentTransactionResult;

DECLARE_DELEGATE_RetVal_ThreeParams(
	bool,
	FGameXXKSaveSlotWriteDelegate,
	USaveGame*,
	const FString&,
	int32);

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

	/** Development-only visual fixture. This is a non-saving view over a copied active card battle. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	bool ApplyBattleHudFixtureForTest(FString& OutError);

	/**
	 * Development-only pilot-comparison fixture: three hero-role party units and three rooster
	 * enemies, used to visually compare animation atlas resolutions. Same non-saving overlay
	 * contract as ApplyBattleHudFixtureForTest.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	bool ApplyPilotComparisonFixtureForTest(FString& OutError);

	/** Development-only non-saving tooltip fixture: replaces the first visible hand card with CardId. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	bool ApplyCardTooltipFixtureForTest(FName CardId, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	void ClearBattleHudFixtureForTest();

	/** True only while the non-persistent development Battle HUD fixture is the visible runtime view. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	bool IsBattleHudFixtureActiveForTest() const;

	/** Development-only mutable battle fixture used to certify outcome-preview parity in real PIE. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	bool ApplyTargetOutcomeFixtureForTest(FName ScenarioId, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	bool ClearTargetOutcomeFixtureForTest(FString& OutError);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	bool IsTargetOutcomeFixtureActiveForTest() const;

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

	/** Empty after a successful load; otherwise contains the stable player-facing load/migration error. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	FText GetLastSaveLoadError() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|MetaShop")
	TArray<FGameXXKMetaShopProductDefinition> GetMetaShopProducts() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|MetaShop")
	bool PreviewMetaShopPurchase(
		EGameXXKMetaShopProductId ProductId,
		FGameXXKMetaShopPurchasePreview& OutPreview) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MetaShop")
	bool PurchaseMetaShopProduct(
		EGameXXKMetaShopProductId ProductId,
		FGameXXKMetaShopPurchaseResult& OutResult);

	/** UI read model: returns the save-authoritative warehouse order without mutating runtime state. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Equipment")
	bool GetEquipmentWarehouseSnapshot(TArray<FName>& OutOrderedInstanceIds) const;

	/** UI read model: projects one valid permanent equipment owner through the authoritative stat rules. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Equipment")
	bool GetEquipmentLoadoutSnapshot(FName CharacterId, FGameXXKEquipmentLoadoutSnapshot& OutSnapshot) const;

	/** UI read model: compares one item against a valid permanent character without mutating runtime state. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Equipment")
	bool GetEquipmentTooltipSnapshot(FName InstanceId, FName CompareCharacterId, FGameXXKEquipmentTooltipSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Equipment")
	bool EquipEquipmentInstance(FName CharacterId, EGameXXKEquipmentSlot Slot, FName InstanceId, FGameXXKEquipmentTransactionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Equipment")
	bool UnequipEquipmentSlot(FName CharacterId, EGameXXKEquipmentSlot Slot, FGameXXKEquipmentTransactionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Equipment")
	bool EnhanceEquipmentInstance(FName InstanceId, FGameXXKEquipmentTransactionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Equipment")
	bool BeginEquipmentReforge(FName InstanceId, int32 AffixIndex, FGameXXKEquipmentTransactionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Equipment")
	bool ResolveEquipmentReforge(bool bAccept, FGameXXKEquipmentTransactionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Equipment")
	bool DismantleEquipmentInstances(const TArray<FName>& InstanceIds, bool bConfirmedProtected, FGameXXKEquipmentTransactionResult& OutResult);

#if WITH_DEV_AUTOMATION_TESTS
	/** Per-subsystem deterministic disk-write seam used only by automation rollback tests. */
	void SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate InDelegate);
	void ResetSaveSlotWriteDelegateForTest();
#endif

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

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool EnsureQingshanTownRuntimeForDirectMap();

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

	/** Commits one tiered battle reward option through the rules, then the victory gate advances the route. */
	bool ResolvePendingBattleRewardChoiceAndFinish(
		int32 OptionIndex,
		FName ReplacementEntryId = NAME_None,
		FString* OutError = nullptr);

	bool SkipPendingRouteRewardAndFinish(FString* OutError = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ExecuteBattleBasicAttack(int32 PartyIndex, int32 EnemyIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ExecuteBattleCraneWingSlash(int32 PartyIndex, int32 EnemyIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ExecuteBattleGuiyuanArt(int32 PartyIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ExecuteBattleDefend(int32 PartyIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ExecuteBattleHealingPowder(int32 PartyIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ResolveEventReward(bool bTakeGold);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ResolveRouteEncounterChoice(int32 ChoiceIndex);

	/** Accepts the named task NPC offered by the active route event as this route's temporary third party member. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool AcceptRouteEventNpcSupport();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ResolveCampReward(bool bHealNow);

	bool EnsureRouteMerchantStock(FString* OutError = nullptr);

	bool GetRouteMerchantView(FGameXXKRouteMerchantView& OutView, FString* OutError = nullptr) const;

	bool RefreshRouteMerchant(FString* OutError = nullptr);

	bool PreviewRouteMerchantPurchase(
		FName OfferId,
		FName ReplacementEntryId,
		FGameXXKRouteMerchantPurchasePreview& OutPreview,
		FString* OutError = nullptr) const;

	bool PurchaseRouteMerchant(
		FName OfferId,
		FName ReplacementEntryId,
		FGameXXKRouteMerchantPurchaseResult& OutResult);

	bool CancelPendingRouteMerchantPurchase(FString* OutError = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ResolveMerchantRouteNode();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool FailDungeonToTown();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool BuyItem(FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool SellItem(FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	bool CanSellItem(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	int32 GetItemEnhancementLevel(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	bool CanEnhanceItem(FName ItemId) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool EnhanceItem(FName ItemId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	bool CanDecomposeItem(FName ItemId) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool DecomposeItem(FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool UseHealingItem();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool UseItem(FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool EquipItem(FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool UnequipItem(FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool OpenTownPanel(EGameXXKTownPanelMode PanelMode);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool CloseTownPanel();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	int32 GetItemCount(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Codex")
	TArray<FGameXXKCodexEntryView> GetCodexEntryViews(EGameXXKCodexCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Codex")
	int32 GetCodexEntryCount(EGameXXKCodexCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Codex")
	int32 GetDiscoveredCodexEntryCount(EGameXXKCodexCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Codex")
	bool HasUnreadCodexEntries() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Codex")
	bool MarkCodexEntryRead(FName EntryId);

	/** Copy-safe permanent-roster views for companion/deck UI; the save-authoritative state remains private. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Companion")
	TArray<FGameXXKPermanentCompanion> GetPermanentCompanionViews() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Companion")
	bool TryGetPermanentCompanionView(FName InstanceId, FGameXXKPermanentCompanion& OutCompanion) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Companion")
	int32 GetPermanentCompanionRosterCapacity() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Companion")
	bool IsCompanionLoadoutMutationLocked() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Companion")
	TArray<FName> GetHeroCardLoadout() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Companion")
	FGameXXKQuestNpcCardSelection GetQuestNpcCardLoadout() const;

	/** Initializes/migrates the persistent card pool when the town companion backpack is opened. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool PrepareCompanionRosterForTown();

	/** Town-only deterministic test/development seam. A full roster produces the canonical pending-replacement result. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool RecruitPermanentCompanionFromSeed(int32 RecruitOrderSeed, FGameXXKCompanionRecruitResult& OutResult);

	/** Town-only player-facing recruit action backed by the persistent sequence in the save state. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool StartRandomPermanentCompanionRecruitment(FGameXXKCompanionRecruitResult& OutResult);

	/** Reads, but never mutates, the saved full-roster replacement candidate. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Companion")
	bool TryGetPendingPermanentCompanionRecruitment(FGameXXKPermanentCompanion& OutCandidate) const;

	/** Replaces only the player-selected existing companion with the saved candidate. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool ResolvePendingPermanentCompanionReplacement(FName DismissedInstanceId, FName ActivePermanentCompanionInstanceIdAfterReplacement = NAME_None);

	/** Authoritative C++ transaction used by the new equipment-aware UI. */
	bool ResolvePendingPermanentCompanionReplacement(
		FName DismissedInstanceId,
		FName ActivePermanentCompanionInstanceIdAfterReplacement,
		FGameXXKEquipmentTransactionResult& OutResult);

	/**
	 * Dismisses one permanent companion without a replacement (the 遣散 action).
	 * Equipment returns to the warehouse, the active party slot clears, and the
	 * roster must keep at least one companion.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool DismissPermanentCompanion(FName InstanceId);

	/** Explicitly abandons the saved full-roster candidate without consuming a new ticket. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool DiscardPendingPermanentCompanionRecruitment();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Companion")
	int32 GetPermanentCompanionSigilCount() const;

	/** Selects the optional single permanent partner for a future route. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool SetActivePermanentCompanion(FName InstanceId);

	/** Town-only player action for leaving the optional permanent-partner slot empty. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool ClearActivePermanentCompanion();

	/** Persists exactly five unlocked personal cards for one permanent companion. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool SetPermanentCompanionCardLoadout(FName InstanceId, const TArray<FName>& SelectedCardIds);

	/** Persists exactly eight unlocked hero cards. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool SetHeroCardLoadout(const TArray<FName>& SelectedCardIds);

	/** Town F-interaction action: replaces the optional temporary named NPC and uses its fixed default cards. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool SelectTownQuestNpcForParty(FName QuestNpcId);

	/** Compatibility-only public entry point. Task NPC cards are route-owned, fixed, and read-only; always rejects edits. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool SetTemporaryQuestNpcCardLoadout(FName QuestNpcId, const TArray<FName>& SelectedCardIds);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool AwardPermanentCompanionExperience(FName InstanceId, int32 ExperienceAmount);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool PromotePermanentCompanionStar(FName InstanceId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	TArray<FName> BuildTurnOrder(bool bBossBattle) const;

private:
	bool WriteSaveGameToSlot(USaveGame* SaveGame, const FString& SlotName, int32 UserIndex);
	void SetSaveMigrationFailure();
	void SetSaveRollbackFailure();

	/** Never serialized: this only lets visual PIE probes render a safe copied battle state. */
	TOptional<FGameXXKRuntimeState> BattleHudFixtureView;

	/** Never serialized: exact source state restored after each mutable target-outcome parity fixture. */
	TOptional<FGameXXKRuntimeState> TargetOutcomeFixtureBackup;

	UPROPERTY(Transient)
	FText LastSaveLoadError;

#if WITH_DEV_AUTOMATION_TESTS
	FGameXXKSaveSlotWriteDelegate SaveSlotWriteDelegateForTest;
#endif

	UPROPERTY()
	FGameXXKRuntimeState RuntimeState;
};
