#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMarkDirectDamageRulesTest,
	"GameXXK.Data.MarkRules.DirectDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	FGameXXKCardCombatUnit MakeMarkUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 HP,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.bLiving = true;
		Unit.HP = HP;
		Unit.MaxHP = HP;
		Unit.Attack = 20;
		Unit.Defense = 0;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardCombatUnit* FindMarkUnit(
		TArray<FGameXXKCardCombatUnit>& Units,
		const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	bool ResolveMarkedHit(
		TArray<FGameXXKCardCombatUnit>& Units,
		const int32 RequestedDamage,
		FGameXXKCardDamageResult& OutResult)
	{
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = TEXT("Attacker");
		Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		TArray<FGameXXKCardGuardLinkRuntime> GuardLinks;
		return GameXXKCardRules::ApplyCombatDirectDamage(
			Units, GuardLinks, Context, TEXT("Target"), RequestedDamage, OutResult);
	}
}

bool FGameXXKMarkDirectDamageRulesTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKCardCombatUnit> FiveMarkUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	TestEqual(TEXT("five Mark stacks can be prepared"),
		GameXXKCardRules::AddCombatStatus(FiveMarkUnits[1], EGameXXKCardStatus::Mark, 5), 5);
	FGameXXKCardDamageResult FiveMarkResult;
	TestTrue(TEXT("a five-Mark direct hit resolves"), ResolveMarkedHit(FiveMarkUnits, 100, FiveMarkResult));
	TestEqual(TEXT("five stacks still grant one fixed fifteen-percent hit"), FiveMarkResult.DamageAfterVulnerability, 115);
	TestEqual(TEXT("one hit consumes only one of five Mark stacks"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(FiveMarkUnits, TEXT("Target")), EGameXXKCardStatus::Mark), 4);

	TArray<FGameXXKCardCombatUnit> OneMarkUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	GameXXKCardRules::AddCombatStatus(OneMarkUnits[1], EGameXXKCardStatus::Mark, 1);
	FGameXXKCardDamageResult OneMarkResult;
	TestTrue(TEXT("a one-Mark direct hit resolves"), ResolveMarkedHit(OneMarkUnits, 100, OneMarkResult));
	TestEqual(TEXT("one Mark stack grants the same fixed fifteen percent"), OneMarkResult.DamageAfterVulnerability, 115);
	TestEqual(TEXT("the final Mark stack is consumed"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(OneMarkUnits, TEXT("Target")), EGameXXKCardStatus::Mark), 0);

	TArray<FGameXXKCardCombatUnit> CombinedUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	GameXXKCardRules::AddCombatStatus(CombinedUnits[1], EGameXXKCardStatus::Vulnerability, 2);
	GameXXKCardRules::AddCombatStatus(CombinedUnits[1], EGameXXKCardStatus::Mark, 3);
	FGameXXKCardDamageResult CombinedResult;
	TestTrue(TEXT("Vulnerability and Mark resolve together"), ResolveMarkedHit(CombinedUnits, 100, CombinedResult));
	TestEqual(TEXT("two Vulnerability plus Mark use one additive thirty-five-percent multiplier"),
		CombinedResult.DamageAfterVulnerability, 135);
	FGameXXKCardCombatUnit* CombinedTarget = FindMarkUnit(CombinedUnits, TEXT("Target"));
	TestEqual(TEXT("the hit clears all Vulnerability"),
		GameXXKCardRules::GetCombatStatusStacks(*CombinedTarget, EGameXXKCardStatus::Vulnerability), 0);
	TestEqual(TEXT("the hit removes only one Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*CombinedTarget, EGameXXKCardStatus::Mark), 2);

	return true;
}

#endif
