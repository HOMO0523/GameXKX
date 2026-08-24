#pragma once

#include "CoreMinimal.h"
#include "GameXXKEquipmentTypes.h"

/** Stable item IDs, values, icons, and socket-capacity rules for all gems. */
class GAMEXXK_API FGameXXKGemRules final
{
public:
	static constexpr int32 MinimumQualityRank = 1;
	static constexpr int32 MaximumQualityRank = 10;

	static bool IsValidType(EGameXXKGemType Type);
	static bool IsValidQuality(EGameXXKGemQuality Quality);
	static bool IsValidSocketValue(const FGameXXKSocketedGem& Gem);
	static int32 GetQualityRank(EGameXXKGemQuality Quality);
	static EGameXXKGemQuality QualityFromRank(int32 Rank);
	static EGameXXKGemQuality GetNextQuality(EGameXXKGemQuality Quality);
	static int32 GetSocketCapacity(EGameXXKEquipmentQuality EquipmentQuality);
	static int32 GetStatBonus(EGameXXKGemType Type, EGameXXKGemQuality Quality);
	static FText GetTypeDisplayName(EGameXXKGemType Type);
	static FText GetQualityDisplayName(EGameXXKGemQuality Quality);
	static FText GetDisplayName(EGameXXKGemType Type, EGameXXKGemQuality Quality);
	static FName MakeItemId(EGameXXKGemType Type, EGameXXKGemQuality Quality);
	static bool TryParseItemId(FName ItemId, EGameXXKGemType& OutType, EGameXXKGemQuality& OutQuality);
	static FSoftObjectPath GetIconTexturePath(EGameXXKGemType Type, EGameXXKGemQuality Quality);
	static FSoftObjectPath GetIconTexturePathForItemId(FName ItemId);
	static TArray<FName> GetAllItemIds();
};
