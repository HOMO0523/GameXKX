#include "GameXXKRelicSynergyRules.h"
#include "UI/GameXXKLocalization.h"

namespace
{
    FGameXXKRelicDefinition MakeSynergy(const TCHAR* Slug, EGameXXKCardQuality Quality,
        EGameXXKRelicTrigger Trigger, const TCHAR* Effect, int32 Primary, int32 Secondary)
    {
        FGameXXKRelicDefinition Result;
        const FString Id = FString(TEXT("Relic.")) + Slug;
        Result.Id = FName(Id);
        Result.DisplayName = GameXXKLocalization::Text(*(Id + TEXT(".Name")));
        Result.Description = GameXXKLocalization::Text(*(Id + TEXT(".Description")));
        Result.DetailedDescription = Result.Description;
        Result.BaseQuality = Quality;
        Result.Trigger = Trigger;
        Result.EffectKind = EGameXXKRelicEffectKind::Synergy;
        Result.SynergyKey = Effect;
        Result.Magnitude = Primary;
        Result.SecondaryMagnitude = Secondary;
        Result.bStackable = false;
        Result.IconTexturePath = FSoftObjectPath(FString::Printf(
            TEXT("/Game/GameXXK/UI/Relics/Icons/T_Relic_%s.T_Relic_%s"), Slug, Slug));
        return Result;
    }
}

const TArray<FGameXXKRelicDefinition>& GameXXKRelicSynergyRules::Definitions()
{
    static const TArray<FGameXXKRelicDefinition> Result = {
#include "GameXXKRelicSynergyDefinitions.inl"
    };
    return Result;
}

void GameXXKRelicSynergyRules::ReplaceHighTierDefinitions(TArray<FGameXXKRelicDefinition>& InOutDefinitions)
{
    for (const FGameXXKRelicDefinition& Replacement : Definitions())
    {
        FGameXXKRelicDefinition* Existing = InOutDefinitions.FindByPredicate(
            [&](const FGameXXKRelicDefinition& Value) { return Value.Id == Replacement.Id; });
        if (Existing) *Existing = Replacement;
        else InOutDefinitions.Add(Replacement);
    }
}
