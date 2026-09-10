#pragma once

#include "CoreMinimal.h"
#include "GameXXKRelicTypes.h"

struct FGameXXKRuntimeState;

/** Authored high-tier relics; ordinary relic definitions and their IDs remain unchanged. */
namespace GameXXKRelicSynergyRules
{
    GAMEXXK_API const TArray<FGameXXKRelicDefinition>& Definitions();
    GAMEXXK_API void ReplaceHighTierDefinitions(TArray<FGameXXKRelicDefinition>& InOutDefinitions);
    GAMEXXK_API bool Apply(FGameXXKRuntimeState& State, const FGameXXKRelicDefinition& Definition,
        FGameXXKRelicInstance& Instance, EGameXXKRelicTrigger Trigger,
        const FGameXXKRelicActionEvidence* Evidence, const TArray<FGameXXKCardDamageResult>* PrimaryDamage,
        FGameXXKCardPlayResult* CardResult, FString* OutError = nullptr);

    GAMEXXK_API bool BeginAction(FGameXXKRuntimeState& State, const FGameXXKCardBattleRuntime& Before,
        const FGameXXKCardPlayResult& Primary, FGameXXKCardPlayResult& Output, FString* OutError);
    GAMEXXK_API bool ResumeAction(FGameXXKRuntimeState& State,
        TArray<FGameXXKCardPlayResult>& ResumedResults, FString* OutError);
}
