#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

/** Pure, deterministic card-zone rules. This layer deliberately does not resolve card effects or turns. */
namespace GameXXKCardRules
{
	/** Fixed additive direct-hit bonus while the resolved target has at least one Mark stack. */
	inline constexpr int32 MarkDirectDamageBonusPercent = 15;

	/** Validates unique materialized IDs, deterministically shuffles, and draws up to the normal hand limit. */
	GAMEXXK_API bool InitializeBattleDeck(
		FGameXXKBattleDeckState& InOutDeck,
		const TArray<FGameXXKCardInstance>& Instances,
		int32 InitialRandomSeed,
		FString* OutError = nullptr);

	/**
	 * Draws up to Count cards into a hard twenty-card battle hand. HandLimit remains only the
	 * round-refill target. Once the hard capacity is reached, undrawn cards stay in play and the
	 * remaining draw pile is deterministically shuffled. A positive RequiredDiscardCount opens
	 * an exact ForcedDiscard choice after the draw. The battle instance ledger remains unchanged;
	 * route-deck authority is outside this battle-only API and is never consumed by a draw.
	 */
	GAMEXXK_API bool DrawCards(
		FGameXXKBattleDeckState& InOutDeck,
		int32 Count,
		int32 RequiredDiscardCount,
		FString* OutError = nullptr);

	/**
	 * Removes every card owned by a defeated party unit from the draw pile,
	 * discard pile, hand, and exhaust pile, then rebuilds the instance ledger so a dead
	 * character's cards can never be drawn after their death.
	 */
	GAMEXXK_API void RemoveDefeatedPartyOwnerCards(
		FGameXXKBattleDeckState& InOutDeck,
		const TArray<FGameXXKCardCombatUnit>& Units);

	/** Moves exactly one existing hand instance to the end of the discard pile. */
	GAMEXXK_API bool MoveHandCardToDiscard(
		FGameXXKBattleDeckState& InOutDeck,
		FName InstanceId,
		FString* OutError = nullptr);

	/** Resolves a ForcedDiscard choice with distinct IDs that are still in the current hand. */
	GAMEXXK_API bool SubmitForcedDiscard(
		FGameXXKBattleDeckState& InOutDeck,
		const TArray<FName>& DiscardedInstanceIds,
		FString* OutError = nullptr);

	/** Opens a top-N, choose-one insight view without moving any draw-pile instances. */
	GAMEXXK_API bool BeginInsight(
		FGameXXKBattleDeckState& InOutDeck,
		int32 LookCount,
		FString* OutError = nullptr);

	/**
	 * Moves the selected offered instance into Hand and applies the remaining IDs in logical
	 * top-to-bottom order. The remaining order must be a complete permutation of the offer.
	 */
	GAMEXXK_API bool SubmitInsightChoice(
		FGameXXKBattleDeckState& InOutDeck,
		FName PickedInstanceId,
		const TArray<FName>& ReorderedRemainingInstanceIds,
		FString* OutError = nullptr);

	/** Clears only an active insight choice. It never revives a card already moved to discard. */
	GAMEXXK_API bool CancelInsight(FGameXXKBattleDeckState& InOutDeck, FString* OutError = nullptr);

	/** Checks all initialization ledger IDs occur exactly once across DrawPile, Hand, DiscardPile, and ExhaustPile. */
	GAMEXXK_API bool ValidateDeckState(const FGameXXKBattleDeckState& Deck, FString* OutError = nullptr);

	/** Returns a non-owning pointer to the requested live instance, if it exists in a logical zone. */
	GAMEXXK_API const FGameXXKCardInstance* FindInstance(
		const FGameXXKBattleDeckState& Deck,
		FName InstanceId,
		EGameXXKCardZone& OutZone);

	/** Returns the logical draw top (DrawPile.Last()), or nullptr for an empty draw pile. */
	GAMEXXK_API const FGameXXKCardInstance* GetDrawPileTop(const FGameXXKBattleDeckState& Deck);

	GAMEXXK_API bool HasPendingChoice(const FGameXXKBattleDeckState& Deck);

