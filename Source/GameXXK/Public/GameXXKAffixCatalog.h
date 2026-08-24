#pragma once

#include "CoreMinimal.h"
#include "GameXXKEquipmentTypes.h"

#include "GameXXKAffixCatalog.generated.h"

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKAffixTierWeights
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Common = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Rare = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Epic = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Legendary = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Immortal = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Treasure = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Transcendent = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Celestial = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Ascendant = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Cosmic = 0;

	int32 GetWeight(EGameXXKAffixTier Tier) const;
	void SetWeight(EGameXXKAffixTier Tier, int32 Weight);
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKAffixMagnitudeRange
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Minimum = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Maximum = 0;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKAffixDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEquipmentSet Set = EGameXXKEquipmentSet::Invalid;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEquipmentModifierKind ModifierKind = EGameXXKEquipmentModifierKind::Invalid;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEquipmentMagnitudeUnit Unit = EGameXXKEquipmentMagnitudeUnit::Invalid;
};

class GAMEXXK_API FGameXXKAffixCatalog final
{
public:
	static const TArray<FGameXXKAffixDefinition>& GetUniversalDefinitions();
	static const TArray<FGameXXKAffixDefinition>& GetSetDefinitions(EGameXXKEquipmentSet Set);
	static const TArray<FGameXXKAffixDefinition>& GetAllDefinitions();
	static const FGameXXKAffixDefinition* FindDefinition(FName AffixId);
	static FGameXXKAffixTierWeights GetTierWeights(EGameXXKEquipmentQuality Quality);
	static FGameXXKAffixMagnitudeRange GetMagnitudeRange(EGameXXKEquipmentMagnitudeUnit Unit, EGameXXKAffixTier Tier);
};
