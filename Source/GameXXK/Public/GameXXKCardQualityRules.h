#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"
#include "GameXXKRelicTypes.h"

/** Stable base-quality classification shared by immutable card and relic catalogs. */
class GAMEXXK_API FGameXXKCardQualityRules final
{
public:
	/** Returns a scaled copy for the requested runtime quality without mutating catalog data. */
	static FGameXXKCardDefinition BuildEffectiveDefinition(
		const FGameXXKCardDefinition& BaseDefinition,
		EGameXXKCardQuality CurrentQuality);
	static int32 GetCardPrice(EGameXXKCardQuality Quality);
	static int32 GetRelicPrice(EGameXXKCardQuality Quality);
	static FText GetDisplayName(EGameXXKCardQuality Quality);
	static FLinearColor GetDisplayColor(EGameXXKCardQuality Quality);

	static EGameXXKCardQuality GetCardBaseQuality(FName CardId);
	static EGameXXKCardQuality GetRelicBaseQuality(FName RelicId);
	static bool ValidateCardCatalog(const TArray<FGameXXKCardDefinition>& Definitions, FString& OutError);
	static bool ValidateRelicCatalog(const TArray<FGameXXKRelicDefinition>& Definitions, FString& OutError);
};
