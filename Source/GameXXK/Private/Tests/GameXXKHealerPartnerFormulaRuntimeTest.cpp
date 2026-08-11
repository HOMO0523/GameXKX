#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHealerPartnerFormulaRuntimeTest
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
		Unit.MaxMana = Side == EGameXXKCardTargetSide::Party ? 40 : 0;
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
		Card.SourceEntryId = FName(*FString::Printf(TEXT("HealerFormula.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
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

	bool BuildRuntime(
		FAutomationTestBase& Test,
		const TCHAR* TriggerCardId,
		const TCHAR* FormulaCardId,
		FGameXXKCardBattleRuntime& OutRuntime)
	{
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(TEXT("Trigger"), TriggerCardId, 0),
			MakeCard(TEXT("Reward"), TEXT("Hero.Generic.QingFengYiShi"), 1),
			MakeCard(TEXT("Spare"), TEXT("Hero.Generic.YinLiangZhouZhuan"), 2)};
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(HealerId, EGameXXKCardTargetSide::Party, 1),
			MakeUnit(AllyAId, EGameXXKCardTargetSide::Party, 2),
			MakeUnit(AllyBId, EGameXXKCardTargetSide::Party, 3),
			MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, 10),
			MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, 11)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(OutRuntime, Cards, Units, EGameXXKCardTerrain::Plain, 58201, &Error))
		{
			Test.AddError(FString::Printf(TEXT("formula fixture initializes: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = {Cards[0]};
		OutRuntime.Deck.DrawPile = {Cards[2], Cards[1]};
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 20;
		const FGameXXKCardDefinition* FormulaDefinition = FGameXXKCardCatalog::FindCardDefinition(FName(FormulaCardId));
		if (!FormulaDefinition || FormulaDefinition->HealerRule.FormulaKind == EGameXXKHealerFormulaKind::None)
		{
			Test.AddError(FString::Printf(TEXT("formula source is missing: %s"), FormulaCardId));
			return false;
		}
		FGameXXKHealerFormulaRuntime& Formula = OutRuntime.HealerFormulas.AddDefaulted_GetRef();
		Formula.OwnerUnitId = HealerId;
		Formula.SourceCardId = FormulaDefinition->Id;
		Formula.Kind = FormulaDefinition->HealerRule.FormulaKind;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("formula fixture validates: %s"), *Error));
			return false;
		}
		return true;
	}

	bool Resolve(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime, const FName TargetId, FGameXXKCardPlayResult& OutResult, const TCHAR* Label)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Trigger"), TargetId, OutResult, &Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves: %s"), Label, *Error), bResolved);
		return bResolved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerTreatmentFormulaMatrixTest,
	"GameXXK.Data.PartnerCards.Healer.Formulas.TreatmentEight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerTreatmentFormulaMatrixTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHealerPartnerFormulaRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	FGameXXKCardPlayResult Result;

	if (BuildRuntime(*this, TEXT("Profession.Healer.CaoMuFuZhi"), TEXT("Profession.Healer.CaoMuFuZhi"), Runtime))
	{
		Unit(Runtime, AllyAId)->HP = 50;
		if (Resolve(*this, Runtime, AllyAId, Result, TEXT("first-healing formula")))
			TestEqual(TEXT("first real healing grants Medicine2"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 2);
	}
	if (BuildRuntime(*this, TEXT("Profession.Healer.QingXinSan"), TEXT("Profession.Healer.QingXinSan"), Runtime))
	{
		Unit(Runtime, AllyAId)->HP = 50;
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, AllyAId), EGameXXKCardStatus::Bleed, 1);
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, AllyAId), EGameXXKCardStatus::Poison, 1);
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, AllyAId), EGameXXKCardStatus::Burn, 1);
		if (Resolve(*this, Runtime, AllyAId, Result, TEXT("three-cleanse formula")))
			TestEqual(TEXT("three cleansed DOT layers grant Medicine1"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 1);
	}
	if (BuildRuntime(*this, TEXT("Profession.Healer.XingQiZhen"), TEXT("Profession.Healer.LingZhiXuMing"), Runtime))
	{
		Unit(Runtime, HealerId)->HP = 35;
		if (Resolve(*this, Runtime, NAME_None, Result, TEXT("35-percent crossing formula")))
			TestEqual(TEXT("party loss grants Medicine3 and crossing below 35 percent grants another Medicine3"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 6);
	}
	if (BuildRuntime(*this, TEXT("Hero.Healer.BaiCaoJiZhen"), TEXT("Profession.Healer.HuiChunLu"), Runtime))
	{
		for (const FName AllyId : {HealerId, AllyAId, AllyBId}) Unit(Runtime, AllyId)->HP = 50;
		if (Resolve(*this, Runtime, NAME_None, Result, TEXT("three-effective-heals formula")))
			TestEqual(TEXT("three effective heals draw one card"), Runtime.Deck.Hand.Num(), 1);
	}
	if (BuildRuntime(*this, TEXT("Profession.Healer.QingXinSan"), TEXT("Profession.Healer.ZhiXueCao"), Runtime))
	{
		Unit(Runtime, AllyAId)->HP = 50;
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, AllyAId), EGameXXKCardStatus::Bleed, 1);
		if (Resolve(*this, Runtime, AllyAId, Result, TEXT("bleed-removal formula")))
			for (const FName AllyId : {HealerId, AllyAId, AllyBId}) TestEqual(TEXT("Bleed removal grants party Armor2"), Unit(Runtime, AllyId)->Armor, 2);
	}
	if (BuildRuntime(*this, TEXT("Profession.Healer.CaoMuFuZhi"), TEXT("Profession.Healer.WenYangGao"), Runtime))
	{
		Unit(Runtime, AllyAId)->HP = 50;
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, HealerId), EGameXXKCardStatus::Medicine, 2);
		if (Resolve(*this, Runtime, AllyAId, Result, TEXT("large-healing formula")))
			TestEqual(TEXT("a resolved 10-point heal grants Armor4 to that ally"), Unit(Runtime, AllyAId)->Armor, 4);
	}
	if (BuildRuntime(*this, TEXT("Profession.Healer.XingQiZhen"), TEXT("Profession.Healer.JinChuangXuMing"), Runtime))
	{
		Unit(Runtime, HealerId)->HP = 30;
		if (Resolve(*this, Runtime, NAME_None, Result, TEXT("30-percent crossing formula")))
			TestEqual(TEXT("crossing below 30 percent grants Agility2"), Status(Runtime, HealerId, EGameXXKCardStatus::Agility), 2);
	}
	if (BuildRuntime(*this, TEXT("Hero.Healer.BaiCaoJiZhen"), TEXT("Profession.Healer.YaoWangGuiYuan"), Runtime))
	{
		for (const FName AllyId : {HealerId, AllyAId, AllyBId}) { Unit(Runtime, AllyId)->HP = 50; Unit(Runtime, AllyId)->Mana = 30; }
		if (Resolve(*this, Runtime, NAME_None, Result, TEXT("three-unit health-change formula")))
		{
			TestEqual(TEXT("three changed units draw one card"), Runtime.Deck.Hand.Num(), 1);
			TestEqual(TEXT("the owner pays the trigger card's Mana6 and then gains Mana2"), Unit(Runtime, HealerId)->Mana, 26);
			for (const FName AllyId : {AllyAId, AllyBId}) TestEqual(TEXT("the other allies gain Mana2"), Unit(Runtime, AllyId)->Mana, 32);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerDotFormulaMatrixTest,
	"GameXXK.Data.PartnerCards.Healer.Formulas.DotEight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerDotFormulaMatrixTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHealerPartnerFormulaRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	FGameXXKCardPlayResult Result;

	if (BuildRuntime(*this, TEXT("Profession.Healer.LianQiaoJieDu"), TEXT("Profession.Healer.BaiCaoDu"), Runtime))
	{
		if (Resolve(*this, Runtime, EnemyAId, Result, TEXT("Poison-packet formula")))
			TestEqual(TEXT("one real Poison explosion packet grants Medicine1"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 1);
	}
	if (BuildRuntime(*this, TEXT("Profession.Healer.FuGuSan"), TEXT("Profession.Healer.FuGuSan"), Runtime))
	{
		if (Resolve(*this, Runtime, EnemyAId, Result, TEXT("Bleed-Poison formula")))
		{
			TestEqual(TEXT("the trigger applies Bleed6 and its direct hit consumes one layer"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Bleed), 5);
			TestEqual(TEXT("the trigger applies Poison4"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Poison), 4);
			TestEqual(TEXT("an enemy gaining a debuff with Bleed and Poison gains Mark1"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Mark), 1);
		}
	}
	if (BuildRuntime(*this, TEXT("Profession.Healer.HuiQiXiang"), TEXT("Profession.Healer.HuiQiXiang"), Runtime))
	{
		if (Resolve(*this, Runtime, NAME_None, Result, TEXT("group-Poison formula")))
		{
			TestEqual(TEXT("poisoning two enemies grants Medicine2"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 2);
			TestEqual(TEXT("poisoning two enemies draws one card"), Runtime.Deck.Hand.Num(), 1);
		}
	}
	if (BuildRuntime(*this, TEXT("Profession.Healer.LianQiaoJieDu"), TEXT("Profession.Healer.LianQiaoJieDu"), Runtime))
	{
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, EnemyAId), EGameXXKCardStatus::Bleed, 3);
		if (Resolve(*this, Runtime, EnemyAId, Result, TEXT("dual-DOT explosion formula")))
			TestEqual(TEXT("one dual-DOT explosion grants Medicine2"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 2);
	}
	if (BuildRuntime(*this, TEXT("Profession.Healer.WuWeiTiaoHe"), TEXT("Profession.Healer.YaoJiuWenShen"), Runtime))
	{
		for (const FName EnemyId : {EnemyAId, EnemyBId}) GameXXKCardRules::AddCombatStatus(*Unit(Runtime, EnemyId), EGameXXKCardStatus::Bleed, 2);
		if (Resolve(*this, Runtime, NAME_None, Result, TEXT("two-Bleed-packet formula")))
			TestEqual(TEXT("four real Bleed explosion packets grant Medicine2"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 2);
	}
	if (BuildRuntime(*this, TEXT("Profession.Healer.YaoNangFeiTou"), TEXT("Profession.Healer.YaoNangFeiTou"), Runtime))
	{
		const int32 EnergyBefore = Runtime.Deck.SharedEnergy;
		if (Resolve(*this, Runtime, NAME_None, Result, TEXT("group-direct-damage formula")))
			TestEqual(TEXT("directly damaging two enemies refunds one Energy after paying two"), Runtime.Deck.SharedEnergy, EnergyBefore - 1);
	}
	if (BuildRuntime(*this, TEXT("Profession.Healer.KuShenMaSan"), TEXT("Profession.Healer.KuShenMaSan"), Runtime))
	{
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, EnemyAId), EGameXXKCardStatus::Poison, 1);
		if (Resolve(*this, Runtime, EnemyAId, Result, TEXT("poisoned-Vulnerability formula")))
		{
			TestEqual(TEXT("Vulnerability on an already poisoned enemy grants Medicine1"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 1);
			TestEqual(TEXT("Vulnerability on an already poisoned enemy draws one"), Runtime.Deck.Hand.Num(), 1);
		}
	}
	if (BuildRuntime(*this, TEXT("Profession.Healer.WuWeiTiaoHe"), TEXT("Profession.Healer.WuWeiTiaoHe"), Runtime))
	{
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, EnemyAId), EGameXXKCardStatus::Bleed, 2);
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, EnemyAId), EGameXXKCardStatus::Burn, 2);
		if (Resolve(*this, Runtime, NAME_None, Result, TEXT("triple-DOT explosion formula")))
		{
			TestEqual(TEXT("a triple-DOT explosion grants Momentum1"), Status(Runtime, HealerId, EGameXXKCardStatus::Momentum), 1);
			TestEqual(TEXT("a triple-DOT explosion draws one"), Runtime.Deck.Hand.Num(), 1);
		}
	}
	return true;
}

#endif
