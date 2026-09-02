#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHealerPartnerCoreRuntimeTest
{
	const FName HealerId(TEXT("Healer"));
	const FName AllyAId(TEXT("AllyA"));
	const FName AllyBId(TEXT("AllyB"));
	const FName EnemyAId(TEXT("EnemyA"));
	const FName EnemyBId(TEXT("EnemyB"));

	FGameXXKCardCombatUnit MakeUnit(const FName UnitId, const EGameXXKCardTargetSide Side, const int32 Sort)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Side == EGameXXKCardTargetSide::Party ? EGameXXKCharacterRole::Healer : EGameXXKCharacterRole::Invalid;
		Unit.bLiving = true;
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 30 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Attack = 10;
		Unit.Defense = 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = Sort;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(const TCHAR* InstanceId, const TCHAR* CardId, const int32 Ordinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(InstanceId);
		Card.CardId = FName(CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = HealerId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("HealerPartner.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	bool BuildRuntime(FAutomationTestBase& Test, const TArray<FGameXXKCardInstance>& Cards, FGameXXKCardBattleRuntime& OutRuntime)
	{
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(HealerId, EGameXXKCardTargetSide::Party, 1),
			MakeUnit(AllyAId, EGameXXKCardTargetSide::Party, 2),
			MakeUnit(AllyBId, EGameXXKCardTargetSide::Party, 3),
			MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, 10),
			MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, 11)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(OutRuntime, Cards, Units, EGameXXKCardTerrain::Plain, 58101, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Healer partner runtime initializes: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = Cards;
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Healer partner fixture validates: %s"), *Error));
			return false;
		}
		return true;
	}

	FGameXXKCardCombatUnit* Unit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate) { return Candidate.UnitId == UnitId; });
	}

	int32 Status(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId, const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Found = Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate) { return Candidate.UnitId == UnitId; });
		return Found ? GameXXKCardRules::GetCombatStatusStacks(*Found, StatusType) : INDEX_NONE;
	}

	bool Resolve(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime, const FName InstanceId, const FName TargetId, FGameXXKCardPlayResult& OutResult)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, TargetId, OutResult, &Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves: %s"), *InstanceId.ToString(), *Error), bResolved);
		return bResolved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerPartnerYinYangFormulaRuntimeTest,
	"GameXXK.Data.PartnerCards.Healer.Core.YinYangFormulaCostAndFriendlyBranch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerPartnerYinYangFormulaRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHealerPartnerCoreRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("YinYangA"), TEXT("Profession.Healer.YaoYin"), 0),
		MakeCard(TEXT("CaoMu"), TEXT("Profession.Healer.CaoMuFuZhi"), 1),
		MakeCard(TEXT("YinYangB"), TEXT("Profession.Healer.YaoYin"), 2)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Cards, Runtime)) return false;
	Unit(Runtime, AllyAId)->HP = 50;

	FGameXXKCardPlayPreview Preview;
	FString Error;
	TestTrue(FString::Printf(TEXT("unopened Yin-Yang formula previews: %s"), *Error), GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("YinYangA"), Preview, &Error));
	TestEqual(TEXT("the unopened formula adds one Energy"), Preview.EffectiveEnergyCost, 3);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("YinYangA"), AllyAId, Result)) return true;
	TestEqual(TEXT("level-one friendly Yin-Yang continuously scales its coefficient eight to healing nine"), Unit(Runtime, AllyAId)->HP, 59);
	for (const FName EnemyId : {EnemyAId, EnemyBId})
	{
		TestEqual(TEXT("friendly Yin-Yang applies Poison1 to every enemy"), Status(Runtime, EnemyId, EGameXXKCardStatus::Poison), 1);
		TestEqual(TEXT("friendly Yin-Yang applies Burn1 to every enemy"), Status(Runtime, EnemyId, EGameXXKCardStatus::Burn), 1);
	}
	TestTrue(FString::Printf(TEXT("opened duplicate Yin-Yang previews: %s"), *Error), GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("YinYangB"), Preview, &Error));
	TestEqual(TEXT("the formula surcharge disappears for the CardId after first play"), Preview.EffectiveEnergyCost, 2);
	if (!Resolve(*this, Runtime, TEXT("CaoMu"), AllyAId, Result)) return true;
	TestEqual(TEXT("the installed Yin-Yang formula grants one Medicine for the later real heal"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerPartnerYinYangEnemyRuntimeTest,
	"GameXXK.Data.PartnerCards.Healer.Core.YinYangEnemyBranchConsumesMedicineOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerPartnerYinYangEnemyRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHealerPartnerCoreRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("YinYang"), TEXT("Profession.Healer.YaoYin"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Cards, Runtime)) return false;
	for (const FName AllyId : {HealerId, AllyAId, AllyBId}) Unit(Runtime, AllyId)->HP = 50;
	GameXXKCardRules::AddCombatStatus(*Unit(Runtime, HealerId), EGameXXKCardStatus::Medicine, 3);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("YinYang"), EnemyAId, Result)) return true;
	TestEqual(TEXT("the enemy branch resolves Poison2 and Burn2 once each"), Unit(Runtime, EnemyAId)->HP, 96);
	TestEqual(TEXT("Poison2 remains in its reservoir after the explosion"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Poison), 2);
	TestEqual(TEXT("Burn2 remains in its reservoir after the explosion"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Burn), 2);
	for (const FName AllyId : {HealerId, AllyAId, AllyBId})
	{
		TestEqual(TEXT("every ally receives the full level-one scaled 4+Medicine3 snapshot"), Unit(Runtime, AllyId)->HP, 58);
	}
	TestEqual(TEXT("group healing consumes Medicine only once"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerPartnerYinYangLethalEnemyRuntimeTest,
	"GameXXK.Data.PartnerCards.Healer.Core.YinYangLethalExplosionStillHealsParty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerPartnerYinYangLethalEnemyRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHealerPartnerCoreRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("YinYangLethal"), TEXT("Profession.Healer.YaoYin"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Cards, Runtime)) return false;
	for (const FName AllyId : {HealerId, AllyAId, AllyBId}) Unit(Runtime, AllyId)->HP = 50;
	Unit(Runtime, EnemyAId)->HP = 1;
	GameXXKCardRules::AddCombatStatus(*Unit(Runtime, HealerId), EGameXXKCardStatus::Medicine, 3);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("YinYangLethal"), EnemyAId, Result)) return true;
	TestFalse(TEXT("Yin-Yang toxic explosion defeats the selected enemy"), Unit(Runtime, EnemyAId)->bLiving);
	for (const FName AllyId : {HealerId, AllyAId, AllyBId})
	{
		TestEqual(TEXT("lethal enemy branch still heals every ally for the full level-one scaled 4+Medicine3 snapshot"),
			Unit(Runtime, AllyId)->HP, 58);
	}
	TestEqual(TEXT("lethal enemy branch consumes the shared Medicine snapshot once"),
		Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerPartnerYaoWangLethalSideAnchorRuntimeTest,
	"GameXXK.Data.PartnerCards.Healer.Core.YaoWangLethalSideAnchorKeepsResolving",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerPartnerYaoWangLethalSideAnchorRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHealerPartnerCoreRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("YaoWangLethal"), TEXT("Profession.Healer.YaoWangGuiYuan"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Cards, Runtime)) return false;
	Unit(Runtime, EnemyAId)->HP = 1;

	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("YaoWangLethal"), EnemyAId, Result)) return true;
	TestFalse(TEXT("Yao Wang defeats the selected enemy even when it is the side anchor"),
		Unit(Runtime, EnemyAId)->bLiving);
	TestEqual(TEXT("Yao Wang continues the selected-side reverse heal against the other living enemy"),
		Unit(Runtime, EnemyBId)->HP, 93);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerPartnerBloodFormulaRuntimeTest,
	"GameXXK.Data.PartnerCards.Healer.Core.BloodFormulaFirstHighEnergyCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerPartnerBloodFormulaRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHealerPartnerCoreRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Blood"), TEXT("Profession.Healer.XingQiZhen"), 0),
		MakeCard(TEXT("HighCost"), TEXT("Profession.Guard.TieSuoHengJiang"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Cards, Runtime)) return false;
	for (const FName AllyId : {HealerId, AllyAId, AllyBId}) Unit(Runtime, AllyId)->HP = 50;
	FGameXXKCardPlayPreview Preview;
	FString Error;
	TestTrue(FString::Printf(TEXT("unopened blood formula previews: %s"), *Error), GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("Blood"), Preview, &Error));
	TestEqual(TEXT("the unopened blood formula costs three Energy"), Preview.EffectiveEnergyCost, 3);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Blood"), NAME_None, Result)) return true;
	for (const FName AllyId : {HealerId, AllyAId, AllyBId}) TestEqual(TEXT("blood base loses one nonlethal HP"), Unit(Runtime, AllyId)->HP, 49);
	TestEqual(TEXT("blood base grants one Medicine per actual ally loss"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 3);
	if (!Resolve(*this, Runtime, TEXT("HighCost"), NAME_None, Result)) return true;
	for (const FName AllyId : {HealerId, AllyAId, AllyBId}) TestEqual(TEXT("the first paid-2 card resolves fixed lose1 then heal2"), Unit(Runtime, AllyId)->HP, 50);
	TestEqual(TEXT("the formula terminal cycle does not grant itself Medicine"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 3);
	return true;
}

#endif
