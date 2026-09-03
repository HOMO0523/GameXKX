#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKTaskNpcSupportRuntimeTest
{
	const FName NpcUnitId(TEXT("Npc.JinGui"));
	const FName FirstStrongAllyId(TEXT("Ally.Strong.First"));
	const FName SecondStrongAllyId(TEXT("Ally.Strong.Second"));
	const FName EnemyUnitId(TEXT("Enemy.Primary"));
	const FName OtherEnemyUnitId(TEXT("Enemy.Other"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 Attack,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = Side == EGameXXKCardTargetSide::Enemy ? 500 : 100;
		Unit.MaxHP = Unit.HP;
		Unit.Attack = Attack;
		Unit.Defense = 0;
		Unit.Mana = UnitId == FirstStrongAllyId ? 1 : 20;
		Unit.MaxMana = 20;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardCombatUnit> MakeUnits()
	{
		return {
			MakeUnit(FirstStrongAllyId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hunter, 12, 1),
			MakeUnit(SecondStrongAllyId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 12, 2),
			MakeUnit(NpcUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 5, 3),
			MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 8, 10),
			MakeUnit(OtherEnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 8, 11)};
	}

	FGameXXKCardInstance MakeCard(
		const TCHAR* InstanceId,
		const TCHAR* CardId,
		const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(InstanceId);
		Card.CardId = FName(CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = NpcUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("TaskNpc.Support.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TCHAR* ActiveInstanceId,
		const TCHAR* ActiveCardId,
		const int32 Seed)
	{
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(ActiveInstanceId, ActiveCardId, 0),
			MakeCard(TEXT("Filler.1"), TEXT("Hero.Generic.QingFengYiShi"), 1),
			MakeCard(TEXT("Filler.2"), TEXT("Hero.Generic.HeYuZhan"), 2),
			MakeCard(TEXT("Filler.3"), TEXT("Hero.Generic.SuiYanJi"), 3),
			MakeCard(TEXT("Filler.4"), TEXT("Hero.Generic.JieLiShi"), 4)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			MakeUnits(),
			EGameXXKCardTerrain::Plain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("task-NPC support runtime failed to initialize: %s"), *Error));
			return false;
		}

		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		for (const FGameXXKCardInstance& Card : Cards)
		{
			(Card.InstanceId == FName(ActiveInstanceId) ? OutRuntime.Deck.Hand : OutRuntime.Deck.DrawPile).Add(Card);
		}
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("deterministic task-NPC support fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	FGameXXKCardCombatUnit* FindUnit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	int32 Status(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId, const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Unit = Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
		return Unit ? GameXXKCardRules::GetCombatStatusStacks(*Unit, StatusType) : INDEX_NONE;
	}

	bool Resolve(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		FGameXXKCardPlayResult& OutResult,
		const TCHAR* Context)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, EnemyUnitId, OutResult, &Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves: %s"), Context, *Error), bResolved);
		return bResolved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcHighestAttackAssistTest,
	"GameXXK.Data.TaskNpcCards.Runtime.Support.HighestAttackAllyIsStableAndSuppliesAssistDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcHighestAttackAssistTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcSupportRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, TEXT("ZhaiZhu"), TEXT("Npc.TusiChief.ZhaiZhuHaoLing"), 58101)) return false;
	FindUnit(Runtime, NpcUnitId)->Defense = 20;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("ZhaiZhu"), Result, TEXT("寨主号令"))) return true;

	TestEqual(TEXT("stable first tied highest-Attack ally gains Momentum1"), Status(Runtime, FirstStrongAllyId, EGameXXKCardStatus::Momentum), 1);
	TestEqual(TEXT("stable first tied highest-Attack ally gains Armor8"), FindUnit(Runtime, FirstStrongAllyId)->Armor, 8);
	TestEqual(TEXT("second tied ally receives no Momentum"), Status(Runtime, SecondStrongAllyId, EGameXXKCardStatus::Momentum), 0);
	TestEqual(TEXT("second tied ally receives no Armor"), FindUnit(Runtime, SecondStrongAllyId)->Armor, 0);
	TestEqual(TEXT("assist emits one direct packet"), Result.DamageResults.Num(), 1);
	if (Result.DamageResults.Num() == 1)
	{
		TestEqual(TEXT("assist packet source is the locked highest-Attack ally"), Result.DamageResults[0].SourceUnitId, FirstStrongAllyId);
		TestEqual(TEXT("assist base packet uses 100% of ally Attack12"), Result.DamageResults[0].BaseRequestedDamage, 12);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcHeavyArrowAllyChargeTest,
	"GameXXK.Data.TaskNpcCards.Runtime.Support.HeavyArrowConsumesAlliedChargeAndUsesAlliedAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcHeavyArrowAllyChargeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcSupportRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, TEXT("ShiJing"), TEXT("Npc.JinGui.ShiJingErMu"), 58102)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("ShiJing"), Result, TEXT("市井耳目"))) return true;

	TestEqual(TEXT("市井耳目 consumes the two Charge it just granted to the highest-Attack ally"), Result.HeavyArrowChargeConsumed, 2);
	TestEqual(TEXT("市井耳目 appends two Heavy Arrow attacks"), Result.HeavyArrowExtraAttackCount, 2);
	TestEqual(TEXT("市井耳目 emits exactly two damage packets"), Result.DamageResults.Num(), 2);
	for (int32 Index = 0; Index < Result.DamageResults.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("Heavy Arrow packet %d uses the ally as source"), Index), Result.DamageResults[Index].SourceUnitId, FirstStrongAllyId);
		TestEqual(FString::Printf(TEXT("Heavy Arrow packet %d uses 50%% of Attack12"), Index), Result.DamageResults[Index].BaseRequestedDamage, 6);
	}
	TestEqual(TEXT("highest-Attack ally Charge is empty after Heavy Arrow"), Status(Runtime, FirstStrongAllyId, EGameXXKCardStatus::Charge), 0);
	TestEqual(TEXT("NPC owner never receives or consumes Charge"), Status(Runtime, NpcUnitId, EGameXXKCardStatus::Charge), 0);
	TestEqual(TEXT("two live hits consume the selected enemy's two new Mark stacks"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("the untouched enemy keeps its two group-applied Mark stacks"), Status(Runtime, OtherEnemyUnitId, EGameXXKCardStatus::Mark), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcHeavyArrowManaLoopTest,
	"GameXXK.Data.TaskNpcCards.Runtime.Support.HeavyArrowRestoresManaToItsAlliedChargeOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcHeavyArrowManaLoopTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcSupportRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, TEXT("ZaYi"), TEXT("Npc.JinGui.ZaYiChouBei"), 58103)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("ZaYi"), Result, TEXT("杂役筹备"))) return true;

	TestEqual(TEXT("杂役筹备 consumes Charge3 from the highest-Attack ally"), Result.HeavyArrowChargeConsumed, 3);
	TestEqual(TEXT("杂役筹备 appends three Heavy Arrow attacks"), Result.HeavyArrowExtraAttackCount, 3);
	TestEqual(TEXT("each consumed Charge restores one Mana to that ally"), FindUnit(Runtime, FirstStrongAllyId)->Mana, 4);
	TestEqual(TEXT("the NPC owner pays its own Mana3"), FindUnit(Runtime, NpcUnitId)->Mana, 17);
	TestEqual(TEXT("gain-one-energy closes the one-energy card loop"), Runtime.Deck.SharedEnergy, 10);
	TestTrue(TEXT("draw-three/discard-one opens a one-card discard choice"), Result.bOpenedPendingChoice);
	TestEqual(TEXT("discard choice requests exactly one card"), Runtime.Deck.PendingChoice.RequiredDiscardCount, 1);
	TestEqual(TEXT("杂役筹备 emits exactly three damage packets"), Result.DamageResults.Num(), 3);
	for (int32 Index = 0; Index < Result.DamageResults.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("mana-loop packet %d uses the ally as source"), Index), Result.DamageResults[Index].SourceUnitId, FirstStrongAllyId);
		TestEqual(FString::Printf(TEXT("mana-loop packet %d uses floor(40%% of Attack12)"), Index), Result.DamageResults[Index].BaseRequestedDamage, 4);
	}
	return true;
}

#endif
