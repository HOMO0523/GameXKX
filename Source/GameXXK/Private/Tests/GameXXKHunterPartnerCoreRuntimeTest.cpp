#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHunterPartnerCoreRuntimeTest
{
	const FName HunterUnitId(TEXT("Partner.Hunter"));
	const FName EnemyUnitId(TEXT("Enemy"));

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
		Unit.HP = Side == EGameXXKCardTargetSide::Enemy ? 1000 : 100;
		Unit.MaxHP = Unit.HP;
		Unit.Attack = Attack;
		Unit.Defense = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 20 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
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
		Card.OwnerUnitId = HunterUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("HunterPartner.Core.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildRuntime(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& OutRuntime)
	{
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(TEXT("RuiYi"), TEXT("Profession.Hunter.YingYan"), 0),
			MakeCard(TEXT("LianZhu"), TEXT("Profession.Hunter.LianZhuJian"), 1),
			MakeCard(TEXT("FillerA"), TEXT("Hero.Generic.QingFengYiShi"), 2),
			MakeCard(TEXT("FillerB"), TEXT("Hero.Generic.HeYuZhan"), 3)};
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(HunterUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hunter, 10, 1),
			MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 8, 10)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			61001,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("hunter partner core fixture failed to initialize: %s"), *Error));
			return false;
		}

		OutRuntime.Deck.Hand = {Cards[0], Cards[1]};
		OutRuntime.Deck.DrawPile = {Cards[2], Cards[3]};
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("hunter partner core fixture is invalid: %s"), *Error));
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

	int32 CountDamageCause(
		const TArray<FGameXXKCardDamageResult>& Results,
		const EGameXXKCardDamageCause Cause)
	{
		int32 Count = 0;
		for (const FGameXXKCardDamageResult& Result : Results)
		{
			Count += Result.Cause == Cause ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHunterPartnerCoreCatalogTest,
	"GameXXK.Data.PartnerCards.Hunter.CoreCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterPartnerCoreCatalogTest::RunTest(const FString& Parameters)
{
	const FGameXXKCardDefinition* RuiYi = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Hunter.YingYan"));
	if (!TestNotNull(TEXT("Rui Yi Gan Zhi keeps the stable YingYan CardId"), RuiYi))
	{
		return false;
	}
	TestEqual(TEXT("Rui Yi display name"), RuiYi->DisplayName.ToString(), FString(TEXT("锐意感知")));
	TestTrue(TEXT("Rui Yi is a fixed Hunter core"), RuiYi->bCoreProfessionCard);
	TestEqual(TEXT("Rui Yi costs one Energy"), RuiYi->EnergyCost, 1);
	TestEqual(TEXT("Rui Yi costs no Mana"), RuiYi->ManaCost, 0);
	TestEqual(TEXT("Rui Yi targets self"), RuiYi->TargetSpec.Mode, EGameXXKCardTargetMode::Self);
	TestEqual(TEXT("Rui Yi has four base effects"), RuiYi->Effects.Num(), 4);
	if (RuiYi->Effects.Num() == 4)
	{
		TestEqual(TEXT("Rui Yi draws two"), RuiYi->Effects[0].Type, EGameXXKCardEffectType::DrawCards);
		TestEqual(TEXT("Rui Yi draw amount"), RuiYi->Effects[0].Magnitude, 2);
		TestEqual(TEXT("Rui Yi refunds one Energy"), RuiYi->Effects[1].Type, EGameXXKCardEffectType::GainEnergy);
		TestEqual(TEXT("Rui Yi Energy amount"), RuiYi->Effects[1].Magnitude, 1);
		TestEqual(TEXT("Rui Yi grants Agility"), RuiYi->Effects[2].Status, EGameXXKCardStatus::Agility);
		TestEqual(TEXT("Rui Yi grants two Agility"), RuiYi->Effects[2].Magnitude, 2);
		TestEqual(TEXT("Rui Yi grants Charge"), RuiYi->Effects[3].Status, EGameXXKCardStatus::Charge);
		TestEqual(TEXT("Rui Yi grants three Charge"), RuiYi->Effects[3].Magnitude, 3);
	}

	const FGameXXKCardDefinition* LianZhu = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Hunter.LianZhuJian"));
	if (!TestNotNull(TEXT("Lian Zhu Jian exists"), LianZhu))
	{
		return false;
	}
	TestTrue(TEXT("Lian Zhu Jian is a fixed Hunter core"), LianZhu->bCoreProfessionCard);
	TestEqual(TEXT("Lian Zhu costs one Energy"), LianZhu->EnergyCost, 1);
	TestEqual(TEXT("Lian Zhu costs three Mana"), LianZhu->ManaCost, 3);
	TestEqual(TEXT("Lian Zhu has four ordered base effects"), LianZhu->Effects.Num(), 4);
	if (LianZhu->Effects.Num() == 4)
	{
		TestEqual(TEXT("Lian Zhu first applies Bleed"), LianZhu->Effects[0].Status, EGameXXKCardStatus::Bleed);
		TestEqual(TEXT("Lian Zhu applies Bleed8"), LianZhu->Effects[0].Magnitude, 8);
		TestEqual(TEXT("Lian Zhu then applies Poison"), LianZhu->Effects[1].Status, EGameXXKCardStatus::Poison);
		TestEqual(TEXT("Lian Zhu applies Poison6"), LianZhu->Effects[1].Magnitude, 6);
		TestEqual(TEXT("Lian Zhu base attack type"), LianZhu->Effects[2].Type, EGameXXKCardEffectType::DamagePercentAttack);
		TestEqual(TEXT("Lian Zhu base attack is fifty percent"), LianZhu->Effects[2].Magnitude, 50);
		TestEqual(TEXT("Lian Zhu finally grants Charge"), LianZhu->Effects[3].Status, EGameXXKCardStatus::Charge);
		TestEqual(TEXT("Lian Zhu grants Charge1"), LianZhu->Effects[3].Magnitude, 1);
	}
	TestEqual(TEXT("Lian Zhu Heavy Arrow repeats attacks"), LianZhu->HeavyArrow.Kind, EGameXXKHeavyArrowKind::ExtraAttackPerCharge);
	TestEqual(TEXT("Lian Zhu Heavy Arrow attack percent"), LianZhu->HeavyArrow.MagnitudePerCharge, 50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHunterPartnerCoreLoopTest,
	"GameXXK.Data.PartnerCards.Hunter.CoreLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterPartnerCoreLoopTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHunterPartnerCoreRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime))
	{
		return false;
	}

	FString Error;
	FGameXXKCardPlayResult RuiYiResult;
	if (!TestTrue(TEXT("Rui Yi resolves"), GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("RuiYi"), NAME_None, RuiYiResult, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Rui Yi is Energy-neutral"), Runtime.Deck.SharedEnergy, 10);
	TestEqual(TEXT("Rui Yi draws two cards"), Runtime.Deck.Hand.Num(), 3);
	TestEqual(TEXT("Rui Yi grants Agility2"), Status(Runtime, HunterUnitId, EGameXXKCardStatus::Agility), 2);
	TestEqual(TEXT("Rui Yi grants Charge3"), Status(Runtime, HunterUnitId, EGameXXKCardStatus::Charge), 3);

	const FGameXXKCardCombatUnit* HunterBefore = FindUnit(Runtime, HunterUnitId);
	const FGameXXKCardCombatUnit* EnemyBefore = FindUnit(Runtime, EnemyUnitId);
	if (!TestNotNull(TEXT("Hunter exists before Lian Zhu"), HunterBefore)
		|| !TestNotNull(TEXT("enemy exists before Lian Zhu"), EnemyBefore))
	{
		return false;
	}
	const int32 ManaBefore = HunterBefore->Mana;
	const int32 EnemyHpBefore = EnemyBefore->HP;

	FGameXXKCardPlayResult LianZhuResult;
	Error.Reset();
	if (!TestTrue(TEXT("Lian Zhu resolves"), GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("LianZhu"), EnemyUnitId, LianZhuResult, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Lian Zhu consumes the locked Charge3"), LianZhuResult.HeavyArrowChargeConsumed, 3);
	TestEqual(TEXT("Lian Zhu adds three fifty-percent attacks"), LianZhuResult.HeavyArrowExtraAttackCount, 3);
	TestEqual(TEXT("Lian Zhu leaves its newly granted Charge1"), Status(Runtime, HunterUnitId, EGameXXKCardStatus::Charge), 1);
	TestEqual(TEXT("Lian Zhu spends three Mana"), FindUnit(Runtime, HunterUnitId)->Mana, ManaBefore - 3);
	TestEqual(TEXT("Lian Zhu leaves Poison6"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Poison), 6);
	TestEqual(TEXT("four hits consume Bleed8 down to Bleed4"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Bleed), 4);
	TestEqual(TEXT("Lian Zhu emits four direct attack packets"), CountDamageCause(LianZhuResult.DamageResults, EGameXXKCardDamageCause::DirectAttack), 4);
	TestEqual(TEXT("Lian Zhu emits four Bleed packets"), CountDamageCause(LianZhuResult.DamageResults, EGameXXKCardDamageCause::Bleed), 4);
	TestEqual(TEXT("the full core combo deals forty-six damage"), FindUnit(Runtime, EnemyUnitId)->HP, EnemyHpBefore - 46);
	return true;
}

#endif