	/**
	 * Produces a pure, stable target-selection request from catalog data and the current battle-unit view.
	 * The function never consumes random state or mutates its input views.
	 */
	GAMEXXK_API bool BuildTargetRequest(
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardInstance& CardInstance,
		EGameXXKCardTerrain Terrain,
		const TArray<FGameXXKCardTargetUnit>& TargetUnits,
		FGameXXKCardTargetRequest& OutRequest,
		FString* OutError = nullptr);

	/**
	 * Resolves a non-manual target request. RandomEnemy advances the supplied deterministic state once;
	 * every other automatic mode leaves it untouched. Outputs change only on success.
	 */
	GAMEXXK_API bool ResolveAutomaticTargetIds(
		const FGameXXKCardTargetRequest& Request,
		const TArray<FGameXXKCardTargetUnit>& TargetUnits,
		int32& InOutRandomState,
		TArray<FName>& OutTargetIds,
		FString* OutError = nullptr);

	/** Returns whether an already-built manual target request contains exactly one legal stable candidate ID. */
	GAMEXXK_API bool IsManualTargetLegal(const FGameXXKCardTargetRequest& Request, FName UnitId);

	/**
	 * Creates a serializable player-phase battle state and its shuffled five-card opening hand.
	 * The operation validates all stable battle/unit identities and commits only on success.
	 */
	GAMEXXK_API bool InitializeCardBattleRuntime(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TArray<FGameXXKCardInstance>& Instances,
		const TArray<FGameXXKCardCombatUnit>& Units,
		EGameXXKCardTerrain Terrain,
		int32 InitialRandomSeed,
		FString* OutError = nullptr);

	/** Validates a persisted card-battle state before it is projected into UI, scene, or save-game code. */
	GAMEXXK_API bool ValidateCardBattleRuntime(const FGameXXKCardBattleRuntime& Runtime, FString* OutError = nullptr);

	/** Queues one non-stacking enemy-phase surcharge to bind after the next player hand refresh. */
	GAMEXXK_API bool QueueNextPlayerHandEnergySurcharge(
		FGameXXKCardBattleRuntime& InOutRuntime,
		int32 SurchargeAmount,
		FName SourceUnitId,
		FString* OutError = nullptr);

	/**
	 * Resolves a ForcedDiscard choice and commits the deck mutation together with cleanup of any
	 * exact hand-bound energy surcharge whose target has just left the hand.
	 */
	GAMEXXK_API bool SubmitForcedDiscard(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TArray<FName>& DiscardedInstanceIds,
		FString* OutError = nullptr,
		TArray<FGameXXKCardPlayResult>* OutResumedResults = nullptr);

	/** Resolves a runtime insight choice, then resumes every remaining automatic card in order. */
	GAMEXXK_API bool SubmitInsightChoice(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FName PickedInstanceId,
		const TArray<FName>& ReorderedRemainingInstanceIds,
		FString* OutError = nullptr,
		TArray<FGameXXKCardPlayResult>* OutResumedResults = nullptr);

	/** Cancels a runtime insight choice, then resumes every remaining automatic card in order. */
	GAMEXXK_API bool CancelInsight(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FString* OutError = nullptr,
		TArray<FGameXXKCardPlayResult>* OutResumedResults = nullptr);

	/** Moves one unfinished equipped Hero card from Draw/Discard to Hand, then resumes any saved queue. */
	GAMEXXK_API bool SubmitHeroTaskSearchChoice(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FName PickedInstanceId,
		TArray<FGameXXKCardPlayResult>& OutResumedResults,
		FString* OutError = nullptr);

	/** Resolves the saved automatic-card continuation until it completes or opens a card choice. */
	GAMEXXK_API bool ResumeAutomaticResolutionQueue(
		FGameXXKCardBattleRuntime& InOutRuntime,
		TArray<FGameXXKCardPlayResult>& OutResults,
		FString* OutError = nullptr);

