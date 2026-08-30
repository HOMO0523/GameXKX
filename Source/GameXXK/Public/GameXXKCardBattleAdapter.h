#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"

/**
 * Bridges the card-combat authority to the existing MVP game state.  Every public operation accepts
 * stable card instance / unit IDs; legacy party and enemy arrays are UI and scene projections only.
 */
class GAMEXXK_API FGameXXKCardBattleAdapter final
{
public:
	/** Process-independent seed for deterministic random enemy target selection. */
	static uint32 MakeStableEnemyIntentTargetSeed(FName SourceUnitId, int32 RoundNumber);

	/** Mixes a route/run seed with a battle node id. Uses 64-bit multiplication then int32 truncation to avoid signed-overflow UB while preserving the previous wrapped result for every node id. */
	static int32 MixBattleSeed(int32 BaseSeed, int32 NodeId);

	/** Migrates or initializes persistent hero loadout and card-run fields without resetting existing legacy state. */
	static bool EnsureCardRunInitialized(FGameXXKRuntimeState& InOutState, FString* OutError = nullptr);

	/** Valid only outside a locked route; hero owns exactly eight selected cards from its unlocked pool. */
	static bool SetHeroSelectedCards(FGameXXKRuntimeState& InOutState, const TArray<FName>& SelectedCardIds, FString* OutError = nullptr);

	/** Selects one of the six owned NPCs outside a locked route; an empty card list restores its persisted loadout. */
	static bool SetQuestNpcForCurrentRun(FGameXXKRuntimeState& InOutState, FName QuestNpcId, const TArray<FName>& SelectedCardIds, FString* OutError = nullptr);

	/** Builds a fresh, saved five-card opening hand for one battle from the current locked route party and cards. */
	static bool BeginCardBattle(
		FGameXXKRuntimeState& InOutState,
		EGameXXKNodeKind NodeKind,
		EGameXXKCardTerrain Terrain,
		int32 InitialRandomSeed,
		FString* OutError = nullptr);

	/** Copies HP/MP/attributes/defeat state from card authority to existing scene/widget-facing battle arrays. */
	static bool SyncCardBattleToLegacyProjection(FGameXXKRuntimeState& InOutState, FString* OutError = nullptr);

	static bool BuildCardPlayPreview(
		const FGameXXKRuntimeState& State,
		FName CardInstanceId,
		FGameXXKCardPlayPreview& OutPreview,
		FString* OutError = nullptr);

	/** Revalidates the selected stable target inside the card rules and projects only after a committed card resolution. */
	static bool ResolveCardPlay(
		FGameXXKRuntimeState& InOutState,
		FName CardInstanceId,
		FName SelectedTargetUnitId,
		FGameXXKCardPlayResult& OutResult,
		FString* OutError = nullptr);

	/** Resolves the active insight offer by stable instance ID, preserving the supplied remaining top-order. */
	static bool SubmitInsightChoice(
		FGameXXKRuntimeState& InOutState,
		FName PickedInstanceId,
		const TArray<FName>& ReorderedRemainingInstanceIds,
		FString* OutError = nullptr,
		TArray<FGameXXKCardPlayResult>* OutResumedResults = nullptr);

	/** Moves one real unfinished protagonist task card from draw/discard to hand and resumes any paused replay. */
	static bool SubmitHeroTaskSearchChoice(
		FGameXXKRuntimeState& InOutState,
		FName PickedInstanceId,
		TArray<FGameXXKCardPlayResult>& OutResumedResults,
		FString* OutError = nullptr);

	/** Resolves the active forced-discard choice by stable runtime-card IDs. */
	static bool SubmitForcedDiscard(
		FGameXXKRuntimeState& InOutState,
		const TArray<FName>& DiscardedInstanceIds,
		FString* OutError = nullptr,
		TArray<FGameXXKCardPlayResult>* OutResumedResults = nullptr);

	/** Cancels the active insight without changing the inspected draw-pile top. */
	static bool CancelInsight(
		FGameXXKRuntimeState& InOutState,
		FString* OutError = nullptr,
		TArray<FGameXXKCardPlayResult>* OutResumedResults = nullptr);

