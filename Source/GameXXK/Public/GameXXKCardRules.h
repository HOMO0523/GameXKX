#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

/** Pure, deterministic card-zone rules. This layer deliberately does not resolve card effects or turns. */
namespace GameXXKCardRules
{
	/** Validates unique materialized IDs, deterministically shuffles, and draws up to the normal hand limit. */
	GAMEXXK_API bool InitializeBattleDeck(
		FGameXXKBattleDeckState& InOutDeck,
		const TArray<FGameXXKCardInstance>& Instances,
		int32 InitialRandomSeed,
		FString* OutError = nullptr);

	/**
	 * Draws up to Count cards. Normal draws stop at HandLimit; a temporary overdraw may stop at
	 * HandLimit + 1 and creates a ForcedDiscard choice for the exact excess.
	 */
	GAMEXXK_API bool DrawCards(
		FGameXXKBattleDeckState& InOutDeck,
		int32 Count,
		bool bAllowTemporaryOverdraw,
		FString* OutError = nullptr);

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

	/** Checks all initialization ledger IDs occur exactly once across DrawPile, Hand, and DiscardPile. */
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
}
