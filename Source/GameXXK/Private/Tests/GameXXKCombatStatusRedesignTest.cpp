#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatStatusUnlimitedCapacityTest,
	"GameXXK.Data.CombatStatusRedesign.Capacity",
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

#endif
