#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHeroBladeRuntimeTest
{
	const FName HeroUnitId(TEXT("Hero"));
	const FName AllyUnitId(TEXT("Ally"));
	const FName EnemyUnitId(TEXT("Enemy"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 HP,
		const int32 Attack,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = HP > 0;
		Unit.HP = HP;
		Unit.MaxHP = HP;
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
			MakeUnit(HeroUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 10, 1),
			MakeUnit(AllyUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hunter, 100, 8, 2),
			MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1000, 10, 10)};
	}

	FGameXXKCardInstance MakeCard(
		const TCHAR* InstanceId,
		const TCHAR* CardId,
		const FName OwnerUnitId,
		const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(InstanceId);
		Card.CardId = FName(CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Blade.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardInstance>& Cards,
		const TArray<FName>& HandInstanceIds,
		const int32 Seed = 52001)
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
			Test.AddError(FString::Printf(TEXT("blade runtime failed to initialize: %s"), *Error));
			return false;
		}

		TSet<FName> HandIds;
		for (const FName HandInstanceId : HandInstanceIds)
		{
			HandIds.Add(HandInstanceId);
		}
		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		for (const FGameXXKCardInstance& Card : Cards)
		{
			(HandIds.Contains(Card.InstanceId) ? OutRuntime.Deck.Hand : OutRuntime.Deck.DrawPile).Add(Card);
		}
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("deterministic blade fixture is invalid: %s"), *Error));
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

	int32 CountModifiers(
		const FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKCardBattleModifierTrigger Trigger,
		const EGameXXKCardEffectType EffectType = EGameXXKCardEffectType::Invalid)
	{
		int32 Count = 0;
		for (const FGameXXKCardBattleModifierRuntime& Modifier : Runtime.Modifiers)
		{
			Count += Modifier.Definition.Trigger == Trigger
				&& (EffectType == EGameXXKCardEffectType::Invalid || Modifier.Definition.EffectType == EffectType)
				? 1
				: 0;
		}
		return Count;
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

	bool EndRoundAndBeginNext(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime, const TCHAR* Context)
	{
		TArray<FGameXXKCardDamageResult> DamageResults;
		FString Error;
		if (!Test.TestTrue(FString::Printf(TEXT("%s ends the player phase: %s"), Context, *Error),
			GameXXKCardRules::EndPlayerCardPhase(Runtime, DamageResults, &Error)))
		{
			return false;
		}
		Error.Reset();
		if (!Test.TestTrue(FString::Printf(TEXT("%s begins the next player round: %s"), Context, *Error),
			GameXXKCardRules::BeginNextPlayerCardRound(Runtime, DamageResults, &Error)))
		{
			return false;
		}
		return true;
	}

	bool ArrangeHand(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const TArray<FName>& DesiredHandIds,
		const TCHAR* Context)
	{
		TArray<FGameXXKCardInstance> Available;
		Available.Append(MoveTemp(Runtime.Deck.Hand));
		Available.Append(MoveTemp(Runtime.Deck.DrawPile));
		Available.Append(MoveTemp(Runtime.Deck.DiscardPile));
		Runtime.Deck.Hand.Reset();
		Runtime.Deck.DrawPile.Reset();
		Runtime.Deck.DiscardPile.Reset();

		for (const FName DesiredId : DesiredHandIds)
		{
			const int32 Index = Available.IndexOfByPredicate([DesiredId](const FGameXXKCardInstance& Card)
			{
				return Card.InstanceId == DesiredId;
			});
			if (Index == INDEX_NONE)
			{
				Test.AddError(FString::Printf(TEXT("%s cannot find %s outside the exhaust pile"), Context, *DesiredId.ToString()));
				return false;
			}
			Runtime.Deck.Hand.Add(MoveTemp(Available[Index]));
			Available.RemoveAt(Index, 1, EAllowShrinking::No);
		}
		Runtime.Deck.DrawPile = MoveTemp(Available);
		FString Error;
		return Test.TestTrue(FString::Printf(TEXT("%s arranged hand validates: %s"), Context, *Error),
			GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error));
	}

	const FGameXXKCardDamageResult* FindCause(
		const TArray<FGameXXKCardDamageResult>& Results,
		const EGameXXKCardDamageCause Cause)
	{
		return Results.FindByPredicate([Cause](const FGameXXKCardDamageResult& Result)
		{
			return Result.Cause == Cause;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBladeOnlyFirstChargeTest,
	"GameXXK.Data.HeroCards.Blade.OnlyFirstActiveBladeCardTriggersCharge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBladeOnlyFirstChargeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroBladeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("YingFeng"), TEXT("Hero.Blade.YingFengHuanBu"), HeroUnitId, 0),
		MakeCard(TEXT("TongPao"), TEXT("Hero.Blade.TongPaoJuShi"), HeroUnitId, 1),
		MakeCard(TEXT("Attack"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 2)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("YingFeng"), TEXT("TongPao"), TEXT("Attack")})) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("YingFeng"), NAME_None, Result, TEXT("first Blade card"))) return true;
	if (!Resolve(*this, Runtime, TEXT("TongPao"), AllyUnitId, Result, TEXT("second Blade card"))) return true;
	TestEqual(TEXT("Ying Feng base Agility2 plus Charge2 reaches four"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Agility), 4);
	TestEqual(TEXT("Ying Feng base Counter1 plus Charge1 reaches two"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Counter), 2);
	if (!Resolve(*this, Runtime, TEXT("Attack"), EnemyUnitId, Result, TEXT("third active card"))) return true;
	TestEqual(TEXT("the second Blade card does not create another Charge"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Momentum), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBladeChargeAnyOwnerTest,
	"GameXXK.Data.HeroCards.Blade.ChargeAffectsAnyOwnersNextActiveCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBladeChargeAnyOwnerTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroBladeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("TongPao"), TEXT("Hero.Blade.TongPaoJuShi"), HeroUnitId, 0),
		MakeCard(TEXT("AllyCard"), TEXT("Hero.Generic.HengJianShouShi"), AllyUnitId, 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("TongPao"), TEXT("AllyCard")}, 52002)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("TongPao"), HeroUnitId, Result, TEXT("Tong Pao Charge source"))) return true;
	if (!Resolve(*this, Runtime, TEXT("AllyCard"), AllyUnitId, Result, TEXT("different owner's next card"))) return true;
	TestEqual(TEXT("shared-deck Charge grants Momentum2 to a different owner"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::Momentum), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBladeAutomaticReplayBoundaryTest,
	"GameXXK.Data.HeroCards.Blade.AutomaticReplayNeitherConsumesNorCreatesCharge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBladeAutomaticReplayBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroBladeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("YingFeng"), TEXT("Hero.Blade.YingFengHuanBu"), HeroUnitId, 0),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HengJianShouShi"), AllyUnitId, 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("YingFeng"), TEXT("Next")}, 52003)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("YingFeng"), NAME_None, Result, TEXT("Ying Feng source"))) return true;

	FGameXXKResolvedCardSnapshot AutomaticBlade;
	AutomaticBlade.CardId = TEXT("Hero.Blade.TongPaoJuShi");
	AutomaticBlade.Quality = EGameXXKCardQuality::Common;
	AutomaticBlade.OwnerUnitId = HeroUnitId;
	AutomaticBlade.OriginalTargetUnitIds = {AllyUnitId};
	Runtime.AutomaticResolutionQueue.bActive = true;
	Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::AutomaticReplay;
	Runtime.AutomaticResolutionQueue.PendingCards = {AutomaticBlade};
	Runtime.AutomaticResolutionQueue.NextCardIndex = 0;
	TArray<FGameXXKCardPlayResult> AutomaticResults;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("automatic Blade base resolves: %s"), *Error),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, AutomaticResults, &Error))) return true;
	TestEqual(TEXT("automatic replay does not consume the waiting Ying Feng Charge"),
		CountModifiers(Runtime, EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard), 2);
	TestEqual(TEXT("automatic Blade replay does not create its own Momentum Charge"),
		CountModifiers(Runtime, EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::ApplyStatus), 1);
	if (!Resolve(*this, Runtime, TEXT("Next"), AllyUnitId, Result, TEXT("next real active card"))) return true;
	TestEqual(TEXT("the next real owner receives Agility2"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::Agility), 2);
	TestEqual(TEXT("the next real owner receives Counter1"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::Counter), 1);
	TestEqual(TEXT("the waiting Charge is consumed only by the real active card"),
		CountModifiers(Runtime, EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBladeLastCardFinishTest,
	"GameXXK.Data.HeroCards.Blade.EndTurnUsesOnlyLastActiveBladeCardForFinish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBladeLastCardFinishTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroBladeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("YingFeng"), TEXT("Hero.Blade.YingFengHuanBu"), HeroUnitId, 0),
		MakeCard(TEXT("XueLu"), TEXT("Hero.Blade.XueLuXiangCheng"), HeroUnitId, 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("YingFeng"), TEXT("XueLu")}, 52004)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("YingFeng"), NAME_None, Result, TEXT("earlier Blade card"))) return true;
	if (!Resolve(*this, Runtime, TEXT("XueLu"), EnemyUnitId, Result, TEXT("last Blade card"))) return true;
	TArray<FGameXXKCardDamageResult> EndResults;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("player phase ends: %s"), *Error), GameXXKCardRules::EndPlayerCardPhase(Runtime, EndResults, &Error))) return true;
	TestEqual(TEXT("only Xue Lu's two Finish effects are registered"),
		CountModifiers(Runtime, EGameXXKCardBattleModifierTrigger::FirstActiveAttackAgainstStatusNextPlayerRound), 2);
	TestEqual(TEXT("the earlier Ying Feng Finish is suppressed"),
		CountModifiers(Runtime, EGameXXKCardBattleModifierTrigger::NextPlayerRoundStart), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBladeNonBladeFinishTest,
	"GameXXK.Data.HeroCards.Blade.NonBladeLastActiveCardSuppressesFinish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBladeNonBladeFinishTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroBladeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("YingFeng"), TEXT("Hero.Blade.YingFengHuanBu"), HeroUnitId, 0),
		MakeCard(TEXT("Generic"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("YingFeng"), TEXT("Generic")}, 52005)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("YingFeng"), NAME_None, Result, TEXT("Blade card"))) return true;
	if (!Resolve(*this, Runtime, TEXT("Generic"), EnemyUnitId, Result, TEXT("non-Blade last card"))) return true;
	TArray<FGameXXKCardDamageResult> EndResults;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("player phase ends: %s"), *Error), GameXXKCardRules::EndPlayerCardPhase(Runtime, EndResults, &Error))) return true;
	TestEqual(TEXT("a non-Blade final card registers no Finish effects"),
		CountModifiers(Runtime, EGameXXKCardBattleModifierTrigger::NextPlayerRoundStart), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTongFengChargeReplayTest,
	"GameXXK.Data.HeroCards.Blade.TongFengChargeReplaysNextBaseOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTongFengChargeReplayTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroBladeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("TongFeng"), TEXT("Hero.Blade.TongFengYinShi"), HeroUnitId, 0),
		MakeCard(TEXT("HeYu"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1),
		MakeCard(TEXT("DrawFiller"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 2)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("TongFeng"), TEXT("HeYu")}, 52006)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("TongFeng"), AllyUnitId, Result, TEXT("Tong Feng Charge source"))) return true;
	const int32 EnergyBefore = Runtime.Deck.SharedEnergy;
	const int32 ManaBefore = FindUnit(Runtime, HeroUnitId)->Mana;
	if (!Resolve(*this, Runtime, TEXT("HeYu"), EnemyUnitId, Result, TEXT("replayed next card"))) return true;
	TestEqual(TEXT("the active and replayed He Yu bases each emit one attack"), Result.DamageResults.Num(), 2);
	TestEqual(TEXT("the replayed base does not count as another active card"), Runtime.ActiveCardsPlayedThisRound, 2);
	TestEqual(TEXT("the replayed base spends no additional shared energy"), Runtime.Deck.SharedEnergy, EnergyBefore - 1);
	TestEqual(TEXT("the replayed base spends no additional mana"), FindUnit(Runtime, HeroUnitId)->Mana, ManaBefore - 3);
	TestEqual(TEXT("both 160 percent attacks deal damage"), FindUnit(Runtime, EnemyUnitId)->HP, 968);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTongFengFinishReplayTest,
	"GameXXK.Data.HeroCards.Blade.TongFengFinishReplaysSourceBaseAfterNextRoundFirstActive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTongFengFinishReplayTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroBladeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Opening"), TEXT("Hero.Generic.NingShenTuNa"), HeroUnitId, 0),
		MakeCard(TEXT("TongFeng"), TEXT("Hero.Blade.TongFengYinShi"), HeroUnitId, 1),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 2),
		MakeCard(TEXT("DrawFiller"), TEXT("Hero.Generic.HengJianShouShi"), HeroUnitId, 3)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Opening"), TEXT("TongFeng"), TEXT("Next")}, 52007)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Opening"), NAME_None, Result, TEXT("non-Blade opener"))) return true;
	if (!Resolve(*this, Runtime, TEXT("TongFeng"), AllyUnitId, Result, TEXT("Tong Feng Finish source"))) return true;
	TestEqual(TEXT("Tong Feng base grants Momentum2 once"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::Momentum), 2);
	if (!EndRoundAndBeginNext(*this, Runtime, TEXT("Tong Feng Finish"))) return true;
	if (!ArrangeHand(*this, Runtime, {TEXT("Next")}, TEXT("Tong Feng next-round trigger"))) return true;
	const int32 HandBefore = Runtime.Deck.Hand.Num();
	if (!Resolve(*this, Runtime, TEXT("Next"), EnemyUnitId, Result, TEXT("next-round first active card"))) return true;
	TestEqual(TEXT("the stored source base grants its anchored ally Momentum2 again"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::Momentum), 4);
	TestEqual(TEXT("the source replay draws one after the active card leaves hand"), Runtime.Deck.Hand.Num(), HandBefore);
	TestEqual(TEXT("the source replay does not increment the active-card counter"), Runtime.ActiveCardsPlayedThisRound, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKXueLuChargeBleedTest,
	"GameXXK.Data.HeroCards.Blade.XueLuChargeTriggersBleedWithoutDecay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKXueLuChargeBleedTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroBladeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("XueLu"), TEXT("Hero.Blade.XueLuXiangCheng"), HeroUnitId, 0),
		MakeCard(TEXT("Attack"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("XueLu"), TEXT("Attack")}, 52008)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("XueLu"), EnemyUnitId, Result, TEXT("Xue Lu Charge source"))) return true;
	const FGameXXKCardDamageResult* SourceBleed = FindCause(Result.DamageResults, EGameXXKCardDamageCause::Bleed);
	TestNotNull(TEXT("Xue Lu's own direct hit triggers its attached Bleed"), SourceBleed);
	if (SourceBleed)
	{
		TestEqual(TEXT("Xue Lu's coefficient six resolves to Bleed7 at the fixture level"), SourceBleed->StatusStacksBefore, 7);
		TestEqual(TEXT("the ordinary direct hit does not drain the Bleed reservoir"), SourceBleed->StatusStacksConsumed, 0);
	}
	TestEqual(TEXT("Xue Lu retains the full Bleed7 reservoir after its own trigger"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Bleed), 7);
	if (!Resolve(*this, Runtime, TEXT("Attack"), EnemyUnitId, Result, TEXT("next active attack"))) return true;
	TArray<const FGameXXKCardDamageResult*> BleedResults;
	for (const FGameXXKCardDamageResult& DamageResult : Result.DamageResults)
	{
		if (DamageResult.Cause == EGameXXKCardDamageCause::Bleed)
		{
			BleedResults.Add(&DamageResult);
		}
	}
	TestEqual(TEXT("the next attack produces its normal Bleed trigger plus Xue Lu's extra trigger"), BleedResults.Num(), 2);
	if (BleedResults.Num() == 2)
	{
		TestEqual(TEXT("the ordinary hit snapshots the seven live Bleed"), BleedResults[0]->StatusStacksBefore, 7);
		TestEqual(TEXT("the ordinary hit consumes no Bleed"), BleedResults[0]->StatusStacksConsumed, 0);
		TestEqual(TEXT("Xue Lu's extra trigger reads the same seven-point reservoir"), BleedResults[1]->StatusStacksBefore, 7);
		TestEqual(TEXT("the extra trigger also consumes no Bleed"), BleedResults[1]->StatusStacksConsumed, 0);
	}
	TestEqual(TEXT("both triggers leave the Bleed reservoir unchanged"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Bleed), 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKXueLuFinishTimingTest,
	"GameXXK.Data.HeroCards.Blade.XueLuFinishWaitsForFirstActiveAttackAgainstBleedingTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKXueLuFinishTimingTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroBladeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Opening"), TEXT("Hero.Generic.HengJianShouShi"), HeroUnitId, 0),
		MakeCard(TEXT("XueLu"), TEXT("Hero.Blade.XueLuXiangCheng"), HeroUnitId, 1),
		MakeCard(TEXT("NonAttack"), TEXT("Hero.Generic.NingShenTuNa"), HeroUnitId, 2),
		MakeCard(TEXT("Attack"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 3),
		MakeCard(TEXT("FillerA"), TEXT("Hero.Generic.HengJianShouShi"), HeroUnitId, 4),
		MakeCard(TEXT("FillerB"), TEXT("Hero.Generic.HengJianShouShi"), HeroUnitId, 5)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Opening"), TEXT("XueLu"), TEXT("NonAttack"), TEXT("Attack")}, 52009)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Opening"), HeroUnitId, Result, TEXT("non-Blade opener"))) return true;
	if (!Resolve(*this, Runtime, TEXT("XueLu"), EnemyUnitId, Result, TEXT("Xue Lu Finish source"))) return true;
	if (!EndRoundAndBeginNext(*this, Runtime, TEXT("Xue Lu Finish"))) return true;
	if (!ArrangeHand(*this, Runtime, {TEXT("NonAttack"), TEXT("Attack")}, TEXT("Xue Lu trigger order"))) return true;
	const int32 EnergyAtRoundStart = Runtime.Deck.SharedEnergy;
	if (!Resolve(*this, Runtime, TEXT("NonAttack"), NAME_None, Result, TEXT("non-attack first active card"))) return true;
	TestEqual(TEXT("a non-attack leaves both Xue Lu Finish effects waiting"),
		CountModifiers(Runtime, EGameXXKCardBattleModifierTrigger::FirstActiveAttackAgainstStatusNextPlayerRound), 2);
	if (!Resolve(*this, Runtime, TEXT("Attack"), EnemyUnitId, Result, TEXT("first active attack against Bleed"))) return true;
	TestEqual(TEXT("the qualifying attack consumes both Finish effects"),
		CountModifiers(Runtime, EGameXXKCardBattleModifierTrigger::FirstActiveAttackAgainstStatusNextPlayerRound), 0);
	TestEqual(TEXT("the qualifying attack refunds one shared energy"), Runtime.Deck.SharedEnergy, EnergyAtRoundStart);
	TestEqual(TEXT("the qualifying attack draws two cards"), Runtime.Deck.Hand.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKYingFengChargeTest,
	"GameXXK.Data.HeroCards.Blade.YingFengChargeGrantsNextOwnerAgility2Counter1",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKYingFengChargeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroBladeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("YingFeng"), TEXT("Hero.Blade.YingFengHuanBu"), HeroUnitId, 0),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HengJianShouShi"), HeroUnitId, 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("YingFeng"), TEXT("Next")}, 52010)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("YingFeng"), NAME_None, Result, TEXT("Ying Feng Charge source"))) return true;
	if (!Resolve(*this, Runtime, TEXT("Next"), HeroUnitId, Result, TEXT("Ying Feng next card"))) return true;
	TestEqual(TEXT("Ying Feng base2 plus Charge2 produces Agility4"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Agility), 4);
	TestEqual(TEXT("Ying Feng base Counter1 plus Charge Counter1 produces two sources"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Counter), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKYingFengFinishTest,
	"GameXXK.Data.HeroCards.Blade.YingFengFinishAtRoundStartGrantsHeroMark2Counter1",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKYingFengFinishTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroBladeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("YingFeng"), TEXT("Hero.Blade.YingFengHuanBu"), HeroUnitId, 0),
		MakeCard(TEXT("Filler"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("YingFeng"), TEXT("Filler")}, 52011)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("YingFeng"), NAME_None, Result, TEXT("Ying Feng Finish source"))) return true;
	TestEqual(TEXT("the base creates Mark2"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Mark), 2);
	TestEqual(TEXT("the base creates one temporary Counter source"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Counter), 1);
	if (!EndRoundAndBeginNext(*this, Runtime, TEXT("Ying Feng Finish"))) return true;
	TestEqual(TEXT("round-start Finish adds Mark2 without consuming the original Mark"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Mark), 4);
	TestEqual(TEXT("expired base Counter is replaced by exactly one Finish source"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Counter), 1);
	TestEqual(TEXT("the round-start Finish creates one independent reaction"), Runtime.Reactions.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTongPaoChargeTest,
	"GameXXK.Data.HeroCards.Blade.TongPaoChargeGrantsNextOwnerMomentum2",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTongPaoChargeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroBladeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("TongPao"), TEXT("Hero.Blade.TongPaoJuShi"), HeroUnitId, 0),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HengJianShouShi"), HeroUnitId, 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("TongPao"), TEXT("Next")}, 52012)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("TongPao"), AllyUnitId, Result, TEXT("Tong Pao Charge source"))) return true;
	if (!Resolve(*this, Runtime, TEXT("Next"), HeroUnitId, Result, TEXT("Tong Pao next card"))) return true;
	TestEqual(TEXT("the next active card owner receives Momentum2 before its base"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Momentum), 2);
	TestEqual(TEXT("the source base still grants the selected ally its own Momentum2"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::Momentum), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTongPaoFinishFreeTest,
	"GameXXK.Data.HeroCards.Blade.TongPaoFinishDiscountsBoundTargetsFirstNextRoundCardByOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTongPaoFinishFreeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroBladeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Opening"), TEXT("Hero.Generic.NingShenTuNa"), HeroUnitId, 0),
		MakeCard(TEXT("TongPao"), TEXT("Hero.Blade.TongPaoJuShi"), HeroUnitId, 1),
		MakeCard(TEXT("HeroAttack"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 2),
		MakeCard(TEXT("AllyCard"), TEXT("Hero.Generic.GuiYuanFanZhao"), AllyUnitId, 3)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Opening"), TEXT("TongPao"), TEXT("HeroAttack"), TEXT("AllyCard")}, 52013)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Opening"), NAME_None, Result, TEXT("non-Blade opener"))) return true;
	if (!Resolve(*this, Runtime, TEXT("TongPao"), AllyUnitId, Result, TEXT("Tong Pao Finish source"))) return true;
	if (!EndRoundAndBeginNext(*this, Runtime, TEXT("Tong Pao Finish"))) return true;
	if (!ArrangeHand(*this, Runtime, {TEXT("HeroAttack"), TEXT("AllyCard")}, TEXT("Tong Pao bound-target order"))) return true;

	FGameXXKCardPlayPreview Preview;
	FString Error;
	TestTrue(FString::Printf(TEXT("unbound hero preview builds: %s"), *Error), GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("HeroAttack"), Preview, &Error));
	TestEqual(TEXT("an unbound owner's first card still costs one"), Preview.EffectiveEnergyCost, 1);
	if (!Resolve(*this, Runtime, TEXT("HeroAttack"), EnemyUnitId, Result, TEXT("unbound owner's card"))) return true;
	const int32 EnergyAfterHero = Runtime.Deck.SharedEnergy;
	TestEqual(TEXT("the bound discount modifier remains after an unbound play"),
		CountModifiers(Runtime, EGameXXKCardBattleModifierTrigger::BeforeFirstActiveCardNextPlayerRound), 1);

	Error.Reset();
	TestTrue(FString::Printf(TEXT("bound ally preview builds: %s"), *Error), GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("AllyCard"), Preview, &Error));
	TestEqual(TEXT("the bound owner's printed cost-two card is discounted to one"), Preview.EffectiveEnergyCost, 1);
	if (!Resolve(*this, Runtime, TEXT("AllyCard"), NAME_None, Result, TEXT("bound owner's discounted card"))) return true;
	TestEqual(TEXT("the bound card spends exactly one shared energy"), Runtime.Deck.SharedEnergy, EnergyAfterHero - 1);
	TestEqual(TEXT("the bound discount modifier is consumed exactly once"),
		CountModifiers(Runtime, EGameXXKCardBattleModifierTrigger::BeforeFirstActiveCardNextPlayerRound), 0);
	return true;
}

#endif
