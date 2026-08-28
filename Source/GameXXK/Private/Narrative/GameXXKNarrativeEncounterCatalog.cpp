#include "Narrative/GameXXKNarrativeEncounterCatalog.h"

#include "GameXXKEnemyCatalog.h"
#include "Narrative/GameXXKBattleProfile.h"

namespace GameXXKNarrativeEncounterCatalogPrivate
{
	bool SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	const TArray<FGameXXKNarrativeEncounterDefinition>& Definitions()
	{
		static const TArray<FGameXXKNarrativeEncounterDefinition> Result = []
		{
			FGameXXKNarrativeEncounterDefinition Tutorial;
			Tutorial.EncounterId = TEXT("Encounter.Main.XuXiake.0-1");
			Tutorial.EnemyIds = {TEXT("Enemy.Ch1.Rooster")};
			Tutorial.RuleSetId = TEXT("Rules.Tutorial.Basic");
			Tutorial.RewardTableId = TEXT("Rewards.Tutorial.0-1");
			Tutorial.BattleProfileId = TEXT("BattleProfile.Tutorial.0-1");
			return TArray<FGameXXKNarrativeEncounterDefinition>{MoveTemp(Tutorial)};
		}();
		return Result;
	}
}

const FGameXXKNarrativeEncounterDefinition* FGameXXKNarrativeEncounterCatalog::Find(const FName EncounterId)
{
	return GameXXKNarrativeEncounterCatalogPrivate::Definitions().FindByPredicate(
		[EncounterId](const FGameXXKNarrativeEncounterDefinition& Definition)
		{
			return Definition.EncounterId == EncounterId;
		});
}

bool FGameXXKNarrativeEncounterCatalog::Validate(
	const FGameXXKNarrativeEncounterDefinition& Encounter,
	FString* OutError)
{
	using namespace GameXXKNarrativeEncounterCatalogPrivate;
	if (OutError)
	{
		OutError->Reset();
	}
	if (Encounter.EncounterId.IsNone()
		|| Encounter.RuleSetId.IsNone()
		|| Encounter.RewardTableId.IsNone()
		|| Encounter.BattleProfileId.IsNone())
	{
		return SetError(OutError, TEXT("Narrative encounter semantic IDs must not be empty."));
	}
	if (Encounter.EnemyIds.IsEmpty() || Encounter.EnemyIds.Num() > 3)
	{
		return SetError(OutError, TEXT("Narrative encounter must contain between one and three enemies."));
	}
	TSet<FName> EnemyIds;
	for (const FName EnemyId : Encounter.EnemyIds)
	{
		if (EnemyId.IsNone() || EnemyIds.Contains(EnemyId) || !FGameXXKEnemyCatalog::Find(EnemyId))
		{
			return SetError(OutError, FString::Printf(
				TEXT("Narrative encounter has an invalid or duplicate enemy: %s."),
				*EnemyId.ToString()));
		}
		EnemyIds.Add(EnemyId);
	}
	const FGameXXKBattleProfileDefinition* Profile =
		FGameXXKBattleProfileCatalog::Find(Encounter.BattleProfileId);
	if (!Profile)
	{
		return SetError(OutError, TEXT("Narrative encounter battle profile does not exist."));
	}
	return FGameXXKBattleProfileCatalog::Validate(*Profile, OutError);
}
