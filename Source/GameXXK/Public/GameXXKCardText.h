#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

/** Pure interaction state that a caller may append to immutable card text without mutating gameplay. */
struct GAMEXXK_API FGameXXKCardTooltipContext
{
	FString InteractionResult;
	FString UnavailableReason;
};

/**
 * Pure player-facing text generated from immutable card data and non-mutating play previews.
 * This namespace intentionally owns no state and must not branch on card IDs.
 */
namespace GameXXKCardText
{
	/** One authoritative player-facing name for every serialized battle status. */
	GAMEXXK_API FString DescribeStatusName(EGameXXKCardStatus Status);
	GAMEXXK_API FString DescribeTarget(const FGameXXKCardTargetSpec& TargetSpec);
	/** Source-compatible catalog path: resolves Definition.BaseQuality, with Invalid falling back to Common. */
	GAMEXXK_API FString DescribeEffects(const FGameXXKCardDefinition& Definition);
	/** Runtime path: resolves the explicitly supplied current quality. */
	GAMEXXK_API FString DescribeEffects(const FGameXXKCardDefinition& Definition, EGameXXKCardQuality Quality);

	GAMEXXK_API FString DescribeDetail(const FGameXXKCardDefinition& Definition, const FGameXXKCardPlayPreview* Preview);
	GAMEXXK_API FString DescribeDetail(
		const FGameXXKCardDefinition& Definition,
		EGameXXKCardQuality Quality,
		const FGameXXKCardPlayPreview* Preview);

	/** Builds a hover-only tooltip from card detail plus caller-supplied actual interaction and unavailable-state lines. */
	GAMEXXK_API FString DescribeTooltip(
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardPlayPreview* Preview,
		const FGameXXKCardTooltipContext& Context);
	GAMEXXK_API FString DescribeTooltip(
		const FGameXXKCardDefinition& Definition,
		EGameXXKCardQuality Quality,
		const FGameXXKCardPlayPreview* Preview,
		const FGameXXKCardTooltipContext& Context);
}
