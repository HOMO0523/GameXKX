#pragma once

#include "CoreMinimal.h"
#include "Narrative/GameXXKNarrativeTypes.h"

enum class EGameXXKStoryTaskDrawerTab : uint8
{
	Actionable,
	Claimable
};

enum class EGameXXKStoryTaskContinuation : uint8
{
	NarrativeReplay,
	RouteResume,
	ObjectiveResume
};

struct GAMEXXK_API FGameXXKStoryTaskDrawerEntryView
{
	FName TaskId;
	EGameXXKTaskState State = EGameXXKTaskState::Locked;
	FText Title;
	FText Summary;
	FText Description;
	FText ActionLabel;
	EGameXXKStoryTaskContinuation Continuation = EGameXXKStoryTaskContinuation::NarrativeReplay;
	FGameXXKNarrativeTaskRewardDefinition MaterialReward;
	int32 AuthoredOrder = 0;
	int64 CompletedAtUtcTicks = 0;
};

struct GAMEXXK_API FGameXXKStoryTaskDrawerUiState
{
	EGameXXKStoryTaskDrawerTab ActiveTab = EGameXXKStoryTaskDrawerTab::Actionable;
	FName SelectedActionableTaskId;
	FName SelectedClaimableTaskId;
	float ActionableScrollOffset = 0.0f;
	float ClaimableScrollOffset = 0.0f;
};

struct GAMEXXK_API FGameXXKStoryTaskDrawerSnapshot
{
	TArray<FGameXXKStoryTaskDrawerEntryView> Actionable;
	TArray<FGameXXKStoryTaskDrawerEntryView> Claimable;
	FName SelectedActionableTaskId;
	FName SelectedClaimableTaskId;
	bool bHasClaimableRedDot = false;
};

class GAMEXXK_API FGameXXKStoryTaskDrawerRules final
{
public:
	static FGameXXKStoryTaskDrawerSnapshot BuildSnapshot(
		const TArray<FGameXXKTaskDefinition>& Tasks,
		const FGameXXKNarrativeProgress& Progress,
		const FGameXXKStoryTaskDrawerUiState& UiState);
};
