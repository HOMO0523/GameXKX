#include "Misc/AutomationTest.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKTalentCatalog.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FGameXXKCardCombatUnit* FindUnit(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	bool BeginFixtureBattle(FGameXXKRuntimeState& State, FString& Error)
	{
		State.ActiveBattleEnemies.Reset();
		FGameXXKBattleRuntimeUnit Enemy;
		Enemy.Id = TEXT("Enemy.Talent.Target.P1");
		Enemy.DisplayName = FText::FromString(TEXT("天赋木桩"));
		Enemy.HP = 10000;
		Enemy.MaxHP = 10000;
		Enemy.Attack = 1;
		Enemy.Defense = 0;
		Enemy.Speed = 1;
		Enemy.Shield = 0;
		Enemy.bEnemy = true;
		Enemy.EnemyDefinitionId = TEXT("Enemy.Ch1.Rooster");
		Enemy.BattleSlotNumber = 1;
		Enemy.CombatLevel = 1;
		State.ActiveBattleEnemies.Add(Enemy);
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = INDEX_NONE;
		return FGameXXKCardBattleAdapter::BeginCardBattle(
			State,
			EGameXXKNodeKind::Battle,
			EGameXXKCardTerrain::Plain,
			0x5511,
			&Error);
	}

	int32 FindSeedWhoseSecondRollCrits()
	{
		constexpr uint32 Multiplier = 196314165u;
		constexpr uint32 Increment = 907633515u;
		for (uint32 Seed = 1; Seed < 100000; ++Seed)
		{
			const uint32 First = Seed * Multiplier + Increment;
			const uint32 Second = First * Multiplier + Increment;
			if (Second % 100u < 20u)
			{
				return static_cast<int32>(Seed);
			}
		}
		return 1;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentCombatIntegrationTest,
	"GameXXK.Talents.Combat.PartyStatsFinalDamageAndCritical",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentCombatIntegrationTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Fixture =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("combat talent fixture starts"), Fixture && Fixture->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState Baseline = Fixture->GetRuntimeStateCopy();
	FGameXXKRuntimeState Talented = Baseline;
	for (const FGameXXKTalentNodeDefinition& Node : FGameXXKTalentCatalog::GetDefinitions())
	{
		if (Node.bRoot || Node.Branch == EGameXXKTalentBranch::Combat)
		{
			Talented.Talents.NodeRanks.Add(Node.Id, Node.MaxRank);
		}
	}

	FString Error;
	if (!TestTrue(TEXT("baseline battle begins"), BeginFixtureBattle(Baseline, Error))
		|| !TestTrue(TEXT("talented battle begins"), BeginFixtureBattle(Talented, Error)))
	{
		AddError(Error);
		return false;
	}
	const FName HeroId = FGameXXKEquipmentRules::HeroCharacterId();
	const FGameXXKCardCombatUnit* BaselineHero = FindUnit(Baseline.CardRun.ActiveBattle, HeroId);
	const FGameXXKCardCombatUnit* TalentedHero = FindUnit(Talented.CardRun.ActiveBattle, HeroId);
	if (!TestNotNull(TEXT("baseline hero materializes"), BaselineHero)
		|| !TestNotNull(TEXT("talented hero materializes"), TalentedHero))
	{
		return false;
	}
	TestTrue(TEXT("flat and percent attack increase the party hero"), TalentedHero->Attack > BaselineHero->Attack);
	TestTrue(TEXT("flat and percent health increase the party hero"), TalentedHero->MaxHP > BaselineHero->MaxHP);
	TestTrue(TEXT("flat and percent defense increase the party hero"), TalentedHero->Defense > BaselineHero->Defense);
	TestEqual(TEXT("battle snapshots final-damage talent"), Talented.CardRun.ActiveBattle.TalentFinalDamagePercent, 100);
	TestEqual(TEXT("battle snapshots critical-chance talent"), Talented.CardRun.ActiveBattle.TalentCriticalChancePercent, 20);
	TestEqual(TEXT("battle snapshots critical-damage talent"), Talented.CardRun.ActiveBattle.TalentCriticalDamagePercent, 50);

	FGameXXKCardBattleRuntime DamageRuntime = Talented.CardRun.ActiveBattle;
	DamageRuntime.CombatRandomState = FindSeedWhoseSecondRollCrits();
	FGameXXKCardDamageContext Context;
	Context.SourceUnitId = HeroId;
	Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	Context.ResolutionOrigin = EGameXXKCardResolutionOrigin::ActivePlay;
	FGameXXKCardDamageResult Damage;
	TestTrue(TEXT("talented player direct hit resolves"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(
			DamageRuntime,
			Context,
			TEXT("Enemy.Talent.Target.P1"),
			100,
			Damage,
			&Error));
	TestTrue(TEXT("deterministic twenty-percent roll records a critical hit"), Damage.bTalentCriticalHit);
	TestEqual(TEXT("100% final damage plus 200% critical multiplier turns 100 into 400"),
		Damage.HealthDamage,
		400);
	return true;
}

#endif
