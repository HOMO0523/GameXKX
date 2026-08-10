#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

/**
 * Read-only, deterministically ordered card data for future deck and battle systems.
 * The catalog never mutates its backing definitions after first construction.
 */
class GAMEXXK_API FGameXXKCardCatalog final
{
public:
	static const TArray<FGameXXKCardDefinition>& GetAllCardDefinitions();
	static const FGameXXKCardDefinition* FindCardDefinition(FName CardId);
	static bool FindCardDefinition(FName CardId, FGameXXKCardDefinition& OutDefinition);
	/** Validates one externally supplied definition against the catalog data contract. */
	static bool ValidateCardDefinition(const FGameXXKCardDefinition& Definition, FString& OutError);
	static bool ValidateCardDefinitions(FString& OutError);
	static TArray<FGameXXKCardDefinition> GetCardDefinitionsForOwner(FName OwnerId);
	/** Returns the canonical protagonist pool available at the clamped hero level, in catalog order. */
	static TArray<FName> GetHeroCardIdsUnlockedAtLevel(int32 HeroLevel);

	static const TArray<FGameXXKCardVisualDefinition>& GetCardVisualDefinitions();
	static const FGameXXKCardVisualDefinition* FindCardVisualDefinition(FName CardId);
	static bool FindCardVisualDefinition(FName CardId, FGameXXKCardVisualDefinition& OutDefinition);
};
