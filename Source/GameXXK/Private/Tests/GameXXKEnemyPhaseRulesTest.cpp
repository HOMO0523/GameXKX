#include "GameXXKCardTypes.h"
#include "GameXXKCardRules.h"
#include "GameXXKEnemyCatalog.h"
#include "GameXXKEnemyPhaseRules.h"
#include "GameXXKEnemyTypes.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKCardCombatUnit MakePhaseUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 MaxHP,
		const int32 StableSortOrder,
		const FName EnemyDefinitionId = NAME_None)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Side == EGameXXKCardTargetSide::Party
			? EGameXXKCharacterRole::Hero
			: EGameXXKCharacterRole::Invalid;
		Unit.MaxHP = MaxHP;
		Unit.HP = MaxHP;
		Unit.Attack = Side == EGameXXKCardTargetSide::Enemy ? 77 : 30;
		Unit.Defense = Side == EGameXXKCardTargetSide::Enemy ? 33 : 20;
		Unit.Speed = 10;
		Unit.StableSortOrder = StableSortOrder;
		Unit.BattleSlotNumber = Side == EGameXXKCardTargetSide::Enemy ? StableSortOrder : INDEX_NONE;
		Unit.CombatLevel = 100;
		Unit.EnemyDefinitionId = EnemyDefinitionId;
		Unit.bLiving = true;
		return Unit;
	}

	FGameXXKCardBattleRuntime MakePhaseRuntime(const int32 TotalPhases = 3)
	{
		FGameXXKCardBattleRuntime Runtime;
		Runtime.Phase = EGameXXKCardBattlePhase::Player;
		Runtime.RoundNumber = 1;
		Runtime.TeamMaxLevelSnapshot = 100;
		Runtime.EnemyDifficulty = TotalPhases == 3
			? EGameXXKEnemyDifficulty::Hell
			: TotalPhases == 2
				? EGameXXKEnemyDifficulty::Hard
				: EGameXXKEnemyDifficulty::Normal;
		Runtime.EnemyDifficultyDamagePercent = TotalPhases == 3 ? 150 : TotalPhases == 2 ? 125 : 100;
		Runtime.Units = {
			MakePhaseUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 500, 0),
			MakePhaseUnit(TEXT("Ironfeather"), EGameXXKCardTargetSide::Enemy, 100, 1, TEXT("Enemy.Ch1.IronfeatherRooster"))};
		FGameXXKEnemyBattleState& State = Runtime.EnemyStates.Add(TEXT("Ironfeather"));
		State.DefinitionId = TEXT("Enemy.Ch1.IronfeatherRooster");
		State.CurrentPhase = 1;
		State.TotalPhases = TotalPhases;
		return Runtime;
	}

	bool InitializeIntegratedPhaseRuntime(
		FGameXXKCardBattleRuntime& OutRuntime,
		const int32 TotalPhases,
		FString& OutError)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
			Card.InstanceId = FName(*FString::Printf(TEXT("Phase.Card.%d"), Index));
			Card.CardId = TEXT("Hero.Generic.HeYuZhan");
			Card.OwnerUnitId = TEXT("Hero");
			Card.SourceEntryId = FName(*FString::Printf(TEXT("Phase.Source.%d"), Index));
			Card.AcquisitionOrdinal = Index;
		}
		TArray<FGameXXKCardCombatUnit> Units = {
			MakePhaseUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 500, 0),
			MakePhaseUnit(TEXT("Ironfeather"), EGameXXKCardTargetSide::Enemy, 100, 1, TEXT("Enemy.Ch1.IronfeatherRooster"))};
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			4401,
			&OutError,
			TotalPhases == 3 ? 150 : TotalPhases == 2 ? 125 : 100))
		{
			return false;
		}
		FGameXXKEnemyBattleState& State = OutRuntime.EnemyStates.Add(TEXT("Ironfeather"));
		State.DefinitionId = TEXT("Enemy.Ch1.IronfeatherRooster");
		State.CurrentPhase = 1;
		State.TotalPhases = TotalPhases;
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyPhaseDataContractTest,
	"GameXXK.Battle.EnemyPhase.DataContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyPhaseDataContractTest::RunTest(const FString& Parameters)
{
	FGameXXKEnemyDifficultyInt Values;
	Values.Normal = 11;
	Values.Hard = 22;
	Values.Hell = 33;
	TestEqual(TEXT("Normal difficulty value resolves"), Values.Resolve(EGameXXKEnemyDifficulty::Normal), 11);
	TestEqual(TEXT("Hard difficulty value resolves"), Values.Resolve(EGameXXKEnemyDifficulty::Hard), 22);
	TestEqual(TEXT("Hell difficulty value resolves"), Values.Resolve(EGameXXKEnemyDifficulty::Hell), 33);

	TestEqual(TEXT("Normal Boss has one phase"),
		FGameXXKEnemyCatalog::ResolveTotalPhases(EGameXXKEnemyTier::Boss, EGameXXKEnemyDifficulty::Normal), 1);
	TestEqual(TEXT("Hard Elite has two phases"),
		FGameXXKEnemyCatalog::ResolveTotalPhases(EGameXXKEnemyTier::Elite, EGameXXKEnemyDifficulty::Hard), 2);
	TestEqual(TEXT("Hell Elite has three phases"),
		FGameXXKEnemyCatalog::ResolveTotalPhases(EGameXXKEnemyTier::Elite, EGameXXKEnemyDifficulty::Hell), 3);
	TestEqual(TEXT("Hell ordinary enemy remains one phase"),
		FGameXXKEnemyCatalog::ResolveTotalPhases(EGameXXKEnemyTier::Normal, EGameXXKEnemyDifficulty::Hell), 1);

	FGameXXKEnemyBattleState State;
	TestEqual(TEXT("enemy starts at phase one"), State.CurrentPhase, 1);
	TestEqual(TEXT("enemy defaults to one total phase"), State.TotalPhases, 1);
	TestEqual(TEXT("phase transition serial starts at zero"), State.PhaseTransitionSerial, 0);

	FGameXXKCardBattleRuntime Runtime;
	Runtime.EnemyDifficulty = EGameXXKEnemyDifficulty::Hard;
	Runtime.EnemyDifficultyDamagePercent = 125;
	TestEqual(TEXT("battle stores explicit enemy difficulty"), Runtime.EnemyDifficulty, EGameXXKEnemyDifficulty::Hard);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyPhasePacketTransitionTest,
	"GameXXK.Battle.EnemyPhase.Transition.PacketSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyPhasePacketTransitionTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime Runtime = MakePhaseRuntime();
	FGameXXKCardCombatUnit& Enemy = Runtime.Units[1];
	FGameXXKEnemyBattleState& State = Runtime.EnemyStates.FindChecked(Enemy.UnitId);
	Enemy.Armor = 37;
	Enemy.Statuses = {
		{EGameXXKCardStatus::Poison, 20},
		{EGameXXKCardStatus::Bleed, 15},
		{EGameXXKCardStatus::Burn, 10},
		{EGameXXKCardStatus::DamageOverTime, 5},
		{EGameXXKCardStatus::Weak, 2},
		{EGameXXKCardStatus::Vulnerability, 3},
		{EGameXXKCardStatus::Mark, 4},
		{EGameXXKCardStatus::Wealth, 4},
		{EGameXXKCardStatus::Rage, 2},
		{EGameXXKCardStatus::Agility, 1}};
	State.IntentCursor = 3;
	State.PendingChargedIntentId = TEXT("OldCharge");
	State.ChargeRoundsRemaining = 1;
	State.PendingChargeTargetUnitIds = {TEXT("Hero")};
	State.bFirstHitPassiveAvailable = false;
	State.bFirstStatusPassiveAvailable = false;
	State.PhasePassiveTriggerCount = 2;
	Runtime.LockedEnemyIntents = {
		{TEXT("Ironfeather"), TEXT("OldCharge"), 1, 1},
		{TEXT("Other"), TEXT("OtherIntent"), 1, 1}};

	TestEqual(TEXT("lethal packet is clamped to leave the phase at one health"),
		FGameXXKEnemyPhaseRules::ClampHealthDamageForRemainingPhase(Runtime, Enemy, 500), 99);
	Enemy.HP = 1;
	FGameXXKEnemyPhaseTransitionResult Transition;
	FString Error;
	if (!TestTrue(TEXT("the completed packet enters the next phase"),
		FGameXXKEnemyPhaseRules::ResolveAfterDamagePacket(Runtime, Enemy.UnitId, Transition, &Error)))
	{
		AddError(Error);
		return false;
	}

	const FGameXXKCardCombatUnit& After = Runtime.Units[1];
	const FGameXXKEnemyBattleState& AfterState = Runtime.EnemyStates.FindChecked(After.UnitId);
	TestTrue(TEXT("transition result is explicit"), Transition.bTransitioned);
	TestEqual(TEXT("transition records old phase"), Transition.PreviousPhase, 1);
	TestEqual(TEXT("transition records new phase"), Transition.NewPhase, 2);
	TestEqual(TEXT("phase increments"), AfterState.CurrentPhase, 2);
	TestEqual(TEXT("transition serial increments"), AfterState.PhaseTransitionSerial, 1);
	TestEqual(TEXT("next phase fully heals"), After.HP, After.MaxHP);
	TestEqual(TEXT("transition reports the exact healing"), Transition.Healing, 99);
	TestEqual(TEXT("armor is retained"), After.Armor, 37);
	TestEqual(TEXT("attack is unchanged"), After.Attack, 77);
	TestEqual(TEXT("defense is unchanged"), After.Defense, 33);
	TestEqual(TEXT("speed is unchanged"), After.Speed, 10);
	TestEqual(TEXT("wealth is retained"), GameXXKCardRules::GetCombatStatusStacks(After, EGameXXKCardStatus::Wealth), 4);
	TestEqual(TEXT("rage is retained"), GameXXKCardRules::GetCombatStatusStacks(After, EGameXXKCardStatus::Rage), 2);
	TestEqual(TEXT("agility is retained"), GameXXKCardRules::GetCombatStatusStacks(After, EGameXXKCardStatus::Agility), 1);
	for (const EGameXXKCardStatus Negative : {
		EGameXXKCardStatus::Poison,
		EGameXXKCardStatus::Bleed,
		EGameXXKCardStatus::Burn,
		EGameXXKCardStatus::DamageOverTime,
		EGameXXKCardStatus::Weak,
		EGameXXKCardStatus::Vulnerability,
		EGameXXKCardStatus::Mark})
	{
		TestEqual(FString::Printf(TEXT("negative status %d is cleared"), static_cast<int32>(Negative)),
			GameXXKCardRules::GetCombatStatusStacks(After, Negative), 0);
	}
	TestEqual(TEXT("new deck starts at its first card"), AfterState.IntentCursor, 0);
	TestTrue(TEXT("old charge is cancelled"), AfterState.PendingChargedIntentId.IsNone());
	TestEqual(TEXT("charge countdown is cleared"), AfterState.ChargeRoundsRemaining, 0);
	TestTrue(TEXT("locked charge targets are cleared"), AfterState.PendingChargeTargetUnitIds.IsEmpty());
	TestTrue(TEXT("first-hit passive rearms"), AfterState.bFirstHitPassiveAvailable);
	TestTrue(TEXT("first-status passive rearms"), AfterState.bFirstStatusPassiveAvailable);
	TestEqual(TEXT("phase passive budget resets"), AfterState.PhasePassiveTriggerCount, 0);
	TestEqual(TEXT("only the transitioned source lock is removed"), Runtime.LockedEnemyIntents.Num(), 1);
	TestEqual(TEXT("another source lock remains"), Runtime.LockedEnemyIntents[0].SourceUnitId, FName(TEXT("Other")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyPhaseBoundaryTest,
	"GameXXK.Battle.EnemyPhase.Transition.OnePercentBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyPhaseBoundaryTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKEnemyPhaseTransitionResult Transition;
	FGameXXKCardBattleRuntime Runtime = MakePhaseRuntime(2);
	FGameXXKCardCombatUnit& Enemy = Runtime.Units[1];
	Enemy.MaxHP = 250;
	Enemy.HP = 3;
	TestTrue(TEXT("a packet above the integer one-percent boundary is valid"),
		FGameXXKEnemyPhaseRules::ResolveAfterDamagePacket(Runtime, Enemy.UnitId, Transition, &Error));
	TestFalse(TEXT("three of 250 does not transition"), Transition.bTransitioned);
	Enemy.HP = 2;
	TestTrue(TEXT("the exact floor one-percent boundary resolves"),
		FGameXXKEnemyPhaseRules::ResolveAfterDamagePacket(Runtime, Enemy.UnitId, Transition, &Error));
	TestTrue(TEXT("two of 250 transitions"), Transition.bTransitioned);
	TestEqual(TEXT("the second phase is active"), Runtime.EnemyStates.FindChecked(Enemy.UnitId).CurrentPhase, 2);

	Enemy.HP = 0;
	Enemy.bLiving = false;
	Transition = FGameXXKEnemyPhaseTransitionResult();
	TestTrue(TEXT("final-phase defeat resolves without another phase"),
		FGameXXKEnemyPhaseRules::ResolveAfterDamagePacket(Runtime, Enemy.UnitId, Transition, &Error));
	TestFalse(TEXT("no token remains"), Transition.bTransitioned);
	TestEqual(TEXT("final-phase enemy remains defeated"), Enemy.HP, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyPhaseDamageIntegrationTest,
	"GameXXK.Battle.EnemyPhase.Transition.DirectDotAndMultiPacket",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyPhaseDamageIntegrationTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKCardBattleRuntime Runtime;
	if (!TestTrue(TEXT("the integrated phase fixture initializes"),
		InitializeIntegratedPhaseRuntime(Runtime, 3, Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKCardDamageContext Context;
	Context.SourceUnitId = TEXT("Hero");
	Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	Context.ResolutionOrigin = EGameXXKCardResolutionOrigin::ActivePlay;
	for (int32 ExpectedPhase = 2; ExpectedPhase <= 3; ++ExpectedPhase)
	{
		FGameXXKCardDamageResult Result;
		if (!TestTrue(FString::Printf(TEXT("lethal packet enters phase %d"), ExpectedPhase),
			GameXXKCardRules::ApplyPlayerCardDirectDamage(
				Runtime,
				Context,
				TEXT("Ironfeather"),
				1000,
				Result,
				&Error)))
		{
			AddError(Error);
			return false;
		}
		TestEqual(TEXT("one packet cannot overflow into the new health bar"), Result.HealthDamage, 99);
		TestTrue(TEXT("the damage audit reports a transition"), Result.bTriggeredEnemyPhase);
		TestEqual(TEXT("the damage audit reports the destination phase"), Result.EnemyPhaseAfter, ExpectedPhase);
		TestEqual(TEXT("the new bar is full before a later packet"), Runtime.Units[1].HP, 100);
	}
	FGameXXKCardDamageResult FinalResult;
	TestTrue(TEXT("a later packet can defeat the final phase"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(
			Runtime,
			Context,
			TEXT("Ironfeather"),
			1000,
			FinalResult,
			&Error));
	TestFalse(TEXT("the final health bar creates no extra phase"), FinalResult.bTriggeredEnemyPhase);
	TestEqual(TEXT("the final health bar is depleted"), Runtime.Units[1].HP, 0);

	FGameXXKCardBattleRuntime DotRuntime;
	if (!TestTrue(TEXT("the DOT phase fixture initializes"),
		InitializeIntegratedPhaseRuntime(DotRuntime, 2, Error)))
	{
		AddError(Error);
		return false;
	}
	DotRuntime.Units[1].HP = 3;
	TestEqual(TEXT("the DOT fixture adds poison"),
		GameXXKCardRules::AddCombatStatus(DotRuntime.Units[1], EGameXXKCardStatus::Poison, 10), 10);
	TArray<FGameXXKCardDamageResult> DotResults;
	if (!TestTrue(TEXT("a triggered DOT packet resolves through the shared phase boundary"),
		GameXXKCardRules::ResolveToxicExplosion(
			DotRuntime,
			TEXT("Hero"),
			TEXT("Ironfeather"),
			false,
			DotResults,
			&Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestEqual(TEXT("one poison packet is audited"), DotResults.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("DOT clamps to the remaining old phase health"), DotResults[0].HealthDamage, 2);
	TestTrue(TEXT("DOT reports the phase transition"), DotResults[0].bTriggeredEnemyPhase);
	TestEqual(TEXT("DOT enters phase two"), DotRuntime.EnemyStates.FindChecked(TEXT("Ironfeather")).CurrentPhase, 2);
	TestEqual(TEXT("DOT transition heals the new phase"), DotRuntime.Units[1].HP, 100);
	TestEqual(TEXT("DOT transition clears poison"),
		GameXXKCardRules::GetCombatStatusStacks(DotRuntime.Units[1], EGameXXKCardStatus::Poison), 0);
	return true;
}

#endif
