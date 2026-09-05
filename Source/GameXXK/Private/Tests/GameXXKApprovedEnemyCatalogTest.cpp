#include "GameXXKEnemyCatalog.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FGameXXKEnemyIntentDefinition* FindIntent(
		const TCHAR* EnemyId,
		const int32 PhaseNumber,
		const TCHAR* IntentId)
	{
		const FGameXXKEnemyDefinition* Enemy = FGameXXKEnemyCatalog::Find(EnemyId);
		const TArray<FGameXXKEnemyIntentDefinition>* Intents = Enemy
			? FGameXXKEnemyCatalog::GetPhaseIntents(*Enemy, PhaseNumber)
			: nullptr;
		return Intents ? Intents->FindByPredicate([IntentId](const FGameXXKEnemyIntentDefinition& Intent)
		{
			return Intent.Id == FName(IntentId);
		}) : nullptr;
	}

	const FGameXXKEnemyIntentEffectDefinition* FindEffect(
		const FGameXXKEnemyIntentDefinition* Intent,
		const EGameXXKEnemyIntentEffectType Type)
	{
		return Intent ? Intent->Effects.FindByPredicate([Type](const FGameXXKEnemyIntentEffectDefinition& Effect)
		{
			return Effect.Type == Type;
		}) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKApprovedEnemyCatalogTest,
	"GameXXK.Data.EnemyCatalog.Approved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKApprovedEnemyCatalogTest::RunTest(const FString& Parameters)
{
	FString Error;
	if (!TestTrue(TEXT("the approved enemy catalog validates"), FGameXXKEnemyCatalog::Validate(&Error)))
	{
		AddError(Error);
		return false;
	}
	const TArray<FGameXXKEnemyDefinition>& Definitions = FGameXXKEnemyCatalog::GetAllDefinitions();
	TestEqual(TEXT("all twenty-one monsters are present"), Definitions.Num(), 21);
	int32 OrdinaryCases = 0;
	int32 PhaseOneCases = 0;
	int32 PhaseTwoCases = 0;
	int32 PhaseThreeCases = 0;
	for (const FGameXXKEnemyDefinition& Definition : Definitions)
	{
		TestTrue(Definition.Id.ToString() + TEXT(" leaves the retired intent array empty"), Definition.Intents.IsEmpty());
		TestEqual(
			Definition.Id.ToString() + TEXT(" phase count"),
			Definition.Phases.Num(),
			Definition.Tier == EGameXXKEnemyTier::Normal ? 1 : 3);
		for (int32 PhaseIndex = 0; PhaseIndex < Definition.Phases.Num(); ++PhaseIndex)
		{
			const FGameXXKEnemyPhaseDefinition& Phase = Definition.Phases[PhaseIndex];
			TestEqual(Definition.Id.ToString() + TEXT(" contiguous phase number"), Phase.PhaseNumber, PhaseIndex + 1);
			TestTrue(Definition.Id.ToString() + TEXT(" phase deck is nonempty"), !Phase.Intents.IsEmpty());
			for (const FGameXXKEnemyIntentDefinition& Intent : Phase.Intents)
			{
				for (const FGameXXKEnemyIntentEffectDefinition& Effect : Intent.Effects)
				{
					if (Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage)
					{
						TestTrue(Intent.Id.ToString() + TEXT(" has Normal attack percent"), Effect.AttackPercentByDifficulty.Normal > 0);
						TestTrue(Intent.Id.ToString() + TEXT(" has Hard attack percent"), Effect.AttackPercentByDifficulty.Hard > 0);
						TestTrue(Intent.Id.ToString() + TEXT(" has Hell attack percent"), Effect.AttackPercentByDifficulty.Hell > 0);
					}
				}
			}
			if (Definition.Tier == EGameXXKEnemyTier::Normal) OrdinaryCases += Phase.Intents.Num() * 3;
			else if (PhaseIndex == 0) PhaseOneCases += Phase.Intents.Num() * 3;
			else if (PhaseIndex == 1) PhaseTwoCases += Phase.Intents.Num() * 2;
			else PhaseThreeCases += Phase.Intents.Num();
		}
	}
	TestEqual(TEXT("ordinary resolved cases"), OrdinaryCases, 108);
	TestEqual(TEXT("phase-one resolved cases"), PhaseOneCases, 126);
	TestEqual(TEXT("phase-two resolved cases"), PhaseTwoCases, 78);
	TestEqual(TEXT("phase-three resolved cases"), PhaseThreeCases, 39);
	TestEqual(TEXT("the complete resolved matrix"), OrdinaryCases + PhaseOneCases + PhaseTwoCases + PhaseThreeCases, 351);

	const FGameXXKEnemyIntentEffectDefinition* RapidPeck = FindEffect(
		FindIntent(TEXT("Enemy.Ch1.IronfeatherRooster"), 1, TEXT("RapidPeck")),
		EGameXXKEnemyIntentEffectType::DirectDamage);
	if (TestNotNull(TEXT("Ironfeather Rapid Peck exists"), RapidPeck))
	{
		TestEqual(TEXT("Rapid Peck Normal percent"), RapidPeck->AttackPercentByDifficulty.Normal, 80);
		TestEqual(TEXT("Rapid Peck Hard percent"), RapidPeck->AttackPercentByDifficulty.Hard, 115);
		TestEqual(TEXT("Rapid Peck Hell percent"), RapidPeck->AttackPercentByDifficulty.Hell, 150);
		TestEqual(TEXT("Rapid Peck hit count"), RapidPeck->HitCount, 3);
	}
	const FGameXXKEnemyIntentEffectDefinition* LifeBound = FindEffect(
		FindIntent(TEXT("Enemy.Ch1.IronfeatherRooster"), 3, TEXT("LifeBoundRapidPeck")),
		EGameXXKEnemyIntentEffectType::DirectDamage);
	if (TestNotNull(TEXT("phase-three Life-Bound Rapid Peck exists"), LifeBound))
	{
		TestEqual(TEXT("Life-Bound percent"), LifeBound->AttackPercentByDifficulty.Hell, 180);
		TestEqual(TEXT("Life-Bound hit count"), LifeBound->HitCount, 4);
	}

	struct FStatCheckpoint
	{
		const TCHAR* Id;
		int32 HP;
		int32 Attack;
		int32 Defense;
	};
	const FStatCheckpoint Checkpoints[] = {
		{TEXT("Enemy.Ch1.IronfeatherRooster"), 2128, 242, 72},
		{TEXT("Enemy.Ch1.BluehornGoatKing"), 2416, 227, 94},
		{TEXT("Enemy.Ch1.MoneyRat"), 3456, 285, 109},
		{TEXT("Enemy.Ch2.GraymaneWolfKing"), 2570, 286, 80},
		{TEXT("Enemy.Ch2.RedtuskBoarKing"), 2868, 272, 110},
		{TEXT("Enemy.Ch2.BlackBear"), 4340, 345, 132},
		{TEXT("Enemy.Ch3.WhiteApe"), 3012, 316, 95},
		{TEXT("Enemy.Ch3.SpiralHornDeer"), 3158, 301, 110},
		{TEXT("Enemy.Ch3.Tiger"), 4936, 390, 146}};
	for (const FStatCheckpoint& Checkpoint : Checkpoints)
	{
		const FGameXXKEnemyComputedStats Stats = FGameXXKEnemyCatalog::ComputeStats(Checkpoint.Id, 135);
		TestEqual(FString(Checkpoint.Id) + TEXT(" L135 HP"), Stats.MaxHP, Checkpoint.HP);
		TestEqual(FString(Checkpoint.Id) + TEXT(" L135 Attack"), Stats.Attack, Checkpoint.Attack);
		TestEqual(FString(Checkpoint.Id) + TEXT(" L135 Defense"), Stats.Defense, Checkpoint.Defense);
	}
	return true;
}

#endif
