#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHealerPartnerEnemyPhaseFormulaTest
{
	const FName HealerId(TEXT("Healer"));
	const FName AllyId(TEXT("Ally"));
	const FName EnemyId(TEXT("Enemy"));

	FGameXXKCardCombatUnit MakeUnit(const FName UnitId, const EGameXXKCardTargetSide Side, const int32 Sort)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Side == EGameXXKCardTargetSide::Party ? EGameXXKCharacterRole::Healer : EGameXXKCharacterRole::Invalid;
		Unit.bLiving = true;
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 20 : 0;
		Unit.MaxMana = Side == EGameXXKCardTargetSide::Party ? 30 : 0;
		Unit.Attack = 10;
		Unit.Defense = 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = Sort;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(const int32 Index)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(*FString::Printf(TEXT("HealerEnemyEvent.Card.%d"), Index));
		Card.CardId = TEXT("Hero.Generic.QingFengYiShi");
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = HealerId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("HealerEnemyEvent.Source.%d"), Index));
		Card.AcquisitionOrdinal = Index;
		return Card;
	}

	FGameXXKCardCombatUnit* Unit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
	}

	int32 Status(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId, const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Found = Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
		return Found ? GameXXKCardRules::GetCombatStatusStacks(*Found, StatusType) : INDEX_NONE;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		const TArray<FName>& FormulaCardIds,
		FGameXXKCardBattleRuntime& OutRuntime)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			Cards.Add(MakeCard(Index));
		}
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(HealerId, EGameXXKCardTargetSide::Party, 1),
			MakeUnit(AllyId, EGameXXKCardTargetSide::Party, 2),
			MakeUnit(EnemyId, EGameXXKCardTargetSide::Enemy, 10)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(OutRuntime, Cards, Units, EGameXXKCardTerrain::Plain, 60211, &Error))
		{
			Test.AddError(FString::Printf(TEXT("enemy-event fixture initializes: %s"), *Error));
			return false;
		}
		for (const FName FormulaCardId : FormulaCardIds)
		{
			const FGameXXKCardDefinition* FormulaDefinition = FGameXXKCardCatalog::FindCardDefinition(FormulaCardId);
			if (!FormulaDefinition || FormulaDefinition->HealerRule.FormulaKind == EGameXXKHealerFormulaKind::None)
			{
				Test.AddError(FString::Printf(TEXT("formula source is missing: %s"), *FormulaCardId.ToString()));
				return false;
			}
			FGameXXKHealerFormulaRuntime& Formula = OutRuntime.HealerFormulas.AddDefaulted_GetRef();
			Formula.OwnerUnitId = HealerId;
			Formula.SourceCardId = FormulaDefinition->Id;
			Formula.Kind = FormulaDefinition->HealerRule.FormulaKind;
		}
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("enemy-event fixture validates: %s"), *Error));
			return false;
		}
		return true;
	}

	bool EnterEnemyPhase(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FGameXXKCardDamageResult> DamageResults;
		FString Error;
		return Test.TestTrue(
			FString::Printf(TEXT("the fixture enters the enemy phase: %s"), *Error),
			GameXXKCardRules::EndPlayerCardPhase(Runtime, DamageResults, &Error));
	}

	bool ResolveEnemyHit(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const int32 Damage,
		const TCHAR* Label)
	{
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = EnemyId;
		Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		FGameXXKCardDamageResult Result;
		FString Error;
		return Test.TestTrue(
			FString::Printf(TEXT("%s resolves: %s"), Label, *Error),
			GameXXKCardRules::ResolveEnemyDirectAttack(Runtime, Context, HealerId, Damage, Result, nullptr, &Error, true));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerEnemyDirectDamageFormulaTest,
	"GameXXK.Data.PartnerCards.Healer.EnemyEvents.DirectDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerEnemyDirectDamageFormulaTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHealerPartnerEnemyPhaseFormulaTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, {
		TEXT("Profession.Healer.YaoYin"),
		TEXT("Profession.Healer.LingZhiXuMing"),
		TEXT("Profession.Healer.JinChuangXuMing")}, Runtime)
		|| !EnterEnemyPhase(*this, Runtime))
	{
		return true;
	}
	Unit(Runtime, HealerId)->HP = 35;
	if (ResolveEnemyHit(*this, Runtime, 6, TEXT("the crossing hit")))
	{
		TestEqual(TEXT("one health change plus the 35-percent crossing grants Medicine4"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 4);
		TestEqual(TEXT("the same hit crossing below 30 percent grants Agility2"), Status(Runtime, HealerId, EGameXXKCardStatus::Agility), 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerEnemyDotFormulaTest,
	"GameXXK.Data.PartnerCards.Healer.EnemyEvents.EndPhasePoison",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerEnemyDotFormulaTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHealerPartnerEnemyPhaseFormulaTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, {
		TEXT("Profession.Healer.YaoYin"),
		TEXT("Profession.Healer.BaiCaoDu")}, Runtime)
		|| !EnterEnemyPhase(*this, Runtime))
	{
		return true;
	}
	GameXXKCardRules::AddCombatStatus(*Unit(Runtime, EnemyId), EGameXXKCardStatus::Poison, 4);
	TArray<FGameXXKCardDamageResult> DamageResults;
	FString Error;
	if (TestTrue(
		FString::Printf(TEXT("enemy-side end-phase Poison resolves: %s"), *Error),
		GameXXKCardRules::BeginNextPlayerCardRound(Runtime, DamageResults, &Error)))
	{
		TestEqual(TEXT("the real Poison packet feeds both health-change and Poison formulas"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 2);
		TestEqual(TEXT("Poison loses one layer after its end-phase packet"), Status(Runtime, EnemyId, EGameXXKCardStatus::Poison), 3);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerEnemyMedicineEnergyQueueTest,
	"GameXXK.Data.PartnerCards.Healer.EnemyEvents.SixMedicineDeferredEnergy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerEnemyMedicineEnergyQueueTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHealerPartnerEnemyPhaseFormulaTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, {
		TEXT("Profession.Healer.YaoYin"),
		TEXT("Profession.Healer.XingQiZhen")}, Runtime)
		|| !EnterEnemyPhase(*this, Runtime))
	{
		return true;
	}
	for (int32 HitIndex = 0; HitIndex < 6; ++HitIndex)
	{
		if (!ResolveEnemyHit(*this, Runtime, 1, TEXT("a one-damage enemy hit")))
		{
			return true;
		}
	}
	TestEqual(TEXT("six enemy-phase health changes grant Medicine6"), Status(Runtime, HealerId, EGameXXKCardStatus::Medicine), 6);
	TestEqual(TEXT("each cumulative Medicine6 grants Momentum1"), Status(Runtime, HealerId, EGameXXKCardStatus::Momentum), 1);
	TestEqual(TEXT("enemy-phase Medicine6 queues one next-round Energy"), Runtime.PendingNextRoundEnergyBonus, 1);

	TArray<FGameXXKCardDamageResult> DamageResults;
	FString Error;
	if (TestTrue(
		FString::Printf(TEXT("the queued-energy player round begins: %s"), *Error),
		GameXXKCardRules::BeginNextPlayerCardRound(Runtime, DamageResults, &Error)))
	{
		TestEqual(TEXT("the next player round starts with Energy4"), Runtime.Deck.SharedEnergy, 4);
		TestEqual(TEXT("the deferred Energy queue is consumed once"), Runtime.PendingNextRoundEnergyBonus, 0);
	}
	return true;
}

#endif