	/** Resumes a saved automatic replay queue and commits its runtime, relic consumption, and legacy projection atomically. */
	static bool ResumeAutomaticResolutionQueue(
		FGameXXKRuntimeState& InOutState,
		TArray<FGameXXKCardPlayResult>& OutResumedResults,
		FString* OutError = nullptr);

	/** Ends the player phase, records deterministic enemy intents, and leaves the card runtime in Enemy or terminal phase. */
	static bool EndPlayerCardPhase(
		FGameXXKRuntimeState& InOutState,
		TArray<FGameXXKCardDamageResult>& OutDamageResults,
		FString* OutError = nullptr);

	/** Resolves at most one saved enemy intent, preserving Enemy phase until explicit completion or a terminal result. */
	static bool ResolveNextEnemyIntent(
		FGameXXKRuntimeState& InOutState,
		FGameXXKCardEnemyIntent& OutResolvedIntent,
		TArray<FGameXXKCardDamageResult>& OutDamageResults,
		bool& bOutIntentsFinished,
		FString* OutError = nullptr);

	/** Explicit board-recovery operation: consume exactly the current saved intent without resolving its effects. */
	static bool SkipCurrentEnemyIntent(
		FGameXXKRuntimeState& InOutState,
		FString* OutError = nullptr);

	/** Begins the next player round only after saved enemy intents have been consumed, then clears those intents. */
	static bool CompleteEnemyCardPhase(
		FGameXXKRuntimeState& InOutState,
		TArray<FGameXXKCardDamageResult>& OutDamageResults,
		FString* OutError = nullptr);

	/** Resolves every currently pending enemy intent once, then begins the next player phase unless terminal. */
	static bool ResolveEnemyPhase(
		FGameXXKRuntimeState& InOutState,
		TArray<FGameXXKCardDamageResult>& OutDamageResults,
		FString* OutError = nullptr);

	/** Produces the tiered three-choice battle reward (Battle: relic-heavy; Elite: attribute bonus; Boss: boss card). */
	static bool CreateTieredBattleRewardOffer(
		FGameXXKRuntimeState& InOutState,
		EGameXXKNodeKind NodeKind,
		int32 SourceNodeId,
		int32 ChoiceSeed,
		FString* OutError = nullptr);

	/** Commits a boss-exclusive reward card into one of the three boss card slots and the current hand. */
	static bool CommitBossCardReward(
		FGameXXKRuntimeState& InOutState,
		FName RewardCardId,
		FString* OutError = nullptr);

	/** Current quality of a configured hero/companion card, honoring earned upgrades. */
	static EGameXXKCardQuality GetConfiguredCardQuality(
		const FGameXXKCardRunState& CardRun,
		FName CardId);

	/** The next quality step (Common->Rare->Epic); Epic stays. */
	static EGameXXKCardQuality GetNextCardQuality(EGameXXKCardQuality Quality);

	/** Purely previews one saved boss-card reward candidate against the free boss-slot availability. */
	static bool PreviewPendingRouteReward(
		const FGameXXKRuntimeState& State,
		FName RewardCardId,
		FName ReplacementEntryId,
		FGameXXKRouteCardAcquisitionPreview& OutPreview,
		FString* OutError = nullptr);

	static bool SkipPendingRouteReward(FGameXXKRuntimeState& InOutState, FString* OutError = nullptr);

	/** Creates a deterministic saved route-event identity; named task NPCs can later be accepted as temporary support. */
	static bool CreateRouteEventOffer(
		FGameXXKRuntimeState& InOutState,
		int32 SourceNodeId,
		int32 ChoiceSeed,
		FName& OutEventNpcId,
		FString* OutError = nullptr);

	/** Clears only the resolved/aborted battle session; route reward cards and temporary NPC selection remain for later nodes. */
	static void ClearActiveCardBattle(FGameXXKRuntimeState& InOutState);

	/** Clears battle, reward, merchant, relic, and pending-event route state while preserving the authoritative party and every owned loadout/progression. */
	static void ClearRouteLocalCardState(FGameXXKRuntimeState& InOutState);

	static bool IsCardBattleTerminal(const FGameXXKRuntimeState& State);
};
