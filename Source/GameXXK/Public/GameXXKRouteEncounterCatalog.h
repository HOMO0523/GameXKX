#pragma once

#include "CoreMinimal.h"
#include "GameXXKRouteEncounterCatalog.generated.h"

UENUM(BlueprintType)
enum class EGameXXKRouteEncounterKind : uint8
{
	Event,
	Chest
};

UENUM(BlueprintType)
enum class EGameXXKRouteEncounterRewardKind : uint8
{
	RouteAttribute = 0,
	TemporaryNpcSupport = 1 UMETA(Hidden),
	Relic = 2
};

UENUM(BlueprintType)
enum class EGameXXKRouteAttributeKind : uint8
{
	MaxHealth,
	MaxMana,
	Attack,
	Defense,
	Speed
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteEncounterChoiceDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText Label;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKRouteEncounterRewardKind RewardKind = EGameXXKRouteEncounterRewardKind::RouteAttribute;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKRouteAttributeKind AttributeKind = EGameXXKRouteAttributeKind::MaxHealth;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Magnitude = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName QuestNpcId = NAME_None;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteEncounterDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKRouteEncounterKind Kind = EGameXXKRouteEncounterKind::Event;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bPositiveOnly = true;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText Title;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText Speaker;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText Body;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName EventNpcId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FGameXXKRouteEncounterChoiceDefinition> Choices;
};

class GAMEXXK_API FGameXXKRouteEncounterCatalog final
{
public:
	static const TArray<FGameXXKRouteEncounterDefinition>& GetAllDefinitions();
	static const FGameXXKRouteEncounterDefinition* FindDefinition(FName EncounterId);
	static bool IsRetiredNpcEncounterId(FName EncounterId);
	static TArray<const FGameXXKRouteEncounterDefinition*> GetDefinitionsOfKind(EGameXXKRouteEncounterKind Kind);
	static const FGameXXKRouteEncounterDefinition* ChooseDeterministic(EGameXXKRouteEncounterKind Kind, int32 ChoiceSeed);
};
