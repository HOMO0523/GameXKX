#pragma once

#include "CoreMinimal.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentToolRules.h"
#include "GameXXKTrainingChestRules.h"
#include "GameXXKMVPRules.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Misc/Optional.h"
#include "GameXXKMVPSubsystem.generated.h"

class USaveGame;
struct FGameXXKEquipmentTransactionResult;
enum class EGameXXKDesktopItemContainer : uint8;

DECLARE_DELEGATE_RetVal_ThreeParams(
	bool,
	FGameXXKSaveSlotWriteDelegate,
	USaveGame*,
	const FString&,
	int32);
DECLARE_MULTICAST_DELEGATE(FGameXXKPersistenceBoundaryDelegate);

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

	/** Copy-safe Training read model used by the desktop workbench. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Training")
	FGameXXKTrainingProgress GetTrainingProgressCopy() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Training")
	TArray<FGameXXKTrainingStageDefinition> GetTrainingStageDefinitions() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Training")
	TArray<FGameXXKTrainingEncounterDefinition> GetTrainingEncounterSequence(FName StageId, bool bTravelMode = false) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Training")
	FText BuildTrainingStageTooltip(FName StageId) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool SelectTrainingStage(FName StageId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool StartTrainingChallenge(FName StageId);

	/** Starts a semantic story encounter through the existing full-screen card-battle runtime. */
	bool StartNarrativeEncounter(FName EncounterId, FString* OutError = nullptr);

	/** True while the Challenge owns a generated route map and the player is choosing the next node. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Training")
	bool IsTrainingChallengeRouteMapActive() const;

	/** Selects a reachable Challenge route node and opens its authored training battle. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool SelectTrainingChallengeRouteNode(int32 NodeId);

	/** True while the selected Training challenge is backed by the real card battle runtime. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Training")
	bool IsTrainingChallengeBattleActive() const;

	/** Cancels the active Training battle without rewards and returns to the pure-2D workbench. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool CancelTrainingChallengeToWorkbench();

	/** Advances one real card-battle step; terminal victory settles the route node and returns to the route map. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool AdvanceTrainingChallengeEncounter(bool& bOutStageCompleted, FGameXXKTrainingReward& OutReward);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool SetTrainingChallengeAutoBattle(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool StartTrainingTravel(FName StageId);

	/** Copy-safe runtime snapshot for the pure 2D Travel strip. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Training")
	FGameXXKTrainingTravelRuntime GetTrainingTravelRuntimeCopy() const;

	/** Advances walking, one auto attack exchange, or encounter settlement. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool AdvanceTrainingTravelStep(
		bool& bOutEncounterCompleted,
		bool& bOutStageCompleted,
		bool& bOutDefeated,
		FGameXXKTrainingReward& OutReward,
		int32 ElapsedSeconds = 1);

	/** Advances one compatibility encounter; the runner API is preferred because it also resolves Travel chests/cooldowns. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool AdvanceTrainingTravelEncounter(bool& bOutStageCompleted, FGameXXKTrainingReward& OutReward);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool SetTrainingRetryOnFailure(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool ResolveTrainingTravelFailure();

	/** Simulates closed-window Travel time into the durable pending reward ledger. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool SimulateTrainingTravelOffline(int32 ElapsedSeconds, FGameXXKTrainingOfflineReward& OutReward);

	/** Returns rewards accumulated while the Travel window was closed. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Training")
	FGameXXKTrainingOfflineReward GetPendingTrainingTravelRewardCopy() const;

	/** Claims pending Travel gold, experience and canonical chests exactly once. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool CollectTrainingTravelRewards(FGameXXKTrainingOfflineReward& OutReward);

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
	void ClearCardTooltipFixtureForTest();

	/** True only while the non-persistent card-tooltip fixture is active. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	bool IsCardTooltipFixtureActiveForTest() const;

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

	/** Non-saving real-PIE fixture: converts the first reachable Battle into Elite and seeds visible abandon rewards. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	bool ApplyRouteExitAcceptanceFixtureForTest(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	bool ClearRouteExitAcceptanceFixtureForTest(FString& OutError);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	bool IsRouteExitAcceptanceFixtureActiveForTest() const;

	/** Non-saving real-PIE fixture that opens a reachable node as YueBai Event or Camp on the active route. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	bool ApplyRouteEncounterAcceptanceFixtureForTest(bool bCamp, FString& OutError);

	/** Non-saving real-PIE fixture that opens one reachable node as the route merchant. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	bool ApplyRouteMerchantAcceptanceFixtureForTest(bool bOpenMerchant, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
	bool ClearRouteEncounterAcceptanceFixtureForTest(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool StartGame();

	/** Atomically validates and immediately saves semantic combat-guide progress. */
	bool CommitGuideProgress(const FGameXXKGuideProgress& GuideProgress, FString* OutError = nullptr);

	/** Resets only combat-guide preference/session/completions, preserving tasks and route rewards. */
	bool ResetCombatGuideProgress(FString* OutError = nullptr);

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

	/**
	 * Fires before new-game, save, or load crosses a persistence boundary.
	 * Transient UI transactions must roll back before the authoritative state is
	 * serialized or replaced.
	 */
	FGameXXKPersistenceBoundaryDelegate& OnPersistenceBoundary()
	{
		return PersistenceBoundaryDelegate;
	}

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

	/** Ensures save-authoritative desktop backpack/warehouse slots include all current possessions. */
	bool NormalizeDesktopInventoryState();

	/** Grants the unique inspectable tutorial map and saves its physical/pending placement atomically. */
	bool GrantTutorialRiverMap(FString* OutError = nullptr);
	bool OwnsTutorialRiverMap() const;

	/** Read-only permanent talent graph and derived shared projection. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Talents")
	TArray<FGameXXKTalentNodeView> GetTalentNodeViews() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Talents")
	FGameXXKTalentProjection GetTalentProjection() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Talents")
	bool PurchaseTalentNode(FName NodeId, FGameXXKTalentPurchaseResult& OutResult);

	/** Atomic whole-stack/instance move between physical desktop cells. */
	bool MoveDesktopInventoryEntry(
		EGameXXKDesktopItemContainer FromContainer,
		int32 FromSlotIndex,
		EGameXXKDesktopItemContainer ToContainer,
		int32 ToSlotIndex,
		FString* OutError = nullptr);

	/** UI read model: projects one valid permanent equipment owner through the authoritative stat rules. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Equipment")
	bool GetEquipmentLoadoutSnapshot(FName CharacterId, FGameXXKEquipmentLoadoutSnapshot& OutSnapshot) const;

	/** UI read model: compares one item against a valid permanent character without mutating runtime state. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Equipment")
	bool GetEquipmentTooltipSnapshot(FName InstanceId, FName CompareCharacterId, FGameXXKEquipmentTooltipSnapshot& OutSnapshot) const;

	/** Applies the single deterministic warehouse sort used by the desktop workbench. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Equipment")
	bool SortEquipmentWarehouse();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Equipment")
	bool EquipEquipmentInstance(FName CharacterId, EGameXXKEquipmentSlot Slot, FName InstanceId, FGameXXKEquipmentTransactionResult& OutResult);

	/** Equips the unequipped instance in one authoritative desktop cell and returns any displaced instance there. */
	bool EquipEquipmentFromDesktopCell(
		FName CharacterId,
		EGameXXKEquipmentSlot Slot,
		EGameXXKDesktopItemContainer SourceContainer,
		int32 SourceSlotIndex,
		FName ExpectedInstanceId,
		FGameXXKEquipmentTransactionResult& OutResult);

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

	const FGameXXKToolProgress& GetToolProgress() const { return RuntimeState.ToolProgress; }
	bool SetToolSelectedCraftingLevel(int32 Level);
	bool SetToolAutoFillIncludesWarehouse(bool bIncludeWarehouse);
	bool ExecuteToolDismantle(const TArray<FGameXXKToolInputRef>& Inputs, bool bConfirmed, FGameXXKEquipmentTransactionResult& OutResult);
	bool ExecuteToolCombine(EGameXXKToolCombineKind Kind, const TArray<FGameXXKToolInputRef>& Inputs, FGameXXKEquipmentTransactionResult& OutResult);
	bool BuildToolCombineAutoFill(EGameXXKToolCombineKind Kind, bool bIncludeWarehouse, TArray<FGameXXKToolInputRef>& OutInputs, FString* OutError = nullptr) const;
	bool ExecuteToolEnhance(const FGameXXKToolInputRef& Input, FGameXXKEquipmentTransactionResult& OutResult);
	bool ExecuteToolBeginReforge(const FGameXXKToolInputRef& Input, int32 AffixIndex, FGameXXKEquipmentTransactionResult& OutResult);
	bool ExecuteToolResolveReforge(bool bAccept, FGameXXKEquipmentTransactionResult& OutResult);
	bool ExecuteToolSocket(const FGameXXKSocketGemRequest& Request, FGameXXKEquipmentTransactionResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "GameXXK|Training")
	int32 GetTrainingChestCount(EGameXXKTrainingRewardTier Tier) const;
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool OpenOneTrainingChest(EGameXXKTrainingRewardTier Tier, FGameXXKTrainingChestOpenResult& OutResult);
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool OpenAllTrainingChests(EGameXXKTrainingRewardTier Tier, FGameXXKTrainingChestOpenResult& OutResult);

