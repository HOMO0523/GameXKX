#include "GameXXKToolCombineProbability.h"
#include "GameXXKGemRules.h"

TArray<FGameXXKToolCombineOutcome> FGameXXKToolCombineProbability::GetOutcomes(const int32 Rank)
{
    // Frozen nine-to-one table, 2026-08-28 design section 14.
    if (Rank >= 1 && Rank <= 3) return {{Rank + 1, 1000}};
    if (Rank >= 4 && Rank <= 6) return {{Rank + 1, 499}, {Rank + 2, 499}, {Rank + 3, 2}};
    if (Rank == 7 || Rank == 8) return {{Rank, 659}, {Rank + 1, 339}, {Rank + 2, 2}};
    if (Rank == 9) return {{9, 750}, {10, 250}};
    return {};
}

int32 FGameXXKToolCombineProbability::Resolve(const int32 Rank, const int32 Roll)
{
    if (Roll < 0 || Roll >= 1000) return 0;
    int32 Cumulative = 0;
    for (const auto& Outcome : GetOutcomes(Rank))
    {
        Cumulative += Outcome.WeightPerThousand;
        if (Roll < Cumulative) return Outcome.QualityRank;
    }
    return 0;
}

FText FGameXXKToolCombineProbability::Describe(const int32 Rank)
{
    TArray<FString> Lines;
    for (const auto& Outcome : GetOutcomes(Rank))
    {
        const FString Percent = Outcome.WeightPerThousand % 10 == 0
            ? FString::Printf(TEXT("%d%%"), Outcome.WeightPerThousand / 10)
            : FString::Printf(TEXT("%.1f%%"), Outcome.WeightPerThousand / 10.0);
        const FString Quality = FGameXXKGemRules::GetQualityDisplayName(
            FGameXXKGemRules::QualityFromRank(Outcome.QualityRank)).ToString();
        Lines.Add(FString::Printf(TEXT("%s%s  %s"), *Quality,
            Outcome.QualityRank == Rank ? TEXT("（原品质）") : TEXT(""), *Percent));
    }
    return FText::FromString(FString::Join(Lines, TEXT("\n")));
}
