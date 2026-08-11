#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKBladePartnerMomentumRuntimeTest
{
	constexpr const TCHAR* BladeUnitId = TEXT("BladePartner");
	constexpr const TCHAR* EnemyUnitId = TEXT("Enemy");

	FGameXXKCardCombatUnit MakeUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 HP,
		const int32 Attack,
		const int32 Mana,
		const int32 MaxMana,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = HP > 0;
		Unit.HP = HP;
		Unit.MaxHP = HP;
		Unit.Attack = Attack;
		Unit.Mana = Mana;
		Unit.MaxMana = MaxMana;
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
		Card.OwnerUnitId = BladeUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("MomentumBreak.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildExactRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardInstance>& Cards,
		const int32 EnemyHP = 1000)
	{
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(BladeUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 100, 20, 20, 20, 1),
			MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, EnemyHP, 10, 0, 0, 10)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			59101,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("Momentum Break exact runtime failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = Cards;
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Momentum Break exact fixture is invalid: %s"), *Error));
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

	bool EndRoundAndBeginNext(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FGameXXKCardDamageResult> DamageResults;
		FString Error;
		if (!Test.TestTrue(
			FString::Printf(TEXT("Momentum Break player phase ends: %s"), *Error),
			GameXXKCardRules::EndPlayerCardPhase(Runtime, DamageResults, &Error)))
		{
			return false;
		}
		Error.Reset();
		return Test.TestTrue(
			FString::Printf(TEXT("Momentum Break next player round begins: %s"), *Error),
			GameXXKCardRules::BeginNextPlayerCardRound(Runtime, DamageResults, &Error));
	}

	FGameXXKCardInstance* FindHandCardByInstanceId(FGameXXKCardBattleRuntime& Runtime, const FName InstanceId)
	{
		return Runtime.Deck.Hand.FindByPredicate([InstanceId](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == InstanceId;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDuanYueMomentumBaseRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.MomentumBreak.DuanYueCombinesMomentumFlatAndPercent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDuanYueMomentumBaseRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerMomentumRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("DuanYue"), TEXT("Profession.Blade.DuanYue"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Blade = FindUnit(Runtime, BladeUnitId);
	FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, EnemyUnitId);
	TestNotNull(TEXT("the Duan Yue owner exists"), Blade);
	TestNotNull(TEXT("the Duan Yue target exists"), Enemy);
	if (!Blade || !Enemy)
	{
		return true;
	}
	Enemy->Defense = 0;
	TestEqual(TEXT("the fixture grants two Momentum before Duan Yue"),
		GameXXKCardRules::AddCombatStatus(*Blade, EGameXXKCardStatus::Momentum, 2), 2);

	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(
		FString::Printf(TEXT("Duan Yue resolves: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("DuanYue"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Duan Yue emits one direct packet"), Result.DamageResults.Num(), 1);
	if (Result.DamageResults.Num() == 1)
	{
		TestEqual(TEXT("two Momentum add twenty percentage points before the flat bonus"),
			Result.DamageResults[0].BaseRequestedDamage, 32);
		TestEqual(TEXT("the same two Momentum add two flat requested damage"),
			Result.DamageResults[0].RequestedDamage, 34);
		TestEqual(TEXT("the direct resolver audits the two-point flat Momentum bonus"),
			Result.DamageResults[0].MomentumDamageBonus, 2);
	}
	Blade = FindUnit(Runtime, BladeUnitId);
	Enemy = FindUnit(Runtime, EnemyUnitId);
	TestEqual(TEXT("Duan Yue keeps both old Momentum and grants one new layer after the card"),
		Blade ? GameXXKCardRules::GetCombatStatusStacks(*Blade, EGameXXKCardStatus::Momentum) : INDEX_NONE,
		3);
	TestEqual(TEXT("Duan Yue applies exactly three Vulnerability"),
		Enemy ? GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Vulnerability) : INDEX_NONE,
		3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunVulnerabilityExtraHitsRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.MomentumBreak.PoJunConsumesUpToThreeVulnerabilityForHits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunVulnerabilityExtraHitsRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerMomentumRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("PoJun"), TEXT("Profession.Blade.PoJun"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Blade = FindUnit(Runtime, BladeUnitId);
	FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, EnemyUnitId);
	TestNotNull(TEXT("the Po Jun owner exists"), Blade);
	TestNotNull(TEXT("the Po Jun target exists"), Enemy);
	if (!Blade || !Enemy)
	{
		return true;
	}
	Enemy->Defense = 0;
	TestEqual(TEXT("the fixture grants two Momentum before Po Jun"),
		GameXXKCardRules::AddCombatStatus(*Blade, EGameXXKCardStatus::Momentum, 2), 2);
	TestEqual(TEXT("the fixture grants four Vulnerability before Po Jun"),
		GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::Vulnerability, 4), 4);

	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(
		FString::Printf(TEXT("Po Jun resolves: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("PoJun"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Po Jun emits its base hit plus one hit for each of three consumed Vulnerability"),
		Result.DamageResults.Num(), 4);
	if (Result.DamageResults.Num() == 4)
	{
		TestEqual(TEXT("Po Jun base hit includes its two-layer Momentum multiplier"),
			Result.DamageResults[0].BaseRequestedDamage, 30);
		TestEqual(TEXT("Po Jun snapshots all four Vulnerability before its base hit"),
			Result.DamageResults[0].VulnerabilityStacksBeforeHit, 4);
		TestEqual(TEXT("Po Jun explicitly consumes exactly three Vulnerability on its base hit"),
			Result.DamageResults[0].VulnerabilityStacksConsumed, 3);
		for (int32 HitIndex = 1; HitIndex < 4; ++HitIndex)
		{
			TestEqual(
				FString::Printf(TEXT("Po Jun extra hit %d includes the same Momentum multiplier"), HitIndex),
				Result.DamageResults[HitIndex].BaseRequestedDamage,
				14);
			TestEqual(
				FString::Printf(TEXT("Po Jun extra hit %d also receives the two-point flat Momentum bonus"), HitIndex),
				Result.DamageResults[HitIndex].RequestedDamage,
				16);
			TestEqual(
				FString::Printf(TEXT("Po Jun extra hit %d preserves the remaining Vulnerability"), HitIndex),
				Result.DamageResults[HitIndex].VulnerabilityStacksConsumed,
				0);
		}
	}
	Enemy = FindUnit(Runtime, EnemyUnitId);
	TestEqual(TEXT("Po Jun consumes no more than three of four Vulnerability"),
		Enemy ? GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Vulnerability) : INDEX_NONE,
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKZhanYiFeiTengBaseRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.MomentumBreak.ZhanYiFeiTengGrantsMomentumAndMana",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKZhanYiFeiTengBaseRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerMomentumRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("ZhanYi"), TEXT("Profession.Blade.ZhanYiFeiTeng"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Blade = FindUnit(Runtime, BladeUnitId);
	TestNotNull(TEXT("the Zhan Yi owner exists"), Blade);
	if (!Blade)
	{
		return true;
	}
	Blade->Mana = 3;

	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(
		FString::Printf(TEXT("Zhan Yi Fei Teng resolves: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("ZhanYi"), NAME_None, Result, &Error)))
	{
		return true;
	}
	Blade = FindUnit(Runtime, BladeUnitId);
	TestEqual(TEXT("Zhan Yi Fei Teng grants two Momentum"),
		Blade ? GameXXKCardRules::GetCombatStatusStacks(*Blade, EGameXXKCardStatus::Momentum) : INDEX_NONE,
		2);
	TestEqual(TEXT("Zhan Yi Fei Teng restores four Mana"), Blade ? Blade->Mana : INDEX_NONE, 7);
	TestEqual(TEXT("Zhan Yi Fei Teng pays its one Energy"), Runtime.Deck.SharedEnergy, 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKZhanJinKillRefundRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.MomentumBreak.ZhanJinKillRefundsCostsAndDraws",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKZhanJinKillRefundRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerMomentumRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("ZhanJin"), TEXT("Profession.Blade.ZhanJin"), 0),
		MakeCard(TEXT("DrawA"), TEXT("Profession.Blade.HuiFengJiaShi"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards, 40))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Blade = FindUnit(Runtime, BladeUnitId);
	FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, EnemyUnitId);
	TestNotNull(TEXT("the Zhan Jin owner exists"), Blade);
	TestNotNull(TEXT("the Zhan Jin target exists"), Enemy);
	if (!Blade || !Enemy)
	{
		return true;
	}
	Enemy->Defense = 0;
	TestEqual(TEXT("the fixture grants two Momentum before Zhan Jin"),
		GameXXKCardRules::AddCombatStatus(*Blade, EGameXXKCardStatus::Momentum, 2), 2);
	const int32 DrawIndex = Runtime.Deck.Hand.IndexOfByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == TEXT("DrawA");
	});
	if (!TestTrue(TEXT("the Zhan Jin draw fixture contains its draw card"), DrawIndex != INDEX_NONE))
	{
		return true;
	}
	Runtime.Deck.DrawPile.Add(MoveTemp(Runtime.Deck.Hand[DrawIndex]));
	Runtime.Deck.Hand.RemoveAt(DrawIndex, 1, EAllowShrinking::No);

	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(
		FString::Printf(TEXT("Zhan Jin resolves a killing hit: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("ZhanJin"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Zhan Jin emits one killing packet"), Result.DamageResults.Num(), 1);
	if (Result.DamageResults.Num() == 1)
	{
		TestEqual(TEXT("Zhan Jin combines two-layer Momentum multiplier with its base attack"),
			Result.DamageResults[0].BaseRequestedDamage, 44);
		TestEqual(TEXT("Zhan Jin also receives two flat Momentum damage"),
			Result.DamageResults[0].RequestedDamage, 46);
		TestEqual(TEXT("Zhan Jin completes the kill"), Result.DamageResults[0].TargetHealthAfter, 0);
	}
	Blade = FindUnit(Runtime, BladeUnitId);
	TestEqual(TEXT("a killing Zhan Jin refunds all three Energy"), Runtime.Deck.SharedEnergy, 10);
	TestEqual(TEXT("a killing Zhan Jin refunds all twelve Mana"), Blade ? Blade->Mana : INDEX_NONE, 20);
	TestNotNull(TEXT("a killing Zhan Jin draws exactly the prepared card"),
		Runtime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == TEXT("DrawA");
		}));
	TestEqual(TEXT("the killing active card commits the terminal victory"), Runtime.Phase, EGameXXKCardBattlePhase::Victory);
	TestEqual(TEXT("terminal victory clears the unusable next-active Charge"),
		Runtime.PendingBladeCharge.Rule,
		EGameXXKBladeChargeRule::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDuanYueChargeMakesEnergyFreeRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.MomentumBreak.DuanYueChargeMakesNextActiveEnergyFree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDuanYueChargeMakesEnergyFreeRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerMomentumRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.DuanYue"), 0),
		MakeCard(TEXT("Next"), TEXT("Profession.Blade.ZhanJin"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(TEXT("Duan Yue resolves as the first active card"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Charge"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	FGameXXKCardPlayPreview Preview;
	Error.Reset();
	if (!TestTrue(TEXT("the next active card remains previewable under Duan Yue Charge"),
		GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("Next"), Preview, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Duan Yue Charge makes only the next active Energy cost zero"), Preview.EffectiveEnergyCost, 0);
	TestEqual(TEXT("Duan Yue Charge leaves the next active Mana cost unchanged"), Preview.EffectiveManaCost, 12);
	const int32 EnergyBefore = Runtime.Deck.SharedEnergy;
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("the Energy-free next active card resolves"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Next"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("the Energy-free next active card pays no Energy"), Runtime.Deck.SharedEnergy, EnergyBefore);
	TestEqual(TEXT("Duan Yue Charge is consumed once"), Runtime.PendingBladeCharge.Rule, EGameXXKBladeChargeRule::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunChargeMakesManaFreeRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.MomentumBreak.PoJunChargeMakesNextActiveManaFree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunChargeMakesManaFreeRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerMomentumRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.PoJun"), 0),
		MakeCard(TEXT("Next"), TEXT("Profession.Blade.ZhanJin"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(TEXT("Po Jun resolves as the first active card"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Charge"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	FGameXXKCardPlayPreview Preview;
	Error.Reset();
	if (!TestTrue(TEXT("the next active card remains previewable under Po Jun Charge"),
		GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("Next"), Preview, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Po Jun Charge leaves the next active Energy cost unchanged"), Preview.EffectiveEnergyCost, 3);
	TestEqual(TEXT("Po Jun Charge makes only the next active Mana cost zero"), Preview.EffectiveManaCost, 0);
	FGameXXKCardCombatUnit* Blade = FindUnit(Runtime, BladeUnitId);
	const int32 ManaBefore = Blade ? Blade->Mana : INDEX_NONE;
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("the Mana-free next active card resolves"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Next"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	Blade = FindUnit(Runtime, BladeUnitId);
	TestEqual(TEXT("the Mana-free next active card pays no Mana"), Blade ? Blade->Mana : INDEX_NONE, ManaBefore);
	TestEqual(TEXT("Po Jun Charge is consumed once"), Runtime.PendingBladeCharge.Rule, EGameXXKBladeChargeRule::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKZhanYiChargeRefundsPaidCostsRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.MomentumBreak.ZhanYiChargePaysThenRefundsNextActiveCosts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKZhanYiChargeRefundsPaidCostsRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerMomentumRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.ZhanYiFeiTeng"), 0),
		MakeCard(TEXT("Next"), TEXT("Profession.Blade.DuanYue"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Blade = FindUnit(Runtime, BladeUnitId);
	TestNotNull(TEXT("the Zhan Yi Charge owner exists"), Blade);
	if (!Blade)
	{
		return true;
	}
	Blade->Mana = 8;
	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(TEXT("Zhan Yi resolves as the first active card"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Charge"), NAME_None, Result, &Error)))
	{
		return true;
	}
	FGameXXKCardPlayPreview Preview;
	Error.Reset();
	if (!TestTrue(TEXT("the next active card remains normally priced under Zhan Yi Charge"),
		GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("Next"), Preview, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Zhan Yi Charge keeps the normal Energy payment visible"), Preview.EffectiveEnergyCost, 2);
	TestEqual(TEXT("Zhan Yi Charge keeps the normal Mana payment visible"), Preview.EffectiveManaCost, 5);
	const int32 EnergyBefore = Runtime.Deck.SharedEnergy;
	Blade = FindUnit(Runtime, BladeUnitId);
	const int32 ManaBefore = Blade ? Blade->Mana : INDEX_NONE;
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("the pay-then-refund next active card resolves"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Next"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	Blade = FindUnit(Runtime, BladeUnitId);
	TestEqual(TEXT("Zhan Yi Charge refunds the paid Energy after resolution"), Runtime.Deck.SharedEnergy, EnergyBefore);
	TestEqual(TEXT("Zhan Yi Charge refunds the paid Mana after resolution"), Blade ? Blade->Mana : INDEX_NONE, ManaBefore);
	TestEqual(TEXT("Zhan Yi Charge is consumed once"), Runtime.PendingBladeCharge.Rule, EGameXXKBladeChargeRule::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKZhanJinChargeCountsNextActiveTwiceRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.MomentumBreak.ZhanJinChargeCountsNextActiveTwice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKZhanJinChargeCountsNextActiveTwiceRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerMomentumRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.ZhanJin"), 0),
		MakeCard(TEXT("Next"), TEXT("Profession.Blade.HuiFengJiaShi"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(TEXT("Zhan Jin resolves as the first non-killing active card"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Charge"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("the next active card resolves inside Zhan Jin Charge"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Next"), NAME_None, Result, &Error)))
	{
		return true;
	}
	const FGameXXKCardCombatUnit* Blade = FindUnit(Runtime, BladeUnitId);
	TestEqual(TEXT("Zhan Jin Charge makes the second active card count as two for card-count thresholds"),
		Runtime.ActiveCardsPlayedThisRound,
		3);
	TestEqual(TEXT("the twice-counted card still resolves its base effect only once"),
		Blade ? GameXXKCardRules::GetCombatStatusStacks(*Blade, EGameXXKCardStatus::Agility) : INDEX_NONE,
		1);
	TestEqual(TEXT("Zhan Jin Charge is consumed once"), Runtime.PendingBladeCharge.Rule, EGameXXKBladeChargeRule::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDuanYueFinishFreezesVulnerabilityAndReplaysRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.MomentumBreak.DuanYueFinishFreezesVulnerabilityAndReplaysFirstTargetingActive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDuanYueFinishFreezesVulnerabilityAndReplaysRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerMomentumRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.DuanYue"), 0),
		MakeCard(TEXT("Trigger"), TEXT("Profession.Blade.LieFengZhan"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, EnemyUnitId);
	if (!TestNotNull(TEXT("the Duan Yue Finish target exists"), Enemy))
	{
		return true;
	}
	Enemy->Defense = 0;
	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(TEXT("Duan Yue resolves as the final active card"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Finisher"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	TArray<FGameXXKCardDamageResult> BoundaryDamage;
	Error.Reset();
	if (!TestTrue(TEXT("Duan Yue Finish ends the player phase"),
		GameXXKCardRules::EndPlayerCardPhase(Runtime, BoundaryDamage, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Duan Yue arms its Vulnerability Finish"),
		Runtime.PendingBladeFinish.Rule,
		EGameXXKBladeFinishRule::FreezeVulnerabilityAndReplay);
	Enemy = FindUnit(Runtime, EnemyUnitId);
	TestEqual(TEXT("the enemy-phase cleanse fixture removes all three Vulnerability"),
		Enemy ? GameXXKCardRules::ConsumeCombatStatus(*Enemy, EGameXXKCardStatus::Vulnerability, 0) : INDEX_NONE,
		3);
	Error.Reset();
	if (!TestTrue(TEXT("the protected next player round begins"),
		GameXXKCardRules::BeginNextPlayerCardRound(Runtime, BoundaryDamage, &Error)))
	{
		return true;
	}
	Enemy = FindUnit(Runtime, EnemyUnitId);
	TestEqual(TEXT("Duan Yue restores enemy-cleared Vulnerability for the protected round"),
		Enemy ? GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Vulnerability) : INDEX_NONE,
		3);
	const FGameXXKCardInstance* Trigger = FindHandCardByInstanceId(Runtime, TEXT("Trigger"));
	if (!TestNotNull(TEXT("the next-round Vulnerability trigger is in hand"), Trigger))
	{
		return true;
	}
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("the first active card targeting Vulnerability resolves"),
		GameXXKCardRules::ResolveCardPlay(Runtime, Trigger->InstanceId, EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Duan Yue Finish replays that active card base exactly once"), Result.AutomaticResolutionCount, 1);
	TestEqual(TEXT("Duan Yue Finish is consumed by the qualifying active card"),
		Runtime.PendingBladeFinish.Rule,
		EGameXXKBladeFinishRule::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunFinishCopiesFirstStatusConsumerRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.MomentumBreak.PoJunFinishCopiesFirstEnemyStatusConsumer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunFinishCopiesFirstStatusConsumerRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerMomentumRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.PoJun"), 0),
		MakeCard(TEXT("Consumer"), TEXT("Profession.Blade.PoJun"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(TEXT("Po Jun resolves as the final active card"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Finisher"), EnemyUnitId, Result, &Error))
		|| !EndRoundAndBeginNext(*this, Runtime))
	{
		return true;
	}
	TestEqual(TEXT("Po Jun Finish enters its next-round status-consumption window"),
		Runtime.PendingBladeFinish.Rule,
		EGameXXKBladeFinishRule::CopyFirstStatusConsumer);
	FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, EnemyUnitId);
	if (!TestNotNull(TEXT("the Po Jun Finish consumer target exists"), Enemy))
	{
		return true;
	}
	TestEqual(TEXT("the consumer fixture grants three Vulnerability"),
		GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::Vulnerability, 3),
		3);
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("Po Jun actively consumes the enemy status"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Consumer"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	const FGameXXKCardInstance* TemporaryCopy = Runtime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.CardId == TEXT("Profession.Blade.PoJun") && Card.bTemporary;
	});
	if (!TestNotNull(TEXT("Po Jun Finish creates the status consumer's temporary copy"), TemporaryCopy))
	{
		return true;
	}
	TestEqual(TEXT("the status-consumer copy costs zero Energy"), TemporaryCopy->EnergyCostOverride, 0);
	TestEqual(TEXT("the status-consumer copy costs zero Mana"), TemporaryCopy->ManaCostOverride, 0);
	TestEqual(TEXT("Po Jun Finish clears after the first status-consuming active card"),
		Runtime.PendingBladeFinish.Rule,
		EGameXXKBladeFinishRule::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKZhanYiFinishRefundsFirstHighCostRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.MomentumBreak.ZhanYiFinishRefundsFirstPrintedHighCostAndDrawsTwo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKZhanYiFinishRefundsFirstHighCostRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerMomentumRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.ZhanYiFeiTeng"), 0),
		MakeCard(TEXT("Low"), TEXT("Profession.Blade.HuiFengJiaShi"), 1),
		MakeCard(TEXT("High"), TEXT("Profession.Blade.DuanYue"), 2),
		MakeCard(TEXT("DrawA"), TEXT("Profession.Blade.LieFengZhan"), 3),
		MakeCard(TEXT("DrawB"), TEXT("Profession.Blade.FengHou"), 4)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(TEXT("Zhan Yi resolves as the final active card"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Finisher"), NAME_None, Result, &Error))
		|| !EndRoundAndBeginNext(*this, Runtime))
	{
		return true;
	}
	TestEqual(TEXT("Zhan Yi Finish enters its next-round window"),
		Runtime.PendingBladeFinish.Rule,
		EGameXXKBladeFinishRule::RefundFirstHighCostAndDrawTwo);
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("a printed one-Energy card does not consume Zhan Yi Finish"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Low"), NAME_None, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Zhan Yi Finish waits through the low-cost active card"),
		Runtime.PendingBladeFinish.Rule,
		EGameXXKBladeFinishRule::RefundFirstHighCostAndDrawTwo);
	for (const FName DrawId : {FName(TEXT("DrawA")), FName(TEXT("DrawB"))})
	{
		const int32 DrawIndex = Runtime.Deck.Hand.IndexOfByPredicate([DrawId](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == DrawId;
		});
		if (!TestTrue(TEXT("the prepared Zhan Yi draw card is in hand before being moved"), DrawIndex != INDEX_NONE))
		{
			return true;
		}
		Runtime.Deck.DrawPile.Add(MoveTemp(Runtime.Deck.Hand[DrawIndex]));
		Runtime.Deck.Hand.RemoveAt(DrawIndex, 1, EAllowShrinking::No);
	}
	const int32 EnergyBeforeHigh = Runtime.Deck.SharedEnergy;
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("the printed two-Energy active card resolves inside Zhan Yi Finish"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("High"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Zhan Yi Finish refunds only the high-cost card Energy"), Runtime.Deck.SharedEnergy, EnergyBeforeHigh);
	TestNotNull(TEXT("Zhan Yi Finish draws the first prepared card"), FindHandCardByInstanceId(Runtime, TEXT("DrawA")));
	TestNotNull(TEXT("Zhan Yi Finish draws the second prepared card"), FindHandCardByInstanceId(Runtime, TEXT("DrawB")));
	TestEqual(TEXT("Zhan Yi Finish clears after the first qualifying card"),
		Runtime.PendingBladeFinish.Rule,
		EGameXXKBladeFinishRule::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKZhanJinFinishCopiesFirstKillRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.MomentumBreak.ZhanJinFinishCopiesFirstKillingActive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKZhanJinFinishCopiesFirstKillRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerMomentumRuntimeTest;
	const FName KillTargetId(TEXT("KillTarget"));
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.ZhanJin"), 0),
		MakeCard(TEXT("NonKill"), TEXT("Profession.Blade.ZhanYiFeiTeng"), 1),
		MakeCard(TEXT("Killer"), TEXT("Profession.Blade.LieFengZhan"), 2)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	Runtime.Units.Add(MakeUnit(TEXT("KillTarget"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 20, 10, 0, 0, 11));
	FString Error;
	if (!TestTrue(TEXT("the two-enemy Zhan Jin Finish fixture validates"),
		GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error)))
	{
		return true;
	}
	FGameXXKCardPlayResult Result;
	Error.Reset();
	if (!TestTrue(TEXT("Zhan Jin resolves as the non-killing final active card"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Finisher"), EnemyUnitId, Result, &Error))
		|| !EndRoundAndBeginNext(*this, Runtime))
	{
		return true;
	}
	TestEqual(TEXT("Zhan Jin Finish enters its next-round kill window"),
		Runtime.PendingBladeFinish.Rule,
		EGameXXKBladeFinishRule::CopyFirstKill);
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("a non-killing active card does not consume Zhan Jin Finish"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("NonKill"), NAME_None, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Zhan Jin Finish waits through a non-killing card"),
		Runtime.PendingBladeFinish.Rule,
		EGameXXKBladeFinishRule::CopyFirstKill);
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("the next active card completes a kill"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Killer"), KillTargetId, Result, &Error)))
	{
		return true;
	}
	const FGameXXKCardInstance* TemporaryCopy = Runtime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.CardId == TEXT("Profession.Blade.LieFengZhan") && Card.bTemporary;
	});
	if (!TestNotNull(TEXT("Zhan Jin Finish creates the killing card's temporary copy"), TemporaryCopy))
	{
		return true;
	}
	TestEqual(TEXT("the killing-card copy costs zero Energy"), TemporaryCopy->EnergyCostOverride, 0);
	TestEqual(TEXT("the killing-card copy costs zero Mana"), TemporaryCopy->ManaCostOverride, 0);
	TestEqual(TEXT("Zhan Jin Finish clears after its first active kill"),
		Runtime.PendingBladeFinish.Rule,
		EGameXXKBladeFinishRule::None);
	return true;
}

#endif
