#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"
#include "Narrative/GameXXKMainStoryCatalog.h"

class GAMEXXK_API FGameXXKMainStoryRules final
{
public:
	static bool IsChapterUnlocked(const FGameXXKRuntimeState& State, FName ChapterId);
	static bool IsNodeCompleted(const FGameXXKRuntimeState& State, FName NodeId);
	static EGameXXKTaskState NodeState(const FGameXXKRuntimeState& State, FName NodeId);
	static bool HasUnseenChapter(const FGameXXKRuntimeState& State);
	static bool HasUnclaimedReward(const FGameXXKRuntimeState& State);
	static bool MarkChapterSeen(FGameXXKRuntimeState& State, FName ChapterId);
	static bool StartNode(FGameXXKRuntimeState& State, FName NodeId, FString* OutError = nullptr);
	static bool AdvanceDialogue(FGameXXKRuntimeState& State, FString* OutError = nullptr);
	static bool ChooseAnswer(FGameXXKRuntimeState& State, int32 Index, FText& OutFeedback, FString* OutError = nullptr);
	static bool RevealHint(FGameXXKRuntimeState& State, FText& OutHint);
	static bool ClaimReward(FGameXXKRuntimeState& State, FName NodeId, FString* OutError = nullptr);
	static bool InjectJourneyGate(FGameXXKRuntimeState& State, FName NodeId, FString* OutError = nullptr);
	static bool GenerateDedicatedJourneyMap(FGameXXKRuntimeState& State, FName NodeId, int32 Seed, FString* OutError = nullptr);
	static bool IsDedicatedJourney(const FGameXXKRuntimeState& State);
	static bool BuildJourneyBattleEncounter(const FGameXXKRuntimeState& State, FGameXXKTrainingEncounterDefinition& OutEncounter);
	static bool FinishDedicatedJourneyNode(FGameXXKRuntimeState& State, FString* OutError = nullptr);
	static bool IsJourneyGate(const FGameXXKRuntimeState& State, int32 RouteNodeId);
	static bool EnterJourneyGate(FGameXXKRuntimeState& State, int32 RouteNodeId, FString* OutError = nullptr);
	static bool ObserveBattleVictory(FGameXXKRuntimeState& State);
	static bool HasBattleVictory(const FGameXXKRuntimeState& State, FName NodeId);
	static bool HasPendingAfterBattleDialogue(const FGameXXKRuntimeState& State, FName NodeId);
	static void PauseActivity(FGameXXKRuntimeState& State);
	static bool ValidateState(const FGameXXKRuntimeState& State, FString* OutError = nullptr);
	static FName NextMainlineNode(const FGameXXKRuntimeState& State, FName ChapterId);
};
