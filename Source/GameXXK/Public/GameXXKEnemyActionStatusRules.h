#pragma once
#include "CoreMinimal.h"
struct FGameXXKCardBattleRuntime;
struct FGameXXKCardDamageResult;

/** Natural status settlement at the end of one actually executed enemy intent. */
namespace GameXXKEnemyActionStatusRules
{
    GAMEXXK_API bool ResolveAfterIntent(FGameXXKCardBattleRuntime& Runtime,FName OwnerUnitId,
        TArray<FGameXXKCardDamageResult>& OutDamage,FString* OutError=nullptr);
}
