#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatStatusUnlimitedCapacityTest,
	"GameXXK.Data.CombatStatusRedesign.Capacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatStatusTerminalPrecedenceTest,
	"GameXXK.Data.CombatStatusRedesign.TerminalPrecedence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	FGameXXKCardCombatUnit MakeStatusCapacityUnit()
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = TEXT("Status.Capacity.Unit");
		Unit.Side = EGameXXKCardTargetSide::Party;
		Unit.Role = EGameXXKCharacterRole::Hero;
		Unit.bLiving = true;
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.Attack = 20;
		Unit.Defense = 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = 1;
		return Unit;
	}

	FGameXXKCardCombatUnit MakeTerminalUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const bool bLiving,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit = MakeStatusCapacityUnit();
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Side == EGameXXKCardTargetSide::Party
			? EGameXXKCharacterRole::Hero
			: EGameXXKCharacterRole::Invalid;
		Unit.bLiving = bLiving;
		Unit.HP = bLiving ? 100 : 0;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}
}

bool FGameXXKCombatStatusUnlimitedCapacityTest::RunTest(const FString& Parameters)
{
	const TArray<EGameXXKCardStatus> UnlimitedStatuses = {
		EGameXXKCardStatus::Momentum,
		EGameXXKCardStatus::Bleed,
		EGameXXKCardStatus::Poison,
		EGameXXKCardStatus::Burn,
		EGameXXKCardStatus::DamageOverTime};

	for (const EGameXXKCardStatus Status : UnlimitedStatuses)
	{
		FGameXXKCardCombatUnit Unit = MakeStatusCapacityUnit();
		TestEqual(
			TEXT("an approved unlimited status accepts a large application"),
			GameXXKCardRules::AddCombatStatus(Unit, Status, 250),
			250);
		TestEqual(
			TEXT("an approved unlimited status preserves the exact large stack count"),
			GameXXKCardRules::GetCombatStatusStacks(Unit, Status),
			250);
	}

	FGameXXKCardCombatUnit CappedUnit = MakeStatusCapacityUnit();
	TestEqual(TEXT("Mark retains its existing five-stack cap"),
		GameXXKCardRules::AddCombatStatus(CappedUnit, EGameXXKCardStatus::Mark, 250), 5);
	TestEqual(TEXT("Weak retains its existing five-stack cap"),
		GameXXKCardRules::AddCombatStatus(CappedUnit, EGameXXKCardStatus::Weak, 250), 5);
	TestEqual(TEXT("Agility retains its existing two-stack cap"),
		GameXXKCardRules::AddCombatStatus(CappedUnit, EGameXXKCardStatus::Agility, 250), 2);
	return true;
}

bool FGameXXKCombatStatusTerminalPrecedenceTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime SimultaneousDefeat;
	SimultaneousDefeat.Phase = EGameXXKCardBattlePhase::Enemy;
	SimultaneousDefeat.PendingNextPlayerHandEnergySurcharge = 2;
	SimultaneousDefeat.PendingNextPlayerHandEnergySurchargeSourceUnitId = TEXT("Enemy");
	SimultaneousDefeat.Units = {
		MakeTerminalUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, false, 1),
		MakeTerminalUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, false, 10)};
	GameXXKCardRules::RefreshCombatTerminalPhase(SimultaneousDefeat);
	TestEqual(TEXT("simultaneous party and enemy elimination resolves as player victory"),
		SimultaneousDefeat.Phase, EGameXXKCardBattlePhase::Victory);
	TestEqual(TEXT("a terminal transition clears a pending hand surcharge"),
		SimultaneousDefeat.PendingNextPlayerHandEnergySurcharge, 0);
	TestTrue(TEXT("a terminal transition clears the pending surcharge source"),
		SimultaneousDefeat.PendingNextPlayerHandEnergySurchargeSourceUnitId.IsNone());

	FGameXXKCardBattleRuntime PartyOnlyDefeat;
	PartyOnlyDefeat.Phase = EGameXXKCardBattlePhase::Enemy;
	PartyOnlyDefeat.Units = {
		MakeTerminalUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, false, 1),
		MakeTerminalUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, true, 10)};
	GameXXKCardRules::RefreshCombatTerminalPhase(PartyOnlyDefeat);
	TestEqual(TEXT("party elimination with a living enemy remains defeat"),
		PartyOnlyDefeat.Phase, EGameXXKCardBattlePhase::Defeat);

	FGameXXKCardBattleRuntime EnemyOnlyDefeat;
	EnemyOnlyDefeat.Phase = EGameXXKCardBattlePhase::Player;
	EnemyOnlyDefeat.Units = {
		MakeTerminalUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, true, 1),
		MakeTerminalUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, false, 10)};
	GameXXKCardRules::RefreshCombatTerminalPhase(EnemyOnlyDefeat);
	TestEqual(TEXT("enemy elimination with a living party remains victory"),
		EnemyOnlyDefeat.Phase, EGameXXKCardBattlePhase::Victory);
	return true;
}

#endif
