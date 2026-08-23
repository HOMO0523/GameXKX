#pragma once

#include "CoreMinimal.h"
#include "GameXXKPartyFormationTypes.h"

struct FGameXXKRuntimeState;

/** Pure validation, legacy projection, and compatibility rules for ordered party deployment. */
class GAMEXXK_API FGameXXKPartyFormationRules final
{
public:
	static constexpr int32 PartySize = 3;
	static constexpr int32 MinimumOwnedPermanentCompanions = 2;

	static bool BuildLegacyProjection(
		const FGameXXKRuntimeState& State,
		FGameXXKOrderedPartyFormation& OutFormation);
	static bool ResolveEffective(
		const FGameXXKRuntimeState& State,
		FGameXXKOrderedPartyFormation& OutFormation,
		FString* OutError = nullptr);
	static bool Validate(
		const FGameXXKRuntimeState& State,
		const FGameXXKOrderedPartyFormation& Formation,
		FString* OutError = nullptr);
	/** Verifies legacy active companion/NPC mirrors are the exact projection of ordered formation. */
	static bool ValidateCompatibilityProjection(
		const FGameXXKRuntimeState& State,
		FString* OutError = nullptr);
	/**
	 * Builds an order-preserving formation after route cleanup retires a known task NPC.
	 * Only known quest-NPC slots with both availability mirrors cleared may be replaced;
	 * unrelated current-version corruption is rejected and OutFormation stays unchanged.
	 */
	static bool RepairUnavailableQuestNpcSlotsPreservingOrder(
		const FGameXXKRuntimeState& State,
		FGameXXKOrderedPartyFormation& OutFormation,
		FString* OutError = nullptr);
	static bool Normalize(FGameXXKRuntimeState& InOutState, FString* OutError = nullptr);
	static void ProjectCompatibility(FGameXXKRuntimeState& InOutState);
};
