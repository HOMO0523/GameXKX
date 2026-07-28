#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardRunTypes.h"

/** Observable result of adding one stable route-card entry and resolving deterministic pairs. */
struct GAMEXXK_API FGameXXKCardMergePreview
{
	bool bWillMerge = false;
	FName SurvivorEntryId = NAME_None;
	TArray<FName> ConsumedEntryIds;
	EGameXXKCardQuality FinalQuality = EGameXXKCardQuality::Common;
	int32 TemporaryCountDelta = 0;
	int32 CapacityDelta = 0;
};

/** Whether a validated acquisition can commit immediately or needs a stable entry selection. */
enum class EGameXXKRouteCardAcquisitionDecision : uint8
{
	CanCommit,
	RequiresReplacement
};

/** Pure acquisition summary shared by preview and commit. */
struct GAMEXXK_API FGameXXKRouteCardAcquisitionPreview
{
	EGameXXKRouteCardAcquisitionDecision Decision = EGameXXKRouteCardAcquisitionDecision::CanCommit;
	FGameXXKCardMergePreview Merge;
	int32 CapacityBefore = 0;
	int32 CapacityAfter = 0;
	FName ReplacementEntryId = NAME_None;
	TArray<FName> EligibleReplacementEntryIds;
};

/** Pure rules for validated, deterministic route-card pair merging. */
class GAMEXXK_API FGameXXKRunDeckRules final
{
public:
	static constexpr int32 MaxRouteCardCapacity = 12;

	/** Validates every stable entry, then reports capacity using bConsumesRouteCapacity only. */
	static bool GetCapacityUsed(
		const TArray<FGameXXKRouteCardEntry>& Entries,
		int32& OutCapacityUsed,
		FString* OutError = nullptr);

	static bool PreviewAdd(
		const TArray<FGameXXKRouteCardEntry>& Entries,
		const FGameXXKRouteCardEntry& Candidate,
		FGameXXKCardMergePreview& OutPreview,
		FString* OutError = nullptr);

	static bool AddAndMerge(
		TArray<FGameXXKRouteCardEntry>& InOutEntries,
		const FGameXXKRouteCardEntry& Candidate,
		FGameXXKCardMergePreview& OutApplied,
		FString* OutError = nullptr);

	static bool PreviewAcquisition(
		const FGameXXKCardRunState& CardRun,
		const FGameXXKRouteCardEntry& Candidate,
		FName ReplacementEntryId,
		FGameXXKRouteCardAcquisitionPreview& OutPreview,
		FString* OutError = nullptr);

	static bool CommitAcquisition(
		FGameXXKCardRunState& InOutCardRun,
		const FGameXXKRouteCardEntry& Candidate,
		FName ReplacementEntryId,
		FGameXXKRouteCardAcquisitionPreview& OutApplied,
		FString* OutError = nullptr);
};
