#pragma once

#include "CoreMinimal.h"
#include "GameXXKEquipmentTypes.h"
#include "UObject/SoftObjectPath.h"

#include "GameXXKEquipmentCatalog.generated.h"

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentStatCurve
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 LevelOne = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 GrowthNumerator = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 GrowthDivisor = 0;

	int32 Resolve(int32 Level) const;
	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentBaseStatCoefficients
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameXXKEquipmentStatCurve MaxHealth;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameXXKEquipmentStatCurve MaxMana;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameXXKEquipmentStatCurve Attack;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameXXKEquipmentStatCurve Defense;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameXXKEquipmentStatCurve Speed;

	FGameXXKCharacterStats Resolve(int32 Level) const;
	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEquipmentSet Set = EGameXXKEquipmentSet::Invalid;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEquipmentSlot Slot = EGameXXKEquipmentSlot::Invalid;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEquipmentScalingRule ScalingRule = EGameXXKEquipmentScalingRule::Invalid;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameXXKEquipmentBaseStatCoefficients BaseStatCoefficients;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameXXKCharacterStats LegacyBaseStatSnapshot;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FSoftObjectPath IconSoftPath;
};

class GAMEXXK_API FGameXXKEquipmentCatalog final
{
public:
	static const TArray<FGameXXKEquipmentDefinition>& GetPackageDefinitions();
	static const FGameXXKEquipmentDefinition* FindDefinition(FName EquipmentId);
	static bool ValidateDefinition(const FGameXXKEquipmentDefinition& Definition, FString* OutError = nullptr);
	static int32 GetEnhancementStoneCost(int32 CurrentEnhancementLevel);
	static int32 GetReforgeSandCost(EGameXXKEquipmentQuality Quality);
	static int32 GetDismantleSandYield(EGameXXKEquipmentQuality Quality);
};
