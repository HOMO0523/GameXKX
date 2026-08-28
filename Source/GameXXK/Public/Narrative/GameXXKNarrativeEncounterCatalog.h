#pragma once

#include "CoreMinimal.h"

#include "GameXXKNarrativeEncounterCatalog.generated.h"

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKNarrativeEncounterDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative|Encounter")
	FName EncounterId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative|Encounter")
	TArray<FName> EnemyIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative|Encounter")
	FName RuleSetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative|Encounter")
	FName RewardTableId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative|Encounter")
	FName BattleProfileId;
};

class GAMEXXK_API FGameXXKNarrativeEncounterCatalog
{
public:
	static const FGameXXKNarrativeEncounterDefinition* Find(FName EncounterId);
	static bool Validate(const FGameXXKNarrativeEncounterDefinition& Encounter, FString* OutError = nullptr);
};
