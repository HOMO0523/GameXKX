#include "GameXXKCardRules.h"

#include "GameXXKEnemyCatalog.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKCardCombatUnit MechanicsUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 HP,
		const int32 Attack,
		const int32 Defense,
		const int32 Order,
		const FName EnemyDefinitionId = NAME_None)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Side == EGameXXKCardTargetSide::Party ? EGameXXKCharacterRole::Hero : EGameXXKCharacterRole::Invalid;
		Unit.HP = HP;
		Unit.MaxHP = HP;
		Unit.Attack = Attack;
		Unit.Defense = Defense;
		Unit.MaxMana = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		Unit.Mana = Unit.MaxMana;
		Unit.Speed = 10;
		Unit.StableSortOrder = Order;
		Unit.BattleSlotNumber = Side == EGameXXKCardTargetSide::Enemy ? Order : INDEX_NONE;
		Unit.CombatLevel = 100;
		Unit.EnemyDefinitionId = EnemyDefinitionId;
		Unit.bLiving = true;
		return Unit;
	}

	TArray<FGameXXKCardInstance> MechanicsCards(const FName CardId)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
			Card.InstanceId = FName(*FString::Printf(TEXT("Mechanics.Card.%d"), Index));
			Card.CardId = CardId;
			Card.OwnerUnitId = TEXT("Hero");
			Card.SourceEntryId = FName(*FString::Printf(TEXT("Mechanics.Source.%d"), Index));
			Card.AcquisitionOrdinal = Index;
		}
		return Cards;
	}

	bool MechanicsRuntime(
		FGameXXKCardBattleRuntime& Runtime,
		const FName EnemyDefinitionId,
		const FName CardId,
		FString& Error,
		const int32 DifficultyPercent = 100,
		const int32 EnemyHP = 1000,
		const int32 EnemyAttack = 10)
	{
		TArray<FGameXXKCardCombatUnit> Units = {
			MechanicsUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 1000, 100, 0, 0),
			MechanicsUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EnemyHP, EnemyAttack, 0, 1, EnemyDefinitionId)};
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			Runtime,
			MechanicsCards(CardId),
			Units,
			EGameXXKCardTerrain::Plain,
			7711,
			&Error,
			DifficultyPercent))
		{
			return false;
		}
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(EnemyDefinitionId);
		if (!Definition)
		{
			return false;
		}
		FGameXXKEnemyBattleState& State = Runtime.EnemyStates.Add(TEXT("Enemy"));
		State.DefinitionId = EnemyDefinitionId;
		State.CurrentPhase = 1;
		State.TotalPhases = FGameXXKEnemyCatalog::ResolveTotalPhases(Definition->Tier, Runtime.EnemyDifficulty);
		Runtime.Deck.SharedEnergy = 99;
		return true;
	}

	FGameXXKCardCombatUnit& EnemyUnit(FGameXXKCardBattleRuntime& Runtime)
	{
		return *Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == TEXT("Enemy");
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKApprovedIronfeatherPassiveTest,
	"GameXXK.Battle.EnemyMechanics.Approved.IronfeatherFirstCardPerPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKApprovedIronfeatherPassiveTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime Runtime;
	FString Error;
	if (!TestTrue(TEXT("Ironfeather fixture initializes"), MechanicsRuntime(
		Runtime,
		TEXT("Enemy.Ch1.IronfeatherRooster"),
		TEXT("Hero.Generic.HeYuZhan"),
		Error,
		125)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKCardDamageContext Context;
	Context.SourceUnitId = TEXT("Hero");
	Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	Context.ResolutionOrigin = EGameXXKCardResolutionOrigin::ActivePlay;
	FGameXXKCardDamageResult First;
	FGameXXKCardDamageResult Second;
	TestTrue(TEXT("first direct card packet resolves"), GameXXKCardRules::ApplyPlayerCardDirectDamage(Runtime, Context, TEXT("Enemy"), 100, First, &Error));
	TestEqual(TEXT("first positive HP packet is halved"), First.HealthDamage, 50);
	TestTrue(TEXT("second direct card packet resolves"), GameXXKCardRules::ApplyPlayerCardDirectDamage(Runtime, Context, TEXT("Enemy"), 100, Second, &Error));
	TestEqual(TEXT("later card in the phase is not halved"), Second.HealthDamage, 100);

	FGameXXKEnemyBattleState& State = Runtime.EnemyStates.FindChecked(TEXT("Enemy"));
	State.TotalPhases = 2;
	EnemyUnit(Runtime).HP = 2;
	FGameXXKCardDamageResult Transition;
	TestTrue(TEXT("lethal packet enters phase two"), GameXXKCardRules::ApplyPlayerCardDirectDamage(Runtime, Context, TEXT("Enemy"), 100, Transition, &Error));
	TestTrue(TEXT("phase transition is reported"), Transition.bTriggeredEnemyPhase);
	TestTrue(TEXT("phase transition rearms first-card reduction"), Runtime.EnemyStates.FindChecked(TEXT("Enemy")).bFirstHitPassiveAvailable);
	FGameXXKCardDamageResult NewPhaseFirst;
	TestTrue(TEXT("first packet of phase two resolves"), GameXXKCardRules::ApplyPlayerCardDirectDamage(Runtime, Context, TEXT("Enemy"), 100, NewPhaseFirst, &Error));
	TestEqual(TEXT("phase two first packet is halved again"), NewPhaseFirst.HealthDamage, 50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKApprovedRedtuskAndPorcupineTest,
	"GameXXK.Battle.EnemyMechanics.Approved.ActiveCardRageAndPorcupineBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKApprovedRedtuskAndPorcupineTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKCardBattleRuntime Redtusk;
	if (!TestTrue(TEXT("Redtusk fixture initializes"), MechanicsRuntime(
		Redtusk,
		TEXT("Enemy.Ch2.RedtuskBoarKing"),
		TEXT("Hero.Generic.HeYuZhan"),
		Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKCardPlayResult RageResult;
	if (!TestTrue(TEXT("one active attack card resolves"), GameXXKCardRules::ResolveCardPlay(
		Redtusk,
		Redtusk.Deck.Hand[0].InstanceId,
		TEXT("Enemy"),
		RageResult,
		&Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("one active card grants one Rage"), GameXXKCardRules::GetCombatStatusStacks(EnemyUnit(Redtusk), EGameXXKCardStatus::Rage), 1);

	FGameXXKCardBattleRuntime Porcupine;
	if (!TestTrue(TEXT("Porcupine fixture initializes"), MechanicsRuntime(
		Porcupine,
		TEXT("Enemy.Ch2.Porcupine"),
		TEXT("Hero.Generic.HeYuZhan"),
		Error,
		100,
		1000,
		10)))
	{
		AddError(Error);
		return false;
	}
	EnemyUnit(Porcupine).Armor = 30;
	GameXXKCardRules::AddCombatStatus(EnemyUnit(Porcupine), EGameXXKCardStatus::Block, 1);
	FGameXXKCardPlayResult BlockResult;
	if (!TestTrue(TEXT("single-target active card resolves into Porcupine Block"), GameXXKCardRules::ResolveCardPlay(
		Porcupine,
		Porcupine.Deck.Hand[0].InstanceId,
		TEXT("Enemy"),
		BlockResult,
		&Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Porcupine consumes one Block"), GameXXKCardRules::GetCombatStatusStacks(EnemyUnit(Porcupine), EGameXXKCardStatus::Block), 0);
	const FGameXXKCardDamageResult* BlockDamage = BlockResult.DamageResults.FindByPredicate([](const FGameXXKCardDamageResult& Damage)
	{
		return Damage.Cause == EGameXXKCardDamageCause::Block;
	});
	TestNotNull(TEXT("Porcupine produces one post-card Block packet"), BlockDamage);
	TestEqual(TEXT("Block uses Porcupine Attack plus post-card remaining Armor"), BlockDamage ? BlockDamage->RequestedDamage : 0, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKApprovedBlackBearPassiveTest,
	"GameXXK.Battle.EnemyMechanics.Approved.BlackBearThickHideNoStatMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKApprovedBlackBearPassiveTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime Runtime;
	FString Error;
	if (!TestTrue(TEXT("Black Bear fixture initializes"), MechanicsRuntime(
		Runtime,
		TEXT("Enemy.Ch2.BlackBear"),
		TEXT("Hero.Generic.HeYuZhan"),
		Error,
		150)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKCardDamageContext Context;
	Context.SourceUnitId = TEXT("Hero");
	Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	Context.ResolutionOrigin = EGameXXKCardResolutionOrigin::ActivePlay;
	FGameXXKCardDamageResult Result;
	TestTrue(TEXT("direct player-card damage resolves"), GameXXKCardRules::ApplyPlayerCardDirectDamage(Runtime, Context, TEXT("Enemy"), 100, Result, &Error));
	TestEqual(TEXT("Thick Hide reduces post-armor health damage by fifteen percent"), Result.HealthDamage, 85);
	FGameXXKEnemyBattleState& State = Runtime.EnemyStates.FindChecked(TEXT("Enemy"));
	State.TotalPhases = 3;
	EnemyUnit(Runtime).HP = 2;
	const int32 AttackBefore = EnemyUnit(Runtime).Attack;
	const int32 DefenseBefore = EnemyUnit(Runtime).Defense;
	TestTrue(TEXT("Black Bear enters another phase"), GameXXKCardRules::ApplyPlayerCardDirectDamage(Runtime, Context, TEXT("Enemy"), 100, Result, &Error));
	TestEqual(TEXT("phase transition never changes Attack"), EnemyUnit(Runtime).Attack, AttackBefore);
	TestEqual(TEXT("phase transition never changes Defense"), EnemyUnit(Runtime).Defense, DefenseBefore);
	return true;
}

#endif