	/**
	 * Runs the non-mutating CardCheck stage. A successful manual preview is the contract for legal
	 * highlight outlines and owner-to-cursor arrow targeting; no resource or card-zone changes occur here.
	 */
	GAMEXXK_API bool BuildCardPlayPreview(
		const FGameXXKCardBattleRuntime& Runtime,
		FName CardInstanceId,
		FGameXXKCardPlayPreview& OutPreview,
		FString* OutError = nullptr);

	/**
	 * Rebuilds card legality immediately before commit, validates the selected stable UnitId when needed,
	 * then pays resources, moves the exact hand card to discard or exhaust, resolves its data-only effects, and commits atomically.
	 */
	GAMEXXK_API bool ResolveCardPlay(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FName CardInstanceId,
		FName SelectedTargetUnitId,
		FGameXXKCardPlayResult& OutResult,
		FString* OutError = nullptr);

	/** Commits a concrete terrain switch and records that this player round contained a real change. */
	GAMEXXK_API bool NotifyTerrainChanged(
		FGameXXKCardBattleRuntime& InOutRuntime,
		EGameXXKCardTerrain NewTerrain,
		FString* OutError = nullptr);

	/**
	 * Ends the player card phase: rejects unresolved choices, discards unused hand cards, resolves
	 * player-side end-phase DoT, and enters the enemy phase without allowing enemy actions to
	 * interleave with individual card plays.
	 */
	GAMEXXK_API bool EndPlayerCardPhase(
		FGameXXKCardBattleRuntime& InOutRuntime,
		TArray<FGameXXKCardDamageResult>& OutEndPhaseDamageResults,
		FString* OutError = nullptr);

	/**
	 * Resolves one declared enemy direct-damage packet during the enemy phase. The context carries
	 * on-hit statuses and defense bypass so agility can cancel the entire packet. For a single-target
	 * packet, card-driven redirects are applied before normal guard handling; group packets bypass
	 * that redirect but still use the same explicit mitigation rules. Top-level enemy-intent
	 * orchestration may defer terminal-phase evaluation until every effect and reaction in that
	 * saved intent has completed.
	 */
	GAMEXXK_API bool ResolveEnemyDirectAttack(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDamageContext& Context,
		FName SelectedPartyTargetUnitId,
		int32 RequestedDamage,
		FGameXXKCardDamageResult& OutResult,
		TArray<FGameXXKCardDamageResult>* OutReactiveDamageResults = nullptr,
		FString* OutError = nullptr,
		bool bDeferTerminalPhase = false);

	/**
	 * Resolves at most one layer from each independently registered Counter or Block batch after one
	 * complete enemy card. Separate card plays and replays remain separate batches and may each fire.
	 * Only a real single-target attack is eligible; group cards merely clean up reaction
	 * records owned by defeated recipients. Damage is queued without recursively opening another
	 * reaction boundary, and a defeated recipient may still emit the reaction already queued by
	 * the completed enemy card.
	 */
	GAMEXXK_API bool ResolvePartyReactionsAfterEnemyCard(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FName EnemySourceUnitId,
		EGameXXKCardDamageKind CompletedCardKind,
		FName FinalRecipientUnitId,
		TArray<FGameXXKCardDamageResult>& OutReactionDamageResults,
		FString* OutError = nullptr);

	/**
	 * Completes the enemy phase, applies enemy-side DoT, expires round-bound modifiers, then starts a
	 * fresh player phase by resetting shared energy and drawing back to the normal hand limit.
	 */
	GAMEXXK_API bool BeginNextPlayerCardRound(
		FGameXXKCardBattleRuntime& InOutRuntime,
		TArray<FGameXXKCardDamageResult>& OutEndPhaseDamageResults,
		FString* OutError = nullptr);

	/** Refreshes the terminal phase after one complete combat event queue; simultaneous elimination is a player victory. */
	GAMEXXK_API void RefreshCombatTerminalPhase(FGameXXKCardBattleRuntime& InOutRuntime);

	/** Returns the total number of stored stacks for one combat status, saturating malformed duplicate entries safely. */
	GAMEXXK_API int32 GetCombatStatusStacks(const FGameXXKCardCombatUnit& Unit, EGameXXKCardStatus Status);

