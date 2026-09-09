#pragma once

#include "CoreMinimal.h"

struct GAMEXXK_API FGameXXKToolCombineOutcome
{
    int32 QualityRank = 0;
    int32 WeightPerThousand = 0;
};

/** Shared by the nine-to-one transaction and the visible recipe preview. */
class GAMEXXK_API FGameXXKToolCombineProbability final
{
public:
    static TArray<FGameXXKToolCombineOutcome> GetOutcomes(int32 InputQualityRank);
    /** Exact integer draw in [0, 999]; zero means an invalid recipe or draw. */
    static int32 Resolve(int32 InputQualityRank, int32 RollPerThousand);
    static FText Describe(int32 InputQualityRank);
};
