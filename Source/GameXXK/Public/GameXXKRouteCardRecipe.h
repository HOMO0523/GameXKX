#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardRunTypes.h"

struct FGameXXKRuntimeState;

/** Pure deterministic construction of the fixed eighteen-entry route deck foundation. */
class GAMEXXK_API FGameXXKRouteCardRecipe final
{
public:
	static constexpr int32 BaseEntryCount = 18;

	/** Builds a stable route-owned entry id from only the normalized route seed and non-negative ordinal. */
	static bool MakeStableEntryId(
		int32 RouteSeed,
		int32 AcquisitionOrdinal,
		FName& OutEntryId,
		FString* OutError = nullptr);

	/** Builds the base recipe without changing the card-run state or caller output on failure. */
	static bool BuildBaseEntries(
		const FGameXXKCardRunState& Run,
		int32 RouteSeed,
		TArray<FGameXXKRouteCardEntry>& OutEntries,
		FString* OutError = nullptr);

	/** Convenience overload for migration/adapter callers that own the complete runtime state. */
	static bool BuildBaseEntries(
		const FGameXXKRuntimeState& State,
		int32 RouteSeed,
		TArray<FGameXXKRouteCardEntry>& OutEntries,
		FString* OutError = nullptr);
};
