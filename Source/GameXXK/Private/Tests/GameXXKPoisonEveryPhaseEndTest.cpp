#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace GameXXKPoisonEveryPhaseEndTest
{
	FGameXXKCardCombatUnit MakeUnit(const TCHAR* Id, EGameXXKCardTargetSide Side, EGameXXKCharacterRole Role, int32 Order)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = Id;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = Unit.MaxHP = 100;
		Unit.Attack = 20;
		Unit.Defense = 500;
		Unit.CombatLevel = 100;
		Unit.Mana = Unit.MaxMana = Side == EGameXXKCardTargetSide::Party ? 30 : 0;
		Unit.StableSortOrder = Order;
		return Unit;
	}
	FGameXXKCardCombatUnit& Unit(FGameXXKCardBattleRuntime& Runtime, const TCHAR* Id)
	{
		return *Runtime.Units.FindByPredicate([Id](const FGameXXKCardCombatUnit& Entry) { return Entry.UnitId == FName(Id); });
	}
	int32 Status(FGameXXKCardBattleRuntime& Runtime, const TCHAR* Id, EGameXXKCardStatus Kind)
	{
		return GameXXKCardRules::GetCombatStatusStacks(Unit(Runtime, Id), Kind);
	}
	bool Build(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
			Card.InstanceId = FName(*FString::Printf(TEXT("Poison.Card.%d"), Index));
			Card.SourceEntryId = Card.InstanceId;
			Card.CardId = TEXT("Hero.Generic.QingFengYiShi");
			Card.OwnerUnitId = TEXT("Hero");
			Card.CurrentQuality = EGameXXKCardQuality::Common;
			Card.AcquisitionOrdinal = Index;
		}
		FString Error;
		return Test.TestTrue(TEXT("poison boundary fixture initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
			Runtime, Cards, {MakeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 1),
			MakeUnit(TEXT("Support"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 2),
			MakeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)},
			EGameXXKCardTerrain::Plain, 6090304, &Error));
	}
	bool End(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime, bool bPlayer, TArray<FGameXXKCardDamageResult>& Results)
	{
		FString Error;
		const bool bSuccess = bPlayer ? GameXXKCardRules::EndPlayerCardPhase(Runtime, Results, &Error)
			: GameXXKCardRules::BeginNextPlayerCardRound(Runtime, Results, &Error);
		return Test.TestTrue(FString::Printf(TEXT("phase boundary resolves: %s"), *Error), bSuccess);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPoisonEveryPhaseEndTest,
	"GameXXK.Data.CombatStatusRedesign.PoisonEveryPhaseEnd", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKPoisonEveryPhaseEndTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoisonEveryPhaseEndTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!Build(*this, Runtime)) return false;
	GameXXKCardRules::AddCombatStatus(Unit(Runtime, TEXT("Hero")), EGameXXKCardStatus::Poison, 3);
	GameXXKCardRules::AddCombatStatus(Unit(Runtime, TEXT("Enemy")), EGameXXKCardStatus::Poison, 7);
	for (const TCHAR* Id : {TEXT("Hero"), TEXT("Enemy")})
	{
		Unit(Runtime, Id).Armor = 50;
		GameXXKCardRules::AddCombatStatus(Unit(Runtime, Id), EGameXXKCardStatus::Weak, 2);
	}
	for (int32 Boundary = 0; Boundary < 2; ++Boundary)
	{
		TArray<FGameXXKCardDamageResult> Results;
		if (!End(*this, Runtime, Boundary == 0, Results)) return false;
		TestEqual(TEXT("Hero Poison ticks at either side's end"), Unit(Runtime, TEXT("Hero")).HP, 100 - 3 * (Boundary + 1));
		TestEqual(TEXT("Enemy Poison ticks at either side's end"), Unit(Runtime, TEXT("Enemy")).HP, 100 - 7 * (Boundary + 1));
		TestEqual(TEXT("unpoisoned support is unaffected"), Unit(Runtime, TEXT("Support")).HP, 100);
		TestEqual(TEXT("one auditable Poison packet per poisoned unit"), Results.Num(), 2);
		TestEqual(TEXT("Hero Weak decays only at player end"), Status(Runtime, TEXT("Hero"), EGameXXKCardStatus::Weak), 1);
		TestEqual(TEXT("Enemy Weak decays only at enemy end"), Status(Runtime, TEXT("Enemy"), EGameXXKCardStatus::Weak), Boundary == 0 ? 2 : 1);
		TestEqual(TEXT("Hero Poison is not consumed"), Status(Runtime, TEXT("Hero"), EGameXXKCardStatus::Poison), 3);
		TestEqual(TEXT("Enemy Poison is not consumed"), Status(Runtime, TEXT("Enemy"), EGameXXKCardStatus::Poison), 7);
		for (const FGameXXKCardDamageResult& Result : Results)
		{
			TestEqual(TEXT("Poison is the damage cause"), Result.Cause, EGameXXKCardDamageCause::Poison);
			TestEqual(TEXT("Poison bypasses Armor"), Result.TargetArmorBefore, Result.TargetArmorAfter);
			TestEqual(TEXT("Poison packets do not consume reservoirs"), Result.StatusStacksConsumed, 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPoisonBothSidesLethalTest,
	"GameXXK.Data.CombatStatusRedesign.PoisonBothSidesLethal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKPoisonBothSidesLethalTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoisonEveryPhaseEndTest;
	for (const bool bPlayerEnd : {true, false})
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!Build(*this, Runtime)) return false;
		TArray<FGameXXKCardDamageResult> Results;
		if (!bPlayerEnd && !End(*this, Runtime, true, Results)) return false;
		Unit(Runtime, TEXT("Hero")).HP = 3;
		Unit(Runtime, TEXT("Enemy")).HP = 7;
		GameXXKCardRules::AddCombatStatus(Unit(Runtime, TEXT("Hero")), EGameXXKCardStatus::Poison, 3);
		GameXXKCardRules::AddCombatStatus(Unit(Runtime, TEXT("Enemy")), EGameXXKCardStatus::Poison, 7);
		if (!End(*this, Runtime, bPlayerEnd, Results)) return false;
		TestEqual(TEXT("all simultaneous boundary packets finish before terminal evaluation"), Results.Num(), 2);
		TestEqual(TEXT("Hero lethal Poison resolves"), Unit(Runtime, TEXT("Hero")).HP, 0);
		TestEqual(TEXT("Enemy lethal Poison also resolves"), Unit(Runtime, TEXT("Enemy")).HP, 0);
		TestEqual(TEXT("existing enemy-elimination victory precedence is preserved"), Runtime.Phase, EGameXXKCardBattlePhase::Victory);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPoisonBothSidesFormulaTest,
	"GameXXK.Data.CombatStatusRedesign.PoisonBothSidesFormula", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKPoisonBothSidesFormulaTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoisonEveryPhaseEndTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!Build(*this, Runtime)) return false;
	const FGameXXKCardDefinition* Card = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Healer.BaiCaoDu"));
	if (!TestNotNull(TEXT("Poison formula is available"), Card)) return false;
	FGameXXKHealerFormulaRuntime& Formula = Runtime.HealerFormulas.AddDefaulted_GetRef();
	Formula.SourceCardId = Card->Id;
	Formula.OwnerUnitId = TEXT("Support");
	Formula.Kind = Card->HealerRule.FormulaKind;
	GameXXKCardRules::AddCombatStatus(Unit(Runtime, TEXT("Hero")), EGameXXKCardStatus::Poison, 3);
	GameXXKCardRules::AddCombatStatus(Unit(Runtime, TEXT("Enemy")), EGameXXKCardStatus::Poison, 7);
	for (int32 Boundary = 0; Boundary < 2; ++Boundary)
	{
		TArray<FGameXXKCardDamageResult> Results;
		if (!End(*this, Runtime, Boundary == 0, Results)) return false;
		TestEqual(TEXT("the formula sees each side's real Poison packet once at each boundary"),
			Status(Runtime, TEXT("Support"), EGameXXKCardStatus::Medicine), 2 * (Boundary + 1));
	}
	return true;
}
#endif
