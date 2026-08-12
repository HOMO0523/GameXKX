#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKBladePartnerBloodEdgeRuntimeTest
{
	constexpr const TCHAR* BladeUnitId = TEXT("BladePartner");
	constexpr const TCHAR* EnemyUnitId = TEXT("Enemy");
	constexpr const TCHAR* SecondEnemyUnitId = TEXT("Enemy.Second");

	FGameXXKCardCombatUnit MakeUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 HP,
		const int32 Attack,
		const int32 Mana,
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
		Unit.MaxMana = Mana;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardInstance> MakeCards(const TCHAR* CardId, const int32 Count)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
			Card.InstanceId = FName(*FString::Printf(TEXT("BloodEdge.%s.%d"), CardId, Index));
			Card.CardId = FName(CardId);
			Card.OwnerUnitId = BladeUnitId;
			Card.SourceEntryId = FName(*FString::Printf(TEXT("BloodEdge.Source.%d"), Index));
			Card.AcquisitionOrdinal = Index;
		}
		return Cards;
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
		Card.SourceEntryId = FName(*FString::Printf(TEXT("BloodEdge.Exact.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildExactRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardInstance>& Cards)
	{
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(BladeUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 100, 20, 20, 1),
			MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1000, 10, 0, 10)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			58104,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("Blood Edge exact runtime failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = Cards;
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Blood Edge exact fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	bool EndRoundAndBeginNext(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FGameXXKCardDamageResult> DamageResults;
		FString Error;
		if (!Test.TestTrue(
			FString::Printf(TEXT("Blood Edge player phase ends: %s"), *Error),
			GameXXKCardRules::EndPlayerCardPhase(Runtime, DamageResults, &Error)))
		{
			return false;
		}
		Error.Reset();
		return Test.TestTrue(
			FString::Printf(TEXT("Blood Edge next player round begins: %s"), *Error),
			GameXXKCardRules::BeginNextPlayerCardRound(Runtime, DamageResults, &Error));
	}

	FGameXXKCardCombatUnit* FindUnit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKJiYuLianZhanLiveBleedRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.BloodEdge.JiYuLianZhanUsesLiveBleedPerHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKJiYuLianZhanLiveBleedRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerBloodEdgeRuntimeTest;

	TArray<FGameXXKCardCombatUnit> Units;
	Units.Add(MakeUnit(BladeUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 100, 20, 20, 1));
	Units.Add(MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 200, 10, 0, 10));

	FGameXXKCardBattleRuntime Runtime;
	FString Error;
	if (!TestTrue(
		FString::Printf(TEXT("Ji Yu runtime initializes: %s"), *Error),
		GameXXKCardRules::InitializeCardBattleRuntime(
			Runtime,
			MakeCards(TEXT("Profession.Blade.JiYuLianZhan"), 6),
			Units,
			EGameXXKCardTerrain::Plain,
			58101,
			&Error)))
	{
		return false;
	}

	FGameXXKCardPlayResult Result;
	const FName CardInstanceId = Runtime.Deck.Hand[0].InstanceId;
	if (!TestTrue(
		FString::Printf(TEXT("Ji Yu resolves: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, CardInstanceId, EnemyUnitId, Result, &Error)))
	{
		return true;
	}

	const FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, EnemyUnitId);
	TestNotNull(TEXT("Ji Yu keeps the enemy audit target available"), Enemy);
	if (!Enemy)
	{
		return true;
	}
	TestEqual(TEXT("three live Blood Edge hits plus three Bleed packets deal fifty-one total damage"), Enemy->HP, 149);
	TestEqual(TEXT("each landed hit consumes one of Ji Yu's three Bleed layers"),
		GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Bleed), 0);
	TestEqual(TEXT("Ji Yu audits three direct hits and the three Bleed triggers that follow them"), Result.DamageResults.Num(), 6);

	const TArray<int32> ExpectedDirectDamage = {17, 15, 13};
	const TArray<int32> ExpectedBleedSnapshots = {3, 2, 1};
	for (int32 HitIndex = 0; HitIndex < 3 && Result.DamageResults.Num() == 6; ++HitIndex)
	{
		const FGameXXKCardDamageResult& Direct = Result.DamageResults[HitIndex * 2];
		const FGameXXKCardDamageResult& Bleed = Result.DamageResults[HitIndex * 2 + 1];
		TestEqual(FString::Printf(TEXT("hit %d reads the live Bleed stack into its attack percentage"), HitIndex + 1),
			Direct.RequestedDamage, ExpectedDirectDamage[HitIndex]);
		TestEqual(FString::Printf(TEXT("hit %d remains a direct-attack audit"), HitIndex + 1),
			Direct.Cause, EGameXXKCardDamageCause::DirectAttack);
		TestEqual(FString::Printf(TEXT("Bleed trigger %d snapshots the pre-decay stack"), HitIndex + 1),
			Bleed.StatusStacksBefore, ExpectedBleedSnapshots[HitIndex]);
		TestEqual(FString::Printf(TEXT("Bleed trigger %d consumes exactly one layer"), HitIndex + 1),
			Bleed.StatusStacksConsumed, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLangDuanPreservesTriggeredBleedRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.BloodEdge.LangDuanPreservesItsTriggeredBleed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLangDuanPreservesTriggeredBleedRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerBloodEdgeRuntimeTest;

	TArray<FGameXXKCardCombatUnit> Units;
	Units.Add(MakeUnit(BladeUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 100, 20, 20, 1));
	Units.Add(MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 10, 0, 10));
	TestEqual(TEXT("Lang Duan fixture starts with four Bleed"),
		GameXXKCardRules::AddCombatStatus(Units[1], EGameXXKCardStatus::Bleed, 4), 4);

	FGameXXKCardBattleRuntime Runtime;
	FString Error;
	if (!TestTrue(
		FString::Printf(TEXT("Lang Duan runtime initializes: %s"), *Error),
		GameXXKCardRules::InitializeCardBattleRuntime(
			Runtime,
			MakeCards(TEXT("Profession.Blade.LangDuan"), 6),
			Units,
			EGameXXKCardTerrain::Plain,
			58102,
			&Error)))
	{
		return false;
	}

	FGameXXKCardPlayResult Result;
	if (!TestTrue(
		FString::Printf(TEXT("Lang Duan resolves: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(
			Runtime,
			Runtime.Deck.Hand[0].InstanceId,
			EnemyUnitId,
			Result,
			&Error)))
	{
		return true;
	}

	const FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, EnemyUnitId);
	TestNotNull(TEXT("Lang Duan keeps its target available"), Enemy);
	if (!Enemy)
	{
		return true;
	}
	TestEqual(TEXT("four Bleed add forty percentage points before Lang Duan deals its four-point Bleed trigger"), Enemy->HP, 68);
	TestEqual(TEXT("Lang Duan's own Bleed trigger does not reduce the target's layers"),
		GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Bleed), 4);
	TestEqual(TEXT("Lang Duan produces one direct hit and one Bleed trigger"), Result.DamageResults.Num(), 2);
	if (Result.DamageResults.Num() == 2)
	{
		TestEqual(TEXT("Lang Duan snapshots its one-hundred-forty-percent live Blood Edge hit"), Result.DamageResults[0].RequestedDamage, 28);
		TestEqual(TEXT("Lang Duan audits all four triggered Bleed layers"), Result.DamageResults[1].StatusStacksBefore, 4);
		TestEqual(TEXT("Lang Duan records zero consumed Bleed layers"), Result.DamageResults[1].StatusStacksConsumed, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKYinXueDaoHealsTriggeredBleedRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.BloodEdge.YinXueDaoHealsOnlyItsTriggeredBleed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKYinXueDaoHealsTriggeredBleedRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerBloodEdgeRuntimeTest;

	TArray<FGameXXKCardCombatUnit> Units;
	Units.Add(MakeUnit(BladeUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 50, 20, 20, 1));
	Units[0].MaxHP = 100;
	Units.Add(MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 10, 0, 10));
	TestEqual(TEXT("Yin Xue fixture starts with four Bleed"),
		GameXXKCardRules::AddCombatStatus(Units[1], EGameXXKCardStatus::Bleed, 4), 4);

	FGameXXKCardBattleRuntime Runtime;
	FString Error;
	if (!TestTrue(
		FString::Printf(TEXT("Yin Xue runtime initializes: %s"), *Error),
		GameXXKCardRules::InitializeCardBattleRuntime(
			Runtime,
			MakeCards(TEXT("Profession.Blade.YinXueDao"), 6),
			Units,
			EGameXXKCardTerrain::Plain,
			58103,
			&Error)))
	{
		return false;
	}

	FGameXXKCardPlayResult Result;
	if (!TestTrue(
		FString::Printf(TEXT("Yin Xue resolves: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(
			Runtime,
			Runtime.Deck.Hand[0].InstanceId,
			EnemyUnitId,
			Result,
			&Error)))
	{
		return true;
	}

	const FGameXXKCardCombatUnit* Blade = FindUnit(Runtime, BladeUnitId);
	const FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, EnemyUnitId);
	TestNotNull(TEXT("Yin Xue keeps its owner available"), Blade);
	TestNotNull(TEXT("Yin Xue keeps its target available"), Enemy);
	if (!Blade || !Enemy)
	{
		return true;
	}
	TestEqual(TEXT("Yin Xue heals exactly the four health damage from its triggered Bleed"), Blade->HP, 54);
	TestEqual(TEXT("Yin Xue deals a live one-hundred-sixty-percent hit followed by four Bleed damage"), Enemy->HP, 64);
	TestEqual(TEXT("Yin Xue consumes one old Bleed then applies its two new layers"),
		GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Bleed), 5);
	TestEqual(TEXT("Yin Xue audits its direct hit and triggered Bleed separately"), Result.DamageResults.Num(), 2);
	TestEqual(TEXT("Yin Xue audits its triggered-Bleed healing attempt"), Result.HealingResults.Num(), 1);
	if (Result.HealingResults.Num() == 1)
	{
		TestEqual(TEXT("Yin Xue healing keeps the Blade source"), Result.HealingResults[0].SourceUnitId, BladeUnitId);
		TestEqual(TEXT("Yin Xue healing keeps the Blade target"), Result.HealingResults[0].TargetUnitId, BladeUnitId);
		TestEqual(TEXT("Yin Xue healing requests the four triggered Bleed damage"), Result.HealingResults[0].RequestedHealing, 4);
		TestEqual(TEXT("Yin Xue healing restores all four requested health"), Result.HealingResults[0].EffectiveHealing, 4);
	}
	if (Result.DamageResults.Num() == 2)
	{
		TestEqual(TEXT("Yin Xue's live Blood Edge hit requests thirty-two damage"), Result.DamageResults[0].RequestedDamage, 32);
		TestEqual(TEXT("Yin Xue's healing source is the four-point Bleed packet"), Result.DamageResults[1].HealthDamage, 4);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFengHouChargeReturnsNextActiveRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.BloodEdge.FengHouChargeReturnsExactNextActiveOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFengHouChargeReturnsNextActiveRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerBloodEdgeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("FengHou"), TEXT("Profession.Blade.FengHou"), 0),
		MakeCard(TEXT("Next"), TEXT("Profession.Blade.LangDuan"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}

	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(
		FString::Printf(TEXT("Feng Hou resolves: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("FengHou"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Feng Hou arms its exact return-to-hand Charge"),
		Runtime.PendingBladeCharge.Rule, EGameXXKBladeChargeRule::ReturnNextActiveToHandOnce);

	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(
		FString::Printf(TEXT("the next active card resolves at its original cost: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Next"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("both original active-card energy costs are paid"), Runtime.Deck.SharedEnergy, 8);
	const FGameXXKCardInstance* Returned = Runtime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == TEXT("Next");
	});
	TestNotNull(TEXT("the exact resolved instance returns from discard to hand"), Returned);
	TestEqual(TEXT("the returned instance is not a free temporary copy"), Returned ? Returned->bTemporary : true, false);
	TestEqual(TEXT("Feng Hou Charge is consumed exactly once"),
		Runtime.PendingBladeCharge.Rule, EGameXXKBladeChargeRule::None);
	TestEqual(TEXT("two active plays are counted despite the returned card"), Runtime.ActiveCardsPlayedThisRound, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFengHouFinishPreservesTwoBleedRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.BloodEdge.FengHouFinishPreservesFirstTwoBleedTriggers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFengHouFinishPreservesTwoBleedRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerBloodEdgeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("FengHou"), TEXT("Profession.Blade.FengHou"), 0),
		MakeCard(TEXT("JiYu"), TEXT("Profession.Blade.JiYuLianZhan"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}

	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(
		FString::Printf(TEXT("Feng Hou resolves as the final active card: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("FengHou"), EnemyUnitId, Result, &Error))
		|| !EndRoundAndBeginNext(*this, Runtime))
	{
		return true;
	}
	TestEqual(TEXT("Feng Hou Finish enters the next player round with two protected triggers"),
		Runtime.PendingBladeFinish.Rule, EGameXXKBladeFinishRule::PreserveFirstTwoBleedTriggers);
	TestEqual(TEXT("exactly two Bleed triggers remain protected"), Runtime.PendingBladeFinish.RemainingTriggers, 2);

	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(
		FString::Printf(TEXT("Ji Yu resolves inside the Finish window: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("JiYu"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Ji Yu still audits all three direct and Bleed packets"), Result.DamageResults.Num(), 6);
	if (Result.DamageResults.Num() == 6)
	{
		TestEqual(TEXT("the first triggered Bleed keeps every layer"), Result.DamageResults[1].StatusStacksConsumed, 0);
		TestEqual(TEXT("the second triggered Bleed keeps every layer"), Result.DamageResults[3].StatusStacksConsumed, 0);
		TestEqual(TEXT("the third triggered Bleed resumes normal one-layer decay"), Result.DamageResults[5].StatusStacksConsumed, 1);
		TestEqual(TEXT("all three triggers see eight live Bleed because only the third decays"),
			Result.DamageResults[5].StatusStacksBefore, 8);
	}
	const FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, EnemyUnitId);
	TestNotNull(TEXT("Feng Hou Finish keeps the enemy available"), Enemy);
	TestEqual(TEXT("five old plus three new Bleed lose only the third trigger's one layer"),
		Enemy ? GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Bleed) : INDEX_NONE, 7);
	TestEqual(TEXT("Feng Hou Finish clears immediately after its second protected trigger"),
		Runtime.PendingBladeFinish.Rule, EGameXXKBladeFinishRule::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKJiYuChargeReplaysNextRoundRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.BloodEdge.JiYuChargeReplaysRecordedActiveNextRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKJiYuChargeReplaysNextRoundRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerBloodEdgeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("JiYu"), TEXT("Profession.Blade.JiYuLianZhan"), 0),
		MakeCard(TEXT("Recorded"), TEXT("Profession.Blade.LangDuan"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}

	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(
		FString::Printf(TEXT("Ji Yu resolves first: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("JiYu"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Ji Yu arms the delayed-replay Charge"),
		Runtime.PendingBladeCharge.Rule, EGameXXKBladeChargeRule::ReplayNextActiveNextRound);

	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(
		FString::Printf(TEXT("the recorded active card resolves normally this round: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Recorded"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	const FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, EnemyUnitId);
	TestNotNull(TEXT("the delayed-replay target remains available"), Enemy);
	const int32 HPBeforeDelayedReplay = Enemy ? Enemy->HP : INDEX_NONE;

	TArray<FGameXXKCardDamageResult> RoundBoundaryDamage;
	if (!TestTrue(
		FString::Printf(TEXT("the delayed-replay player phase ends: %s"), *Error),
		GameXXKCardRules::EndPlayerCardPhase(Runtime, RoundBoundaryDamage, &Error)))
	{
		return true;
	}
	Error.Reset();
	RoundBoundaryDamage.Reset();
	if (!TestTrue(
		FString::Printf(TEXT("the delayed replay resolves at next player-round start: %s"), *Error),
		GameXXKCardRules::BeginNextPlayerCardRound(Runtime, RoundBoundaryDamage, &Error)))
	{
		return true;
	}

	Enemy = FindUnit(Runtime, EnemyUnitId);
	TestNotNull(TEXT("the original target survives the delayed replay"), Enemy);
	TestEqual(TEXT("Lang Duan's recorded one-hundred-percent base hit replays without another active play"),
		Enemy ? Enemy->HP : INDEX_NONE, HPBeforeDelayedReplay - 20);
	TestEqual(TEXT("the next player round still starts with zero active cards played"), Runtime.ActiveCardsPlayedThisRound, 0);
	TestEqual(TEXT("the delayed base replay exposes its one damage packet at the round boundary"), RoundBoundaryDamage.Num(), 1);
	if (RoundBoundaryDamage.Num() == 1)
	{
		TestEqual(TEXT("the delayed hit keeps its original base magnitude"), RoundBoundaryDamage[0].RequestedDamage, 20);
		TestEqual(TEXT("the delayed hit is audited as an automatic replay"),
			RoundBoundaryDamage[0].ResolutionOrigin, EGameXXKCardResolutionOrigin::AutomaticReplay);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKJiYuFinishDrawsOnBleedRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.BloodEdge.JiYuFinishDrawsOnFirstThreeBleedTriggers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKJiYuFinishDrawsOnBleedRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerBloodEdgeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.JiYuLianZhan"), 0),
		MakeCard(TEXT("TriggerA"), TEXT("Profession.Blade.JiYuLianZhan"), 1),
		MakeCard(TEXT("TriggerB"), TEXT("Profession.Blade.JiYuLianZhan"), 2),
		MakeCard(TEXT("TriggerC"), TEXT("Profession.Blade.JiYuLianZhan"), 3),
		MakeCard(TEXT("TriggerD"), TEXT("Profession.Blade.JiYuLianZhan"), 4),
		MakeCard(TEXT("TriggerE"), TEXT("Profession.Blade.JiYuLianZhan"), 5)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}

	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(
		FString::Printf(TEXT("Ji Yu resolves as the Finish source: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Finisher"), EnemyUnitId, Result, &Error))
		|| !EndRoundAndBeginNext(*this, Runtime))
	{
		return true;
	}
	TestEqual(TEXT("Ji Yu Finish enters the next player round"),
		Runtime.PendingBladeFinish.Rule, EGameXXKBladeFinishRule::DrawOnFirstThreeBleedTriggers);
	TestEqual(TEXT("Ji Yu Finish begins with three draw triggers"), Runtime.PendingBladeFinish.RemainingTriggers, 3);

	const int32 TriggerIndex = Runtime.Deck.Hand.IndexOfByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId != TEXT("Finisher");
	});
	if (!TestTrue(TEXT("the next hand contains a non-finisher Ji Yu"), TriggerIndex != INDEX_NONE))
	{
		return true;
	}
	const FName TriggerInstanceId = Runtime.Deck.Hand[TriggerIndex].InstanceId;
	for (int32 HandIndex = Runtime.Deck.Hand.Num() - 1; HandIndex >= 0; --HandIndex)
	{
		if (Runtime.Deck.Hand[HandIndex].InstanceId == TriggerInstanceId)
		{
			continue;
		}
		Runtime.Deck.DrawPile.Add(MoveTemp(Runtime.Deck.Hand[HandIndex]));
		Runtime.Deck.Hand.RemoveAt(HandIndex, 1, EAllowShrinking::No);
	}
	TestEqual(TEXT("the draw fixture leaves only the trigger card in hand"), Runtime.Deck.Hand.Num(), 1);

	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(
		FString::Printf(TEXT("the three-hit Bleed trigger resolves inside Ji Yu Finish: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, TriggerInstanceId, EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("playing the only hand card then triggering Bleed three times draws exactly three cards"),
		Runtime.Deck.Hand.Num(), 3);
	TestEqual(TEXT("Ji Yu Finish is consumed after the third Bleed trigger"),
		Runtime.PendingBladeFinish.Rule, EGameXXKBladeFinishRule::None);
	TestEqual(TEXT("the trigger card still produces three direct and three Bleed packets"), Result.DamageResults.Num(), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKYinXueChargeRestoresConsumptionRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.BloodEdge.YinXueChargeRestoresConsumedStatusAndArmor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKYinXueChargeRestoresConsumptionRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerBloodEdgeRuntimeTest;

	FGameXXKCardBattleRuntime StatusRuntime;
	{
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(TEXT("YinXue"), TEXT("Profession.Blade.YinXueDao"), 0),
			MakeCard(TEXT("Consumer"), TEXT("Hero.Generic.JianYiGuanHong"), 1)};
		if (!BuildExactRuntime(*this, StatusRuntime, Cards))
		{
			return false;
		}
		FGameXXKCardCombatUnit* Blade = FindUnit(StatusRuntime, BladeUnitId);
		TestNotNull(TEXT("the status-consumption Blade exists"), Blade);
		if (!Blade)
		{
			return true;
		}
		TestEqual(TEXT("the status fixture grants three Momentum"),
			GameXXKCardRules::AddCombatStatus(*Blade, EGameXXKCardStatus::Momentum, 3), 3);
		FString Error;
		FGameXXKCardPlayResult Result;
		if (!TestTrue(TEXT("Yin Xue resolves before the status consumer"),
			GameXXKCardRules::ResolveCardPlay(StatusRuntime, TEXT("YinXue"), EnemyUnitId, Result, &Error)))
		{
			return true;
		}
		TestEqual(TEXT("Yin Xue arms the restore-consumption Charge"),
			StatusRuntime.PendingBladeCharge.Rule, EGameXXKBladeChargeRule::RestoreNextActiveOwnerState);
		Error.Reset();
		Result = FGameXXKCardPlayResult();
		if (!TestTrue(TEXT("Jian Yi consumes Momentum and resolves"),
			GameXXKCardRules::ResolveCardPlay(StatusRuntime, TEXT("Consumer"), EnemyUnitId, Result, &Error)))
		{
			return true;
		}
		Blade = FindUnit(StatusRuntime, BladeUnitId);
		TestEqual(TEXT("all three consumed Momentum layers are restored after resolution"),
			Blade ? GameXXKCardRules::GetCombatStatusStacks(*Blade, EGameXXKCardStatus::Momentum) : INDEX_NONE, 3);
	}

	FGameXXKCardBattleRuntime ArmorRuntime;
	const TArray<FGameXXKCardInstance> ArmorCards = {
		MakeCard(TEXT("YinXue"), TEXT("Profession.Blade.YinXueDao"), 0),
		MakeCard(TEXT("Consumer"), TEXT("Hero.Guard.XuanJiaZhenYue"), 1)};
	if (!BuildExactRuntime(*this, ArmorRuntime, ArmorCards))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Blade = FindUnit(ArmorRuntime, BladeUnitId);
	TestNotNull(TEXT("the armor-consumption Blade exists"), Blade);
	if (!Blade)
	{
		return true;
	}
	Blade->Armor = 10;
	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(TEXT("Yin Xue resolves before the armor consumer"),
		GameXXKCardRules::ResolveCardPlay(ArmorRuntime, TEXT("YinXue"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("Xuan Jia consumes all armor and resolves"),
		GameXXKCardRules::ResolveCardPlay(ArmorRuntime, TEXT("Consumer"), BladeUnitId, Result, &Error)))
	{
		return true;
	}
	Blade = FindUnit(ArmorRuntime, BladeUnitId);
	TestEqual(TEXT("all ten consumed Armor points are restored after resolution"), Blade ? Blade->Armor : INDEX_NONE, 10);
	TestTrue(TEXT("internal restore-consumption Armor rollback emits no gain audit packet"), Result.ArmorResults.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKYinXueFinishHealsCappedBleedRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.BloodEdge.YinXueFinishHealsBladeBleedUpToTwelve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKYinXueFinishHealsCappedBleedRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerBloodEdgeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.YinXueDao"), 0),
		MakeCard(TEXT("Trigger"), TEXT("Profession.Blade.JiYuLianZhan"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Blade = FindUnit(Runtime, BladeUnitId);
	TestNotNull(TEXT("the Yin Xue Finish owner exists"), Blade);
	if (!Blade)
	{
		return true;
	}
	Blade->HP = 50;
	Blade->MaxHP = 100;

	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(
		FString::Printf(TEXT("Yin Xue resolves as the final active card: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Finisher"), EnemyUnitId, Result, &Error))
		|| !EndRoundAndBeginNext(*this, Runtime))
	{
		return true;
	}
	TestEqual(TEXT("Yin Xue Finish enters the next player round"),
		Runtime.PendingBladeFinish.Rule, EGameXXKBladeFinishRule::HealBladeBleedCapTwelve);
	TestEqual(TEXT("Yin Xue Finish begins with a twelve-health healing budget"),
		Runtime.PendingBladeFinish.RemainingTriggers, 12);

	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(
		FString::Printf(TEXT("Ji Yu triggers three Blade Bleed packets inside the healing window: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Trigger"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	Blade = FindUnit(Runtime, BladeUnitId);
	TestNotNull(TEXT("the Blade survives its Finish healing window"), Blade);
	TestEqual(TEXT("five, four, and three Bleed damage heal exactly the twelve-point cap"),
		Blade ? Blade->HP : INDEX_NONE, 62);
	TestEqual(TEXT("the healing Finish clears immediately when its budget reaches zero"),
		Runtime.PendingBladeFinish.Rule, EGameXXKBladeFinishRule::None);
	TestEqual(TEXT("the three-hit trigger still audits three direct and three Bleed packets"), Result.DamageResults.Num(), 6);
	TestEqual(TEXT("the three Bleed triggers each emit one healing attempt packet"), Result.HealingResults.Num(), 3);
	if (Result.HealingResults.Num() == 3)
	{
		const int32 ExpectedHealing[3] = {5, 4, 3};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const FString Context = FString::Printf(TEXT("Yin Xue Finish healing packet %d"), Index);
			TestEqual(Context + TEXT(" keeps the Blade source"), Result.HealingResults[Index].SourceUnitId, FName(BladeUnitId));
			TestEqual(Context + TEXT(" keeps the Blade target"), Result.HealingResults[Index].TargetUnitId, FName(BladeUnitId));
			TestEqual(Context + TEXT(" records its capped request"), Result.HealingResults[Index].RequestedHealing, ExpectedHealing[Index]);
			TestEqual(Context + TEXT(" records its effective healing"), Result.HealingResults[Index].EffectiveHealing, ExpectedHealing[Index]);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLangDuanChargeBranchesByTargetModeRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.BloodEdge.LangDuanChargeDuplicatesSingleTargetOrDrawsTwo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLangDuanChargeBranchesByTargetModeRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerBloodEdgeRuntimeTest;

	FGameXXKCardBattleRuntime SingleTargetRuntime;
	const TArray<FGameXXKCardInstance> SingleTargetCards = {
		MakeCard(TEXT("LangDuan"), TEXT("Profession.Blade.LangDuan"), 0),
		MakeCard(TEXT("Duplicated"), TEXT("Profession.Blade.FengHou"), 1)};
	if (!BuildExactRuntime(*this, SingleTargetRuntime, SingleTargetCards))
	{
		return false;
	}
	SingleTargetRuntime.Units.Add(MakeUnit(
		SecondEnemyUnitId,
		EGameXXKCardTargetSide::Enemy,
		EGameXXKCharacterRole::Invalid,
		1000,
		10,
		0,
		11));
	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(TEXT("Lang Duan resolves before a single-target card"),
		GameXXKCardRules::ResolveCardPlay(SingleTargetRuntime, TEXT("LangDuan"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Lang Duan arms its branch-by-target-mode Charge"),
		SingleTargetRuntime.PendingBladeCharge.Rule, EGameXXKBladeChargeRule::DuplicateNextSingleTargetOrDraw);
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("Feng Hou resolves once on its selected target"),
		GameXXKCardRules::ResolveCardPlay(SingleTargetRuntime, TEXT("Duplicated"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	const FGameXXKCardCombatUnit* FirstEnemy = FindUnit(SingleTargetRuntime, EnemyUnitId);
	const FGameXXKCardCombatUnit* SecondEnemy = FindUnit(SingleTargetRuntime, SecondEnemyUnitId);
	TestNotNull(TEXT("the selected target remains available"), FirstEnemy);
	TestNotNull(TEXT("the automatic same-side target remains available"), SecondEnemy);
	TestEqual(TEXT("the selected enemy receives Lang Duan and the original Feng Hou hit"),
		FirstEnemy ? FirstEnemy->HP : INDEX_NONE, 960);
	TestEqual(TEXT("the stable second enemy receives exactly one automatic Feng Hou base hit"),
		SecondEnemy ? SecondEnemy->HP : INDEX_NONE, 980);
	TestEqual(TEXT("the original Feng Hou applies five Bleed to its selected enemy"),
		FirstEnemy ? GameXXKCardRules::GetCombatStatusStacks(*FirstEnemy, EGameXXKCardStatus::Bleed) : INDEX_NONE, 5);
	TestEqual(TEXT("the duplicated Feng Hou applies five Bleed to the second enemy"),
		SecondEnemy ? GameXXKCardRules::GetCombatStatusStacks(*SecondEnemy, EGameXXKCardStatus::Bleed) : INDEX_NONE, 5);
	TestEqual(TEXT("the duplicated base does not count as a third active card"), SingleTargetRuntime.ActiveCardsPlayedThisRound, 2);

	FGameXXKCardBattleRuntime DrawRuntime;
	const TArray<FGameXXKCardInstance> DrawCards = {
		MakeCard(TEXT("LangDuan"), TEXT("Profession.Blade.LangDuan"), 0),
		MakeCard(TEXT("NonSingle"), TEXT("Hero.Generic.XingQiHuiHuan"), 1),
		MakeCard(TEXT("Filler1"), TEXT("Profession.Blade.FengHou"), 2),
		MakeCard(TEXT("Filler2"), TEXT("Profession.Blade.FengHou"), 3),
		MakeCard(TEXT("Filler3"), TEXT("Profession.Blade.FengHou"), 4),
		MakeCard(TEXT("Filler4"), TEXT("Profession.Blade.FengHou"), 5),
		MakeCard(TEXT("Filler5"), TEXT("Profession.Blade.FengHou"), 6),
		MakeCard(TEXT("Filler6"), TEXT("Profession.Blade.FengHou"), 7)};
	if (!BuildExactRuntime(*this, DrawRuntime, DrawCards))
	{
		return false;
	}
	for (int32 HandIndex = DrawRuntime.Deck.Hand.Num() - 1; HandIndex >= 0; --HandIndex)
	{
		const FName InstanceId = DrawRuntime.Deck.Hand[HandIndex].InstanceId;
		if (InstanceId == TEXT("LangDuan") || InstanceId == TEXT("NonSingle"))
		{
			continue;
		}
		DrawRuntime.Deck.DrawPile.Add(MoveTemp(DrawRuntime.Deck.Hand[HandIndex]));
		DrawRuntime.Deck.Hand.RemoveAt(HandIndex, 1, EAllowShrinking::No);
	}
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("Lang Duan resolves before a non-single-target card"),
		GameXXKCardRules::ResolveCardPlay(DrawRuntime, TEXT("LangDuan"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("the no-target cycle card resolves normally"),
		GameXXKCardRules::ResolveCardPlay(DrawRuntime, TEXT("NonSingle"), NAME_None, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("the base two-card draw plus Lang Duan fallback draw exactly four cards"), DrawRuntime.Deck.Hand.Num(), 4);
	TestEqual(TEXT("the non-single fallback is still only the second active play"), DrawRuntime.ActiveCardsPlayedThisRound, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLangDuanFinishReturnsBleedingTargetCardRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.BloodEdge.LangDuanFinishReturnsFirstActiveAgainstBleeding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLangDuanFinishReturnsBleedingTargetCardRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerBloodEdgeRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Setup"), TEXT("Profession.Blade.FengHou"), 0),
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.LangDuan"), 1),
		MakeCard(TEXT("NonQualifying"), TEXT("Profession.Blade.HuiFengJiaShi"), 2),
		MakeCard(TEXT("Qualifying"), TEXT("Profession.Blade.FengHou"), 3)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildExactRuntime(*this, Runtime, Cards))
	{
		return false;
	}

	FString Error;
	FGameXXKCardPlayResult Result;
	if (!TestTrue(TEXT("Feng Hou establishes Bleed before Lang Duan"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Setup"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	const FGameXXKCardCombatUnit* BleedingEnemy = FindUnit(Runtime, EnemyUnitId);
	TestTrue(TEXT("the enemy is bleeding immediately after Feng Hou"),
		BleedingEnemy && GameXXKCardRules::GetCombatStatusStacks(*BleedingEnemy, EGameXXKCardStatus::Bleed) > 0);
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("Lang Duan resolves as the final active card"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Finisher"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	BleedingEnemy = FindUnit(Runtime, EnemyUnitId);
	TestNotNull(TEXT("the enemy remains addressable immediately after Lang Duan"), BleedingEnemy);
	TestEqual(TEXT("Lang Duan preserves all five Bleed immediately after its own trigger"),
		BleedingEnemy ? GameXXKCardRules::GetCombatStatusStacks(*BleedingEnemy, EGameXXKCardStatus::Bleed) : INDEX_NONE,
		5);
	TestEqual(TEXT("the combined Lang Duan play emits one direct packet and one Bleed packet"), Result.DamageResults.Num(), 2);

	TArray<FGameXXKCardDamageResult> RoundBoundaryDamage;
	Error.Reset();
	if (!TestTrue(TEXT("the Lang Duan player phase ends"),
		GameXXKCardRules::EndPlayerCardPhase(Runtime, RoundBoundaryDamage, &Error)))
	{
		return true;
	}
	BleedingEnemy = FindUnit(Runtime, EnemyUnitId);
	TestTrue(TEXT("Bleed survives the player-to-enemy phase boundary"),
		BleedingEnemy && GameXXKCardRules::GetCombatStatusStacks(*BleedingEnemy, EGameXXKCardStatus::Bleed) > 0);

	Error.Reset();
	if (!TestTrue(TEXT("the Lang Duan next player round begins"),
		GameXXKCardRules::BeginNextPlayerCardRound(Runtime, RoundBoundaryDamage, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Lang Duan Finish waits for a Bleed-targeting active card"),
		Runtime.PendingBladeFinish.Rule, EGameXXKBladeFinishRule::ReturnFirstActiveAgainstBleeding);
	BleedingEnemy = FindUnit(Runtime, EnemyUnitId);
	TestNotNull(TEXT("the Lang Duan Finish target survives into the next round"), BleedingEnemy);
	TestTrue(TEXT("the qualifying enemy is still bleeding before the card is played"),
		BleedingEnemy && GameXXKCardRules::GetCombatStatusStacks(*BleedingEnemy, EGameXXKCardStatus::Bleed) > 0);
	TestEqual(TEXT("the Lang Duan Finish trigger round matches the active player round"),
		Runtime.PendingBladeFinish.TriggerPlayerRound, Runtime.RoundNumber);
	const FGameXXKCardInstance* NonQualifyingCard = Runtime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == TEXT("NonQualifying");
	});
	TestNotNull(TEXT("the non-qualifying self card is drawn next round"), NonQualifyingCard);
	if (!NonQualifyingCard)
	{
		return true;
	}
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("a self card resolves before the first Bleed-targeting active card"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("NonQualifying"), NAME_None, Result, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("a non-qualifying active card does not consume Lang Duan Finish"),
		Runtime.PendingBladeFinish.Rule, EGameXXKBladeFinishRule::ReturnFirstActiveAgainstBleeding);

	const FGameXXKCardInstance* QualifyingCard = Runtime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == TEXT("Qualifying");
	});
	TestNotNull(TEXT("the qualifying exact card is drawn next round"), QualifyingCard);
	if (!QualifyingCard)
	{
		return true;
	}
	const int32 EnergyBefore = Runtime.Deck.SharedEnergy;
	Error.Reset();
	Result = FGameXXKCardPlayResult();
	if (!TestTrue(TEXT("the first active card against the bleeding enemy resolves"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Qualifying"), EnemyUnitId, Result, &Error)))
	{
		return true;
	}
	const FGameXXKCardInstance* Returned = Runtime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == TEXT("Qualifying");
	});
	TestNotNull(TEXT("Lang Duan Finish returns the exact qualifying instance"), Returned);
	TestEqual(TEXT("the returned qualifying card keeps its original non-temporary identity"),
		Returned ? Returned->bTemporary : true, false);
	TestEqual(TEXT("the qualifying card still pays its original one Energy"), Runtime.Deck.SharedEnergy, EnergyBefore - 1);
	TestEqual(TEXT("Lang Duan Finish is consumed after returning the qualifying card"),
		Runtime.PendingBladeFinish.Rule, EGameXXKBladeFinishRule::None);
	return true;
}

#endif