	/** Adds up to the approved cap for a combat status and returns the number of stacks actually applied. */
	GAMEXXK_API int32 AddCombatStatus(FGameXXKCardCombatUnit& InOutUnit, EGameXXKCardStatus Status, int32 Amount);

	/**
	 * Applies the White Ape's runtime-only status reaction after AddCombatStatus has already
	 * returned a positive applied-stack count for the supplied final status target.
	 */
	GAMEXXK_API bool ResolveWhiteApeStatusGuardAfterStatusApplied(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FGameXXKCardCombatUnit& InOutStatusTarget,
		FString* OutError = nullptr);

	/** Resets living White Ape status guards after a completed enemy phase has entered the player phase. */
	GAMEXXK_API bool ResetWhiteApeStatusGuardsForPlayerRound(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FString* OutError = nullptr);

	/** Removes up to Maximum stacks for one combat status and returns the amount actually consumed. */
	GAMEXXK_API int32 ConsumeCombatStatus(FGameXXKCardCombatUnit& InOutUnit, EGameXXKCardStatus Status, int32 Maximum);

	/** Adds armor up to the approved cap of 99 and returns the amount actually applied. */
	GAMEXXK_API int32 AddCombatArmor(FGameXXKCardCombatUnit& InOutUnit, int32 Amount);

	/** Restores health up to the unit's saved maximum and returns the amount actually restored. */
	GAMEXXK_API int32 HealCombatUnit(FGameXXKCardCombatUnit& InOutUnit, int32 Amount);

	/** Removes shared party energy down to zero and returns the amount actually consumed. */
	GAMEXXK_API int32 ConsumeSharedCombatEnergy(FGameXXKCardBattleRuntime& InOutRuntime, int32 Amount);

	/** Applies owner-phase-start cleanup that is intrinsic to card combat (currently armor expiry). */
	GAMEXXK_API void BeginCombatUnitPhase(FGameXXKCardCombatUnit& InOutUnit);

	/**
	 * Applies owner-end Poison damage, then decays Poison, Burn, Rot, and Weak by their approved rules.
	 * Bleed is unchanged. The atomic snapshot removes guard links made stale by Poison damage.
	 */
	GAMEXXK_API bool ApplyCombatEndPhaseDot(
		TArray<FGameXXKCardCombatUnit>& InOutUnits,
		TArray<FGameXXKCardGuardLinkRuntime>& InOutGuardLinks,
		FName TargetUnitId,
		int32& OutHealthDamage,
		FString* OutError = nullptr);

	/**
	 * Snapshots Bleed, Poison, and Burn in that order, deals each stack value plus current Rot as
	 * health-only damage, then consumes one matching stack unless preservation is active.
	 */
	GAMEXXK_API bool ResolveToxicExplosion(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FName SourceUnitId,
		FName TargetUnitId,
		bool bPreserveDamageOverTimeStacks,
		TArray<FGameXXKCardDamageResult>& OutResults,
		FString* OutError = nullptr);

	/**
	 * Resolves one positive damage packet by stable UnitId using its explicit damage-context policy.
	 * Inputs and outputs are committed atomically only when all supplied combat state is valid.
	 */
	GAMEXXK_API bool ApplyCombatDirectDamage(
		TArray<FGameXXKCardCombatUnit>& InOutUnits,
		TArray<FGameXXKCardGuardLinkRuntime>& InOutGuardLinks,
		const FGameXXKCardDamageContext& Context,
		FName TargetUnitId,
		int32 RequestedDamage,
		FGameXXKCardDamageResult& OutResult,
		FString* OutError = nullptr);

	/**
	 * Resolves one direct damage packet emitted by a player-owned card against the complete
	 * runtime, allowing the final living receiver to apply serializable receiver passives
	 * before health/death commits. Inputs and outputs commit atomically.
	 */
	GAMEXXK_API bool ApplyPlayerCardDirectDamage(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDamageContext& Context,
		FName TargetUnitId,
		int32 RequestedDamage,
		FGameXXKCardDamageResult& OutResult,
		FString* OutError = nullptr);
}
