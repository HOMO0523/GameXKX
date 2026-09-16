#pragma once
#include "CoreMinimal.h"
struct FGameXXKRuntimeState;
struct FGameXXKCardBattleRuntime;
struct FGameXXKBattleDeckState;
struct FGameXXKCardInstance;
struct FGameXXKCardDamageResult;

/** Result-driven instruction over an ordinary challenge. Never owns victory/settlement. */
namespace GameXXKFirstBattleGuide
{
    GAMEXXK_API FName Marker(const TCHAR* Topic);
    GAMEXXK_API bool Has(const FGameXXKRuntimeState& State,const TCHAR* Topic);
    GAMEXXK_API bool Eligible(const FGameXXKRuntimeState& State);
    GAMEXXK_API bool IsSupportCard(FName CardId);
    GAMEXXK_API void PrepareOpening(FGameXXKRuntimeState& State,FGameXXKCardBattleRuntime& Battle);
    GAMEXXK_API void DeliverSupportHand(FGameXXKBattleDeckState& Deck);
    GAMEXXK_API bool IsDeferred(const FGameXXKBattleDeckState& Deck,const FGameXXKCardInstance& Card);
    GAMEXXK_API bool Observe(FGameXXKRuntimeState& State);
    GAMEXXK_API void ObserveDamage(FGameXXKRuntimeState& State,const TArray<FGameXXKCardDamageResult>& Results);
    GAMEXXK_API FName NextTopic(const FGameXXKRuntimeState& State);
    GAMEXXK_API FName SuggestedCard(FName Topic);
}
