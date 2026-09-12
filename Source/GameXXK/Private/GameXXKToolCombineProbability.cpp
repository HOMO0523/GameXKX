#include "GameXXKToolCombineProbability.h"
#include "GameXXKGemRules.h"

TArray<FGameXXKToolCombineOutcome> FGameXXKToolCombineProbability::GetOutcomes(const int32 Rank)
{
    // Frozen nine-to-one table, 2026-08-28 design section 14, revised 2026-09-11 (proposal B).
    // Every rank advances by exactly one quality or keeps its quality; there are no multi-rank
    // jumps, so the expected advance per combine (success rate / 1000) is a single smooth
    // monotonically decreasing curve: 1.00 / 1.00 / 1.00 / 0.90 / 0.85 / 0.80 / 0.75 / 0.70 / 0.60.
    if (Rank >= 1 && Rank <= 3) return {{Rank + 1, 1000}};
    if (Rank == 4) return {{4, 100}, {5, 900}};
    if (Rank == 5) return {{5, 150}, {6, 850}};
    if (Rank == 6) return {{6, 200}, {7, 800}};
    if (Rank == 7) return {{7, 250}, {8, 750}};
    if (Rank == 8) return {{8, 300}, {9, 700}};
    if (Rank == 9) return {{9, 400}, {10, 600}};
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