#if WITH_DEV_AUTOMATION_TESTS
	/** Per-subsystem deterministic disk-write seam used only by automation rollback tests. */
	void SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate InDelegate);
	void ResetSaveSlotWriteDelegateForTest();
	/** Final pre-commit gate used only to prove new-game candidate rollback. */
	void SetStartNewGameCommitGateForTest(TFunction<bool()> InGate);
	void ResetStartNewGameCommitGateForTest();
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

	/** Direct HUD-map launch needs the same starter party as Start/New Game, without changing 3D-town direct-map semantics. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool EnsureDesktopTrainingRuntimeForDirectMap();

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

	/** Player-confirmed rollback of only the currently selected generated-route encounter. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool RetreatCurrentBattleToRoute();

	/** Session-only preference; retained across monster encounters and never serialized. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Auto")
	bool IsBattleAutoPlayEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Auto")
	bool SetBattleAutoPlayEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ResolveBattleVictory(bool bBossBattle);

	UFUNCTION(BlueprintPure, Category = "GameXXK|Training")
	bool HasPendingTrainingSettlement() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Training")
	FGameXXKTrainingSettlementReceipt GetPendingTrainingSettlementCopy() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Training")
	bool ConfirmTrainingSettlement(FGuid ReceiptId);

	static FString GetTrainingCheckpointSlotName();

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

	/** Returns an unresolved event/camp overlay to the route map without mutating its pending offer. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ReturnPendingRouteChoiceToMap();

	/** Deprecated v29 Blueprint compatibility facade. Always returns false without mutation. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool AcceptRouteEventNpcSupport();

	/** Compatibility facade. The legacy bHealNow pin is preserved: true now means charm; false means 100 route money. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool ResolveCampReward(bool bHealNow);

	bool EnsureRouteMerchantStock(FString* OutError = nullptr);

	bool GetRouteMerchantView(FGameXXKRouteMerchantView& OutView, FString* OutError = nullptr);

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

	/** Side-effect-free exact preview for the route-map abandon confirmation. */
	bool PreviewAbandonedRouteSettlement(
		FGameXXKRouteSettlementReceipt& OutReceipt,
		FString* OutError = nullptr) const;

	/** Atomically settles earned route progress and returns to the same-map pure-2D idle Workbench. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool SettleAndExitActiveRoute(
		FGameXXKRouteSettlementReceipt& OutReceipt,
		FString& OutError);

	/** Compatibility facade for callers that still use the retired abandon verb. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	bool AbandonDungeonToTown();

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

	/** Returns raw authoritative 1P / 2P / 3P order; invalid raw state is logged instead of silently projected. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Party")
	FGameXXKOrderedPartyFormation GetOrderedPartyFormation() const;

	/** Atomically validates and commits a 1P / 2P / 3P formation only at the unlocked town workbench. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Party")
	bool SetOrderedPartyFormation(const FGameXXKOrderedPartyFormation& Formation, FString& OutError);

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

	/**
	 * Replaces only the selected roster member with the saved candidate. A non-None ActiveAfter
	 * becomes the first ordered companion even when the dismissed member was not deployed;
	 * NAME_None leaves the current ordered formation as active-companion authority.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool ResolvePendingPermanentCompanionReplacement(FName DismissedInstanceId, FName ActivePermanentCompanionInstanceIdAfterReplacement = NAME_None);

	/** Authoritative C++ transaction used by the new equipment-aware UI. */
	bool ResolvePendingPermanentCompanionReplacement(
		FName DismissedInstanceId,
		FName ActivePermanentCompanionInstanceIdAfterReplacement,
		FGameXXKEquipmentTransactionResult& OutResult);

	/**
	 * Dismisses one permanent companion without a replacement (the 遣散 action).
	 * Equipment returns to the warehouse; if the companion is in OrderedFormation,
	 * that exact slot receives a stable owned replacement before compatibility is reprojected.
	 * The roster must keep at least two companions.
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

	/** Legacy town action; v24 exact formations containing a companion reject instead of creating an empty compatibility slot. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool ClearActivePermanentCompanion();

	/** Persists exactly five unlocked personal cards for one permanent companion. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool SetPermanentCompanionCardLoadout(FName InstanceId, const TArray<FName>& SelectedCardIds);

	/** Persists exactly eight unlocked hero cards. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Companion")
	bool SetHeroCardLoadout(const TArray<FName>& SelectedCardIds);

	/** Replaces the NPC slot with one of the six permanently owned named NPCs. */
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
	bool PersistTrainingCheckpoint(const FGameXXKRuntimeState& Candidate);
	bool IsTrainingCheckpointWorld() const;
	bool bRecoveringTrainingCheckpoint = false;
	bool WriteSaveGameToSlot(USaveGame* SaveGame, const FString& SlotName, int32 UserIndex);
	bool BuildTrainingTravelRuntimeForState(
		const FGameXXKRuntimeState& State,
		FGameXXKTrainingTravelRuntime& OutRuntime) const;
	bool PrepareFreshTrainingTravelRuntime(
		FGameXXKRuntimeState& InOutState,
		FGameXXKTrainingTravelRuntime& OutRuntime) const;
	bool RebuildTrainingTravelRuntime();
	bool ApplyTrainingOfflineRewardToRuntime(FGameXXKRuntimeState& State, const FGameXXKTrainingOfflineReward& Reward) const;
	int64 GetCurrentTravelUnixSeconds() const;
	bool ApplyOfflineTravelSinceLastUpdate(
		FGameXXKRuntimeState& State,
		FGameXXKTrainingTravelRuntime& OutRuntime,
		int64 NowUnixSeconds) const;
	void SetSaveMigrationFailure();
	void SetSaveRollbackFailure();
	bool FinishTrainingChallengeBossIfNeeded(
		FGameXXKRuntimeState& InOutState,
		bool& bOutStageCompleted,
		FGameXXKTrainingReward& OutReward);
	bool ApplyTrainingChallengeRewardOption(
		FGameXXKRuntimeState& InOutState,
		int32 OptionIndex,
		const FName ReplacementEntryId,
		FString* OutError);
	bool SettleTrainingChallengeBossNode(
		FGameXXKRuntimeState& InOutState,
		bool& bOutStageCompleted,
		FGameXXKTrainingReward& OutReward,
		FString* OutError);

	/** Never serialized: this only lets visual PIE probes render a safe copied battle state. */
	TOptional<FGameXXKRuntimeState> BattleHudFixtureView;

	/** Backup for the development card-tooltip fixture; restoring it never writes a save. */
	TOptional<FGameXXKRuntimeState> CardTooltipFixtureBackup;

	/** Never serialized: exact source state restored after each mutable target-outcome parity fixture. */
	TOptional<FGameXXKRuntimeState> TargetOutcomeFixtureBackup;

	TOptional<FGameXXKRuntimeState> RouteExitAcceptanceFixtureBackup;
	TOptional<FGameXXKRuntimeState> RouteEncounterAcceptanceFixtureBackup;

	UPROPERTY(Transient)
	FText LastSaveLoadError;

	/** Never serialized: a fresh application process always starts with auto play disabled. */
	UPROPERTY(Transient)
	bool bBattleAutoPlayEnabled = false;

	FGameXXKPersistenceBoundaryDelegate PersistenceBoundaryDelegate;

#if WITH_DEV_AUTOMATION_TESTS
	FGameXXKSaveSlotWriteDelegate SaveSlotWriteDelegateForTest;
	TFunction<bool()> StartNewGameCommitGateForTest;
#endif

	UPROPERTY()
	FGameXXKRuntimeState RuntimeState;

	/** Rebuilt from save-authoritative Training progress; never serialized itself. */
	FGameXXKTrainingTravelRuntime TrainingTravelRuntime;
};
