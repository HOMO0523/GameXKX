#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHeroHunterRuntimeTest
{
	const FName HeroUnitId(TEXT("Hero"));
	const FName AllyUnitId(TEXT("Ally"));
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
		Unit.HP = Side == EGameXXKCardTargetSide::Enemy ? 2000 : 100;
		Unit.MaxHP = Unit.HP;
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
			MakeUnit(AllyUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hunter, 8, 2),
			MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 8, 10)};
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
		Card.OwnerUnitId = HeroUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Hunter.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardInstance>& Cards,
		const TArray<FName>& HandInstanceIds,
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
			Test.AddError(FString::Printf(TEXT("hunter runtime failed to initialize: %s"), *Error));
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
			Test.AddError(FString::Printf(TEXT("deterministic hunter fixture is invalid: %s"), *Error));
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

	int32 CountCause(const TArray<FGameXXKCardDamageResult>& Results, const EGameXXKCardDamageCause Cause)
	{
		int32 Count = 0;
		for (const FGameXXKCardDamageResult& Result : Results)
		{
			Count += Result.Cause == Cause ? 1 : 0;
		}
		return Count;
	}

	int32 CountOrigin(const TArray<FGameXXKCardDamageResult>& Results, const EGameXXKCardResolutionOrigin Origin)
	{
		int32 Count = 0;
		for (const FGameXXKCardDamageResult& Result : Results)
		{
			Count += Result.ResolutionOrigin == Origin ? 1 : 0;
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

	void AddAfterNextStatusModifier(
		FGameXXKCardBattleRuntime& Runtime,
		const TCHAR* ModifierId,
		const EGameXXKCardStatus StatusType,
		const int32 Magnitude)
	{
		FGameXXKCardBattleModifierRuntime& Modifier = Runtime.Modifiers.AddDefaulted_GetRef();
		Modifier.ModifierId = FName(ModifierId);
		Modifier.SourceCardInstanceId = TEXT("Hunter.Listener");
		Modifier.SourceUnitId = HeroUnitId;
		Modifier.RecipientUnitIds.Add(HeroUnitId);
		Modifier.Definition.Trigger = EGameXXKCardBattleModifierTrigger::AfterNextActiveCard;
		Modifier.Definition.EffectType = EGameXXKCardEffectType::ApplyStatus;
		Modifier.Definition.Target = EGameXXKCardEffectTarget::CardOwner;
		Modifier.Definition.RecipientScope = EGameXXKCardModifierRecipientScope::CardOwner;
		Modifier.Definition.RecipientTarget = EGameXXKCardEffectTarget::CardOwner;
		Modifier.Definition.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
		Modifier.Definition.Status = StatusType;
		Modifier.Definition.Magnitude = Magnitude;
		Modifier.Definition.RemainingTriggers = 1;
		Modifier.Definition.bPersistent = true;
		Modifier.Definition.bActivePlayOnly = true;
		Modifier.SourceCardSnapshot.CardId = TEXT("Hero.Hunter.FengYanDingXian");
		Modifier.SourceCardSnapshot.Quality = EGameXXKCardQuality::Common;
		Modifier.SourceCardSnapshot.OwnerUnitId = HeroUnitId;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHunterNoChargeTest,
	"GameXXK.Data.HeroCards.Hunter.NoChargeHeavyArrowCardsRemainPlayable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterNoChargeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHunterRuntimeTest;
	struct FCase
	{
		const TCHAR* InstanceId;
		const TCHAR* CardId;
		int32 ExpectedDamageResults;
	};
	const TArray<FCase> Cases = {
		{TEXT("LieYu"), TEXT("Hero.Hunter.LieYuLianShi"), 1},
		{TEXT("CuiDu"), TEXT("Hero.Hunter.CuiDuChuanXin"), 2},
		{TEXT("HuiFeng"), TEXT("Hero.Hunter.HuiFengGuanRi"), 1}};
	for (int32 CaseIndex = 0; CaseIndex < Cases.Num(); ++CaseIndex)
	{
		const FCase& Case = Cases[CaseIndex];
		const TArray<FGameXXKCardInstance> Cards = {MakeCard(Case.InstanceId, Case.CardId, 0)};
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, Cards, {FName(Case.InstanceId)}, 55001 + CaseIndex)) return false;
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, FName(Case.InstanceId), EnemyUnitId, Result, Case.InstanceId)) continue;
		TestEqual(FString::Printf(TEXT("%s keeps its base packet count without Charge"), Case.InstanceId), Result.DamageResults.Num(), Case.ExpectedDamageResults);
		TestEqual(FString::Printf(TEXT("%s consumes no Charge"), Case.InstanceId), Result.HeavyArrowChargeConsumed, 0);
		TestEqual(FString::Printf(TEXT("%s adds no Heavy Arrow attacks"), Case.InstanceId), Result.HeavyArrowExtraAttackCount, 0);
		TestEqual(FString::Printf(TEXT("%s adds no Heavy Arrow explosions"), Case.InstanceId), Result.HeavyArrowToxicExplosionCount, 0);
		TestEqual(FString::Printf(TEXT("%s adds no primary percentage"), Case.InstanceId), Result.HeavyArrowPrimaryBonusPercent, 0);
		TestEqual(FString::Printf(TEXT("%s leaves Charge at zero"), Case.InstanceId), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Charge), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHunterFengYanTest,
	"GameXXK.Data.HeroCards.Hunter.FengYanDrawsDiscardsAndBuildsChargeWithoutConsuming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterFengYanTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHunterRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("FengYan"), TEXT("Hero.Hunter.FengYanDingXian"), 0),
		MakeCard(TEXT("FillerA"), TEXT("Hero.Generic.QingFengYiShi"), 1),
		MakeCard(TEXT("FillerB"), TEXT("Hero.Generic.HeYuZhan"), 2)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("FengYan")}, 55010)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("FengYan"), NAME_None, Result, TEXT("Feng Yan"))) return true;
	TestTrue(TEXT("Feng Yan opens its one-card forced discard"), Result.bOpenedPendingChoice);
	TestEqual(TEXT("Feng Yan draws two cards"), Runtime.Deck.Hand.Num(), 2);
	TestEqual(TEXT("Feng Yan requires one discard"), Runtime.Deck.PendingChoice.RequiredDiscardCount, 1);
	TestEqual(TEXT("Feng Yan grants Agility2"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Agility), 2);
	TestEqual(TEXT("Feng Yan grants Charge3"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Charge), 3);
	TestEqual(TEXT("a non-Heavy-Arrow setup card consumes no Charge"), Result.HeavyArrowChargeConsumed, 0);
	TestEqual(TEXT("a non-Heavy-Arrow setup card produces no damage"), Result.DamageResults.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHunterLieYuTest,
	"GameXXK.Data.HeroCards.Hunter.LieYuLocksChargeAndAddsLiveDirectPackets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterLieYuTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHunterRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("LieYu"), TEXT("Hero.Hunter.LieYuLianShi"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("LieYu")}, 55011)) return false;
	TestEqual(TEXT("fixture grants Charge3"), GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Charge, 3), 3);
	TestEqual(TEXT("fixture grants Mark4"), GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, EnemyUnitId), EGameXXKCardStatus::Mark, 4), 4);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("LieYu"), EnemyUnitId, Result, TEXT("Lie Yu Heavy Arrow"))) return true;
	TestEqual(TEXT("Lie Yu locks and consumes Charge3"), Result.HeavyArrowChargeConsumed, 3);
	TestEqual(TEXT("Lie Yu appends three Heavy Arrow attacks"), Result.HeavyArrowExtraAttackCount, 3);
	TestEqual(TEXT("Lie Yu emits one base plus three extra direct packets"), Result.DamageResults.Num(), 4);
	const TArray<int32> ExpectedBaseRequested = {14, 5, 5, 5};
	const TArray<int32> ExpectedMarkBefore = {4, 3, 2, 1};
	for (int32 Index = 0; Index < Result.DamageResults.Num() && Index < ExpectedBaseRequested.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("Lie Yu packet %d uses its exact requested amount"), Index), Result.DamageResults[Index].BaseRequestedDamage, ExpectedBaseRequested[Index]);
		TestEqual(FString::Printf(TEXT("Lie Yu packet %d observes live Mark"), Index), Result.DamageResults[Index].MarkStacksBeforeHit, ExpectedMarkBefore[Index]);
		TestEqual(FString::Printf(TEXT("Lie Yu packet %d consumes one live Mark"), Index), Result.DamageResults[Index].MarkStacksConsumed, 1);
	}
	TestEqual(TEXT("only the base packet has active-play origin"), CountOrigin(Result.DamageResults, EGameXXKCardResolutionOrigin::ActivePlay), 1);
	TestEqual(TEXT("all three appended packets have HeavyArrow origin"), CountOrigin(Result.DamageResults, EGameXXKCardResolutionOrigin::HeavyArrow), 3);
	TestEqual(TEXT("Bleed8 is applied before and survives all appended hits"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Bleed), 8);
	TestEqual(TEXT("all four direct hits consume the four live Mark layers"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("the locked Charge is empty after resolution"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Charge), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHunterCuiDuTest,
	"GameXXK.Data.HeroCards.Hunter.CuiDuExplodesWithoutAddingPoisonOrRot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterCuiDuTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHunterRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("CuiDu"), TEXT("Hero.Hunter.CuiDuChuanXin"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("CuiDu")}, 55012)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Charge, 3);
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, EnemyUnitId), EGameXXKCardStatus::DamageOverTime, 7);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("CuiDu"), EnemyUnitId, Result, TEXT("Cui Du Heavy Arrow"))) return true;
	TestEqual(TEXT("Cui Du locks Charge3"), Result.HeavyArrowChargeConsumed, 3);
	TestEqual(TEXT("Cui Du records exactly three added Toxic Explosions"), Result.HeavyArrowToxicExplosionCount, 3);
	TestEqual(TEXT("Cui Du never adds an attack segment"), Result.HeavyArrowExtraAttackCount, 0);
	TestEqual(TEXT("one base attack plus four Poison explosions are emitted"), Result.DamageResults.Num(), 5);
	TestEqual(TEXT("four explosion packets are Poison only"), CountCause(Result.DamageResults, EGameXXKCardDamageCause::ToxicExplosionPoison), 4);
	TestEqual(TEXT("Heavy Arrow never emits a Rot packet"), CountCause(Result.DamageResults, EGameXXKCardDamageCause::Rot), 0);
	const TArray<int32> ExpectedPoisonSnapshots = {6, 5, 4, 3};
	int32 PoisonIndex = 0;
	for (const FGameXXKCardDamageResult& DamageResult : Result.DamageResults)
	{
		if (DamageResult.Cause == EGameXXKCardDamageCause::ToxicExplosionPoison)
		{
			TestEqual(FString::Printf(TEXT("Poison explosion %d observes the live decreasing stack"), PoisonIndex), DamageResult.StatusStacksBefore, ExpectedPoisonSnapshots[PoisonIndex]);
			TestEqual(FString::Printf(TEXT("Poison explosion %d consumes one Poison"), PoisonIndex), DamageResult.StatusStacksConsumed, 1);
			++PoisonIndex;
		}
	}
	TestEqual(TEXT("only the base card applies Poison6; four explosions leave Poison2"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Poison), 2);
	TestEqual(TEXT("all Rot survives and only amplifies real Poison packets"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::DamageOverTime), 7);
	TestEqual(TEXT("Cui Du consumes all locked Charge"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Charge), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHunterHuiFengTest,
	"GameXXK.Data.HeroCards.Hunter.HuiFengMergesChargeIntoOnePrimaryPacket",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterHuiFengTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHunterRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("HuiFeng"), TEXT("Hero.Hunter.HuiFengGuanRi"), 0),
		MakeCard(TEXT("FillerA"), TEXT("Hero.Generic.QingFengYiShi"), 1),
		MakeCard(TEXT("FillerB"), TEXT("Hero.Generic.HeYuZhan"), 2),
		MakeCard(TEXT("FillerC"), TEXT("Hero.Generic.SuiYanJi"), 3)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("HuiFeng")}, 55013)) return false;
	Runtime.Deck.SharedEnergy = 4;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Charge, 3);
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, EnemyUnitId), EGameXXKCardStatus::Mark, 3);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("HuiFeng"), EnemyUnitId, Result, TEXT("Hui Feng Heavy Arrow"))) return true;
	TestEqual(TEXT("Hui Feng consumes Charge3"), Result.HeavyArrowChargeConsumed, 3);
	TestEqual(TEXT("Hui Feng records 120 added percentage points"), Result.HeavyArrowPrimaryBonusPercent, 120);
	TestEqual(TEXT("Hui Feng does not create extra attack packets"), Result.HeavyArrowExtraAttackCount, 0);
	TestEqual(TEXT("Hui Feng emits exactly one merged primary packet"), Result.DamageResults.Num(), 1);
	if (Result.DamageResults.Num() == 1)
	{
		TestEqual(TEXT("150% plus 3x40% yields a 270% requested packet"), Result.DamageResults[0].BaseRequestedDamage, 27);
		TestEqual(TEXT("the merged primary remains the active card's attack"), Result.DamageResults[0].ResolutionOrigin, EGameXXKCardResolutionOrigin::ActivePlay);
		TestEqual(TEXT("one merged hit consumes only one Mark"), Result.DamageResults[0].MarkStacksConsumed, 1);
	}
	TestEqual(TEXT("Hui Feng draws once per locked Charge"), Runtime.Deck.Hand.Num(), 3);
	TestEqual(TEXT("Charge3 refunds exactly one energy after paying one"), Runtime.Deck.SharedEnergy, 4);
	TestEqual(TEXT("one merged hit leaves two of three Mark"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Mark), 2);
	TestEqual(TEXT("all three Charge layers are gone"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Charge), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHunterNewChargeSurvivesTest,
	"GameXXK.Data.HeroCards.Hunter.NewChargeCreatedAfterLockSurvivesForTheNextCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterNewChargeSurvivesTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHunterRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("LieYu"), TEXT("Hero.Hunter.LieYuLianShi"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("LieYu")}, 55014)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Charge, 2);
	AddAfterNextStatusModifier(Runtime, TEXT("Hunter.NewCharge.AfterLock"), EGameXXKCardStatus::Charge, 3);
	FString Error;
	TestTrue(FString::Printf(TEXT("post-lock Charge listener validates: %s"), *Error), GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error));
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("LieYu"), EnemyUnitId, Result, TEXT("post-lock Charge Lie Yu"))) return true;
	TestEqual(TEXT("the Heavy Arrow consumes only the two-layer starting snapshot"), Result.HeavyArrowChargeConsumed, 2);
	TestEqual(TEXT("the starting snapshot produces exactly two appended attacks"), Result.HeavyArrowExtraAttackCount, 2);
	TestEqual(TEXT("Charge3 granted after the lock survives for the next Heavy Arrow"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Charge), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHunterBoundaryTest,
	"GameXXK.Data.HeroCards.Hunter.HeavyArrowSegmentsDoNotAdvanceActiveCountersTasksOrListeners",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHunterRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("LieYu"), TEXT("Hero.Hunter.LieYuLianShi"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("LieYu")}, 55015)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Charge, 4);
	AddAfterNextStatusModifier(Runtime, TEXT("Hunter.OneActive.Listener"), EGameXXKCardStatus::Agility, 1);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("LieYu"), EnemyUnitId, Result, TEXT("boundary Lie Yu"))) return true;
	TestEqual(TEXT("one card plus four Heavy Arrow packets counts as one active card"), Runtime.ActiveCardsPlayedThisRound, 1);
	TestEqual(TEXT("the active snapshot remains the source card"), Runtime.LastActiveCard.CardId, FName(TEXT("Hero.Hunter.LieYuLianShi")));
	TestEqual(TEXT("the after-next listener triggers exactly once"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Agility), 1);
	TestEqual(TEXT("no Heavy Arrow segment starts a spell task"), Runtime.HeroSpellTask.bActive, false);
	TestEqual(TEXT("no Heavy Arrow segment records a spell-task play"), Runtime.HeroSpellTask.FirstPlayOrder.Num(), 0);
	TestFalse(TEXT("Heavy Arrow packets never become an automatic card queue"), Runtime.AutomaticResolutionQueue.bActive);
	TestFalse(TEXT("Heavy Arrow packets never claim a terrain change"), Runtime.bTerrainChangedThisRound);
	TestEqual(TEXT("the result audits four Heavy Arrow segments"), Result.HeavyArrowExtraAttackCount, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHunterChargeCapacityTest,
	"GameXXK.Data.HeroCards.Hunter.ChargeCanAccumulateAtLeastThreeLayersAndHasNoLegacyOneStackCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterChargeCapacityTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHunterRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("Filler"), TEXT("Hero.Generic.QingFengYiShi"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Filler")}, 55016)) return false;
	TestEqual(TEXT("the first Charge grant applies two layers"), GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Charge, 2), 2);
	TestEqual(TEXT("a second Charge grant applies three more layers"), GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Charge, 3), 3);
	TestEqual(TEXT("Charge can hold five layers"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Charge), 5);
	FString Error;
	TestTrue(FString::Printf(TEXT("a runtime with Charge5 validates: %s"), *Error), GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error));
	return true;
}

#endif
