#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKCardCombatUnit MakeScalingUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 CombatLevel,
		const int32 Defense,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = 2000;
		Unit.MaxHP = 2000;
		Unit.Attack = 200;
		Unit.Defense = Defense;
		Unit.Speed = 10;
		Unit.StableSortOrder = StableSortOrder;
		Unit.CombatLevel = CombatLevel;
		return Unit;
	}

	TArray<FGameXXKCardInstance> MakeScalingInstances()
	{
		TArray<FGameXXKCardInstance> Instances;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			FGameXXKCardInstance& Instance = Instances.AddDefaulted_GetRef();
			Instance.InstanceId = FName(*FString::Printf(TEXT("Scaling.Instance.%d"), Index));
			Instance.CardId = TEXT("Hero.Generic.QingFengYiShi");
			Instance.CurrentQuality = EGameXXKCardQuality::Common;
			Instance.OwnerUnitId = TEXT("Hero");
			Instance.SourceEntryId = FName(*FString::Printf(TEXT("Scaling.Entry.%d"), Index));
			Instance.AcquisitionOrdinal = Index;
		}
		return Instances;
	}

	TArray<FGameXXKCardCombatUnit> MakeScalingUnits(const int32 EnemyDefense = 100)
	{
		return {
			MakeScalingUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 1),
			MakeScalingUnit(TEXT("Partner"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 80, 80, 2),
			MakeScalingUnit(TEXT("TaskNpc"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 75, 75, 3),
			MakeScalingUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 135, EnemyDefense, 10)
		};
	}

	bool InitializeScalingRuntime(FGameXXKCardBattleRuntime& OutRuntime, const int32 EnemyDefense = 100)
	{
		return GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			MakeScalingInstances(),
			MakeScalingUnits(EnemyDefense),
			EGameXXKCardTerrain::Plain,
			9301);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleScalingIntegrationTest,
	"GameXXK.Data.CardBattleRuntime.Scaling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleScalingIntegrationTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime SnapshotRuntime;
	if (!TestTrue(TEXT("scaling runtime initializes"), InitializeScalingRuntime(SnapshotRuntime)))
	{
		return false;
	}
	TestEqual(TEXT("battle snapshots highest party level"), SnapshotRuntime.TeamMaxLevelSnapshot, 100);
	TestEqual(TEXT("battle defaults enemy difficulty damage to one hundred percent"), SnapshotRuntime.EnemyDifficultyDamagePercent, 100);
	TestEqual(TEXT("battle defaults next-round energy penalty to zero"), SnapshotRuntime.PendingNextRoundEnergyPenalty, 0);

	FGameXXKCardBattleRuntime InvalidRuntime = SnapshotRuntime;
	InvalidRuntime.TeamMaxLevelSnapshot = 0;
	TestFalse(TEXT("team snapshot below one is rejected"), GameXXKCardRules::ValidateCardBattleRuntime(InvalidRuntime));
	InvalidRuntime = SnapshotRuntime;
	InvalidRuntime.EnemyDifficultyDamagePercent = 110;
	TestFalse(TEXT("unsupported difficulty percentage is rejected"), GameXXKCardRules::ValidateCardBattleRuntime(InvalidRuntime));
	InvalidRuntime = SnapshotRuntime;
	InvalidRuntime.PendingNextRoundEnergyPenalty = 100;
	TestFalse(TEXT("energy penalty above the saved bound is rejected"), GameXXKCardRules::ValidateCardBattleRuntime(InvalidRuntime));

	FGameXXKCardBattleRuntime EnemyRuntime;
	if (!TestTrue(TEXT("enemy scaling runtime initializes"), InitializeScalingRuntime(EnemyRuntime)))
	{
		return false;
	}
	EnemyRuntime.Phase = EGameXXKCardBattlePhase::Enemy;
	EnemyRuntime.EnemyDifficultyDamagePercent = 150;
	FGameXXKCardDamageContext EnemyContext;
	EnemyContext.SourceUnitId = TEXT("Enemy");
	EnemyContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardDamageResult EnemyResult;
	TestTrue(TEXT("enemy attack resolves"), GameXXKCardRules::ResolveEnemyDirectAttack(
		EnemyRuntime,
		EnemyContext,
		TEXT("Hero"),
		200,
		EnemyResult));
	TestEqual(TEXT("difficulty is applied before defense"), EnemyResult.DamageAfterDefense, 200);
	TestEqual(TEXT("enemy damage before level difference"), EnemyResult.DamageBeforeLevelDifference, 200);
	TestEqual(TEXT("enemy damage after plus thirty-five levels"), EnemyResult.DamageAfterLevelDifference, 270);
	TestEqual(TEXT("difficulty then defense then level produces 270 health damage"), EnemyResult.HealthDamage, 270);

	FGameXXKCardBattleRuntime PlayerRuntime;
	if (!TestTrue(TEXT("player scaling runtime initializes"), InitializeScalingRuntime(PlayerRuntime)))
	{
		return false;
	}
	FGameXXKCardDamageContext PlayerContext;
	PlayerContext.SourceUnitId = TEXT("Hero");
	PlayerContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	PlayerContext.ResolutionOrigin = EGameXXKCardResolutionOrigin::ActivePlay;
	FGameXXKCardDamageResult PlayerResult;
	TestTrue(TEXT("player attack resolves"), GameXXKCardRules::ApplyPlayerCardDirectDamage(
		PlayerRuntime,
		PlayerContext,
		TEXT("Enemy"),
		200,
		PlayerResult));
	TestEqual(TEXT("player damage after enemy defense"), PlayerResult.DamageBeforeLevelDifference, 100);
	TestEqual(TEXT("player damage after minus thirty-five levels"), PlayerResult.DamageAfterLevelDifference, 65);
	TestEqual(TEXT("player level disadvantage produces 65 health damage"), PlayerResult.HealthDamage, 65);

	FGameXXKCardBattleRuntime FixedRuntime;
	if (!TestTrue(TEXT("fixed scaling runtime initializes"), InitializeScalingRuntime(FixedRuntime, 500)))
	{
		return false;
	}
	FGameXXKCardDamageContext FixedContext;
	FixedContext.SourceUnitId = TEXT("Hero");
	FixedContext.Kind = EGameXXKCardDamageKind::FixedDamage;
	FixedContext.ResolutionOrigin = EGameXXKCardResolutionOrigin::ActivePlay;
	FGameXXKCardDamageResult FixedResult;
	const int32 FixedRandomStateBefore = FixedRuntime.CombatRandomState;
	TestTrue(TEXT("fixed damage resolves"), GameXXKCardRules::ApplyPlayerCardDirectDamage(
		FixedRuntime,
		FixedContext,
		TEXT("Enemy"),
		100,
		FixedResult));
	TestEqual(TEXT("fixed damage bypasses five hundred defense"), FixedResult.DamageBeforeLevelDifference, 100);
	TestEqual(TEXT("fixed damage still receives level difference"), FixedResult.DamageAfterLevelDifference, 65);
	TestEqual(TEXT("fixed packet deals final 65 health damage"), FixedResult.HealthDamage, 65);
	TestEqual(TEXT("fixed packet keeps fixed cause"), FixedResult.Cause, EGameXXKCardDamageCause::FixedDamage);
	TestEqual(TEXT("fixed damage does not consume an Agility roll"), FixedRuntime.CombatRandomState, FixedRandomStateBefore);

	TArray<FGameXXKCardCombatUnit> DotUnits = MakeScalingUnits();
	TArray<FGameXXKCardGuardLinkRuntime> DotGuardLinks;
	TestEqual(TEXT("seed visible poison reservoir"),
		GameXXKCardRules::AddCombatStatus(DotUnits.Last(), EGameXXKCardStatus::Poison, 40),
		40);
	int32 DotDamage = 0;
	TestTrue(TEXT("DOT resolves through dedicated path"), GameXXKCardRules::ApplyCombatEndPhaseDot(
		DotUnits,
		DotGuardLinks,
		TEXT("Enemy"),
		DotDamage));
	TestEqual(TEXT("visible DOT damage is not level-scaled a second time"), DotDamage, 40);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleEnergyPenaltyTest,
	"GameXXK.Data.CardBattleRuntime.Scaling.EnergyPenalty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleEnergyPenaltyTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime Runtime;
	if (!TestTrue(TEXT("energy-penalty runtime initializes"), InitializeScalingRuntime(Runtime)))
	{
		return false;
	}
	TArray<FGameXXKCardDamageResult> EndPhaseDamageResults;
	FString Error;
	if (!TestTrue(
		FString::Printf(TEXT("energy-penalty fixture enters the enemy phase: %s"), *Error),
		GameXXKCardRules::EndPlayerCardPhase(Runtime, EndPhaseDamageResults, &Error)))
	{
		return true;
	}

	TestTrue(TEXT("the first enemy packet queues one point of next-round energy denial"),
		GameXXKCardRules::QueueNextPlayerRoundEnergyPenalty(Runtime, 1, &Error));
	TestTrue(TEXT("a second enemy packet queues two more points in the same phase"),
		GameXXKCardRules::QueueNextPlayerRoundEnergyPenalty(Runtime, 2, &Error));
	TestEqual(TEXT("same-phase energy denial accumulates to three"), Runtime.PendingNextRoundEnergyPenalty, 3);

	TestTrue(TEXT("the separate next-hand surcharge queues one point"),
		GameXXKCardRules::QueueNextPlayerHandEnergySurcharge(Runtime, 1, TEXT("Enemy"), &Error));
	TestTrue(TEXT("a duplicate next-hand surcharge collapses without stacking"),
		GameXXKCardRules::QueueNextPlayerHandEnergySurcharge(Runtime, 1, TEXT("Enemy"), &Error));
	TestEqual(TEXT("hand surcharge remains a separate non-stacking one-point value"), Runtime.PendingNextPlayerHandEnergySurcharge, 1);
	TestEqual(TEXT("queuing a hand surcharge does not alter shared-energy denial"), Runtime.PendingNextRoundEnergyPenalty, 3);

	EndPhaseDamageResults.Reset();
	if (TestTrue(
		FString::Printf(TEXT("the denied player round begins: %s"), *Error),
		GameXXKCardRules::BeginNextPlayerCardRound(Runtime, EndPhaseDamageResults, &Error)))
	{
		TestEqual(TEXT("base three energy minus the accumulated three refills to zero"), Runtime.Deck.SharedEnergy, 0);
		TestEqual(TEXT("the next-round energy denial is consumed once"), Runtime.PendingNextRoundEnergyPenalty, 0);
		TestEqual(TEXT("the pending hand surcharge materializes and clears independently"), Runtime.PendingNextPlayerHandEnergySurcharge, 0);
	}

	EndPhaseDamageResults.Reset();
	if (TestTrue(TEXT("the denied round can end normally"), GameXXKCardRules::EndPlayerCardPhase(Runtime, EndPhaseDamageResults, &Error))
		&& TestTrue(TEXT("the following player round begins normally"), GameXXKCardRules::BeginNextPlayerCardRound(Runtime, EndPhaseDamageResults, &Error)))
	{
		TestEqual(TEXT("the following round refills the normal base three energy"), Runtime.Deck.SharedEnergy, 3);
		TestEqual(TEXT("no energy denial leaks into later rounds"), Runtime.PendingNextRoundEnergyPenalty, 0);
	}
	return true;
}

#endif
