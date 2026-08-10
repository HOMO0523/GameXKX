#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHeroGuardRuntimeTest
{
	const FName HeroUnitId(TEXT("Hero"));
	const FName GuardAUnitId(TEXT("GuardA"));
	const FName GuardBUnitId(TEXT("GuardB"));
	const FName EnemyAUnitId(TEXT("EnemyA"));
	const FName EnemyBUnitId(TEXT("EnemyB"));

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
		Unit.HP = 1000;
		Unit.MaxHP = 1000;
		Unit.Attack = Attack;
		Unit.Defense = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 20 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardCombatUnit> MakeUnits()
	{
		return {
			MakeUnit(HeroUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 10, 1),
			MakeUnit(GuardAUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 12, 2),
			MakeUnit(GuardBUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 18, 3),
			MakeUnit(EnemyAUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 8, 10),
			MakeUnit(EnemyBUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 8, 11)};
	}

	FGameXXKCardInstance MakeCard(const TCHAR* InstanceId, const TCHAR* CardId, const int32 Ordinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(InstanceId);
		Card.CardId = FName(CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = HeroUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Guard.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardInstance>& Cards,
		const int32 Seed)
	{
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			MakeUnits(),
			EGameXXKCardTerrain::Plain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("guard runtime failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = Cards;
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("deterministic guard fixture is invalid: %s"), *Error));
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

	const FGameXXKCardCombatUnit* FindUnit(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	int32 Status(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId, const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Unit = FindUnit(Runtime, UnitId);
		return Unit ? GameXXKCardRules::GetCombatStatusStacks(*Unit, StatusType) : INDEX_NONE;
	}

	bool Resolve(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		const FName TargetUnitId,
		FGameXXKCardPlayResult& OutResult,
		const TCHAR* Context)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, TargetUnitId, OutResult, &Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves: %s"), Context, *Error), bResolved);
		return bResolved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuardExactProtectionTest,
	"GameXXK.Data.HeroCards.Guard.TieBiAndLieZhenGrantExactArmorAndBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardExactProtectionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGuardRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("TieBi"), TEXT("Hero.Guard.TieBiTongShou"), 0),
		MakeCard(TEXT("LieZhen"), TEXT("Hero.Guard.LieZhenChengFeng"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, 53001)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("TieBi"), GuardAUnitId, Result, TEXT("Tie Bi"))) return true;
	TestEqual(TEXT("Tie Bi grants exactly Armor18"), FindUnit(Runtime, GuardAUnitId)->Armor, 18);
	TestEqual(TEXT("Tie Bi registers exactly two Block uses"), Status(Runtime, GuardAUnitId, EGameXXKCardStatus::Block), 2);
	if (!Resolve(*this, Runtime, TEXT("LieZhen"), NAME_None, Result, TEXT("Lie Zhen"))) return true;
	TestEqual(TEXT("Lie Zhen grants Hero Armor8"), FindUnit(Runtime, HeroUnitId)->Armor, 8);
	TestEqual(TEXT("Lie Zhen adds Armor8 to the prior target"), FindUnit(Runtime, GuardAUnitId)->Armor, 26);
	TestEqual(TEXT("Lie Zhen grants GuardB Armor8"), FindUnit(Runtime, GuardBUnitId)->Armor, 8);
	TestEqual(TEXT("Lie Zhen grants Hero Block1"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Block), 1);
	TestEqual(TEXT("Lie Zhen stacks one Block after Tie Bi's two"), Status(Runtime, GuardAUnitId, EGameXXKCardStatus::Block), 3);
	TestEqual(TEXT("Lie Zhen grants GuardB Block1"), Status(Runtime, GuardBUnitId, EGameXXKCardStatus::Block), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuardHighestArmorTieTest,
	"GameXXK.Data.HeroCards.Guard.JieJiaUsesHighestArmorStableTie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardHighestArmorTieTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGuardRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("JieJia"), TEXT("Hero.Guard.JieJiaHuanFeng"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, 53002)) return false;
	FindUnit(Runtime, GuardAUnitId)->Armor = 20;
	FindUnit(Runtime, GuardBUnitId)->Armor = 20;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("JieJia"), EnemyAUnitId, Result, TEXT("Jie Jia stable tie"))) return true;
	TestEqual(TEXT("stable-earlier GuardA receives the follow-up Armor10"), FindUnit(Runtime, GuardAUnitId)->Armor, 30);
	TestEqual(TEXT("stable-later GuardB is not chosen on a tie"), FindUnit(Runtime, GuardBUnitId)->Armor, 20);
	TestEqual(TEXT("stable-earlier GuardA receives Block1"), Status(Runtime, GuardAUnitId, EGameXXKCardStatus::Block), 1);
	TestEqual(TEXT("the packet source is the stable-earlier highest-armor ally"), Result.DamageResults[0].SourceUnitId, GuardAUnitId);
	TestEqual(TEXT("the packet uses GuardA Attack12 plus Armor20"), Result.DamageResults[0].BaseRequestedDamage, 32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuardAttackPlusArmorTest,
	"GameXXK.Data.HeroCards.Guard.JieJiaDealsAttackPlusArmorWithoutConsuming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardAttackPlusArmorTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGuardRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("JieJia"), TEXT("Hero.Guard.JieJiaHuanFeng"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, 53003)) return false;
	FindUnit(Runtime, GuardAUnitId)->Armor = 5;
	FindUnit(Runtime, GuardBUnitId)->Armor = 30;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("JieJia"), EnemyAUnitId, Result, TEXT("Jie Jia conversion"))) return true;
	TestEqual(TEXT("Jie Jia emits one direct packet"), Result.DamageResults.Num(), 1);
	if (Result.DamageResults.Num() == 1)
	{
		TestEqual(TEXT("Jie Jia uses highest ally Attack18 plus Armor30"), Result.DamageResults[0].BaseRequestedDamage, 48);
		TestEqual(TEXT("Jie Jia records GuardB as the actual source"), Result.DamageResults[0].SourceUnitId, GuardBUnitId);
	}
	TestEqual(TEXT("Jie Jia never consumes converted Armor and then grants ten more"), FindUnit(Runtime, GuardBUnitId)->Armor, 40);
	TestEqual(TEXT("Jie Jia grants one Block to the converted source"), Status(Runtime, GuardBUnitId, EGameXXKCardStatus::Block), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuardXuanJiaConsumeTest,
	"GameXXK.Data.HeroCards.Guard.XuanJiaConsumesAllArmorForGroupMultiplier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardXuanJiaConsumeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGuardRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("XuanJia"), TEXT("Hero.Guard.XuanJiaZhenYue"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, 53004)) return false;
	FindUnit(Runtime, GuardAUnitId)->Armor = 4;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("XuanJia"), GuardAUnitId, Result, TEXT("Xuan Jia conversion"))) return true;
	TestEqual(TEXT("Xuan Jia consumes the selected ally's full Armor snapshot once"), FindUnit(Runtime, GuardAUnitId)->Armor, 0);
	TestEqual(TEXT("Xuan Jia emits one group packet per living enemy"), Result.DamageResults.Num(), 2);
	for (const FGameXXKCardDamageResult& DamageResult : Result.DamageResults)
	{
		TestEqual(TEXT("each Xuan Jia packet uses GuardA as source"), DamageResult.SourceUnitId, GuardAUnitId);
		TestEqual(TEXT("Armor4 produces 180% of Attack12, floored to 21"), DamageResult.BaseRequestedDamage, 21);
	}
	TestEqual(TEXT("the first enemy loses 21"), FindUnit(Runtime, EnemyAUnitId)->HP, 979);
	TestEqual(TEXT("the second enemy loses 21"), FindUnit(Runtime, EnemyBUnitId)->HP, 979);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuardXuanJiaZeroArmorTest,
	"GameXXK.Data.HeroCards.Guard.XuanJiaZeroArmorStillDealsBase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardXuanJiaZeroArmorTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGuardRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("XuanJia"), TEXT("Hero.Guard.XuanJiaZhenYue"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, 53005)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("XuanJia"), GuardAUnitId, Result, TEXT("zero-Armor Xuan Jia"))) return true;
	TestEqual(TEXT("zero Armor still emits both group packets"), Result.DamageResults.Num(), 2);
	for (const FGameXXKCardDamageResult& DamageResult : Result.DamageResults)
	{
		TestEqual(TEXT("zero Armor retains the 100% Attack floor"), DamageResult.BaseRequestedDamage, 12);
	}
	TestEqual(TEXT("zero Armor remains zero"), FindUnit(Runtime, GuardAUnitId)->Armor, 0);
	return true;
}

#endif
