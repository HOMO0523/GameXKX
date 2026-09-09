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
        const FGameXXKCardBattleRuntime* BeforeCard, const TArray<FGameXXKCardDamageResult>* PrimaryDamage,
        FGameXXKCardPlayResult* CardResult, FString* OutError = nullptr);
}
