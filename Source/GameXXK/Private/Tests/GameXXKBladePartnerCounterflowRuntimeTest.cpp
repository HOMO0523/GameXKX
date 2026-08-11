#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKBladePartnerCounterflowRuntimeTest
{
	constexpr const TCHAR* BladeUnitId = TEXT("BladePartner");
	constexpr const TCHAR* EnemyAUnitId = TEXT("EnemyA");
	constexpr const TCHAR* EnemyBUnitId = TEXT("EnemyB");

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

	FGameXXKCardInstance MakeCard(const TCHAR* InstanceId, const TCHAR* CardId, const int32 Ordinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(InstanceId);
		Card.CardId = FName(CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = BladeUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Counterflow.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardInstance>& Cards)
	{
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(BladeUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 200, 20, 20, 20, 1),
			MakeUnit(EnemyAUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1000, 10, 0, 0, 10),
			MakeUnit(EnemyBUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1000, 10, 0, 0, 11)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			60101,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("Counterflow runtime failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = Cards;
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Counterflow exact fixture is invalid: %s"), *Error));
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

	const FGameXXKCardInstance* FindHandCard(FGameXXKCardBattleRuntime& Runtime, const FName InstanceId)
	{
		return Runtime.Deck.Hand.FindByPredicate([InstanceId](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == InstanceId;
		});
	}

	bool Resolve(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		const FName TargetId,
		FGameXXKCardPlayResult& OutResult,
		const TCHAR* Label)
	{
		FString Error;
		return Test.TestTrue(
			FString::Printf(TEXT("%s resolves: %s"), Label, *Error),
			GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, TargetId, OutResult, &Error));
	}

	bool EndPlayerPhase(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FGameXXKCardDamageResult> DamageResults;
		FString Error;
		return Test.TestTrue(
			FString::Printf(TEXT("Counterflow player phase ends: %s"), *Error),
			GameXXKCardRules::EndPlayerCardPhase(Runtime, DamageResults, &Error));
	}

	bool BeginNextPlayerRound(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FGameXXKCardDamageResult> DamageResults;
		FString Error;
		return Test.TestTrue(
			FString::Printf(TEXT("Counterflow next player round begins: %s"), *Error),
			GameXXKCardRules::BeginNextPlayerCardRound(Runtime, DamageResults, &Error));
	}

	bool ResolveEnemyCard(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		FGameXXKCardDamageResult& OutIncoming,
		TArray<FGameXXKCardDamageResult>& OutReactions,
		const TCHAR* Label)
	{
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = EnemyAUnitId;
		Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		FString Error;
		if (!Test.TestTrue(
			FString::Printf(TEXT("%s direct packet resolves: %s"), Label, *Error),
			GameXXKCardRules::ResolveEnemyDirectAttack(
				Runtime,
				Context,
				BladeUnitId,
				5,
				OutIncoming,
				nullptr,
				&Error,
				true)))
		{
			return false;
		}
		Error.Reset();
		return Test.TestTrue(
			FString::Printf(TEXT("%s reaction boundary resolves: %s"), Label, *Error),
			GameXXKCardRules::ResolvePartyReactionsAfterEnemyCard(
				Runtime,
				EnemyAUnitId,
				EGameXXKCardDamageKind::SingleTargetAttack,
				BladeUnitId,
				OutReactions,
				&Error));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKJieShiChargeCopiesNextActiveNextRoundTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Counterflow.JieShiChargeCopiesNextActiveNextRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKJieShiChargeCopiesNextActiveNextRoundTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerCounterflowRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.JieShiHuiFeng"), 0),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Charge"), NAME_None, Result, TEXT("Jie Shi opener"))
		|| !Resolve(*this, Runtime, TEXT("Next"), EnemyAUnitId, Result, TEXT("recorded next active")))
	{
		return true;
	}
	TestEqual(TEXT("Jie Shi Charge records the next active for the following round"),
		Runtime.PendingBladeDelayedCard.Rule,
		EGameXXKBladeChargeRule::CopyNextActiveNextRound);
	if (!EndPlayerPhase(*this, Runtime) || !BeginNextPlayerRound(*this, Runtime))
	{
		return true;
	}
	const FGameXXKCardInstance* Copy = Runtime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.CardId == TEXT("Hero.Generic.HeYuZhan") && Card.bTemporary;
	});
	if (!TestNotNull(TEXT("Jie Shi creates the recorded card's temporary copy next round"), Copy))
	{
		return true;
	}
	TestEqual(TEXT("the delayed copy costs zero Energy"), Copy->EnergyCostOverride, 0);
	TestEqual(TEXT("the delayed copy costs zero Mana"), Copy->ManaCostOverride, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKZhuYingChargeRetainsNextActiveNextRoundTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Counterflow.ZhuYingChargeRetainsNextActiveAtOriginalCost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKZhuYingChargeRetainsNextActiveNextRoundTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerCounterflowRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.ZhuYing"), 0),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Charge"), EnemyAUnitId, Result, TEXT("Zhu Ying opener"))
		|| !Resolve(*this, Runtime, TEXT("Next"), EnemyAUnitId, Result, TEXT("retained next active")))
	{
		return true;
	}
	TestEqual(TEXT("Zhu Ying Charge records the retained active card"),
		Runtime.PendingBladeDelayedCard.Rule,
		EGameXXKBladeChargeRule::RetainNextActiveNextRound);
	TestTrue(TEXT("the retained card is isolated from ordinary discard reshuffling"),
		Runtime.Deck.ExhaustPile.ContainsByPredicate([](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == TEXT("Next");
		}));
	if (!EndPlayerPhase(*this, Runtime) || !BeginNextPlayerRound(*this, Runtime))
	{
		return true;
	}
	const FGameXXKCardInstance* Returned = FindHandCard(Runtime, TEXT("Next"));
	if (!TestNotNull(TEXT("Zhu Ying returns the exact retained instance next round"), Returned))
	{
		return true;
	}
	TestFalse(TEXT("the retained card is not temporary"), Returned->bTemporary);
	TestEqual(TEXT("the retained card keeps its original Energy override"), Returned->EnergyCostOverride, INDEX_NONE);
	TestEqual(TEXT("the retained card keeps its original Mana override"), Returned->ManaCostOverride, INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoLangChargePreservesFinishCandidateTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Counterflow.PoLangChargePreservesCurrentFinishCandidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoLangChargePreservesFinishCandidateTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerCounterflowRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.PoLangTuJin"), 0),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Charge"), EnemyAUnitId, Result, TEXT("Po Lang opener"))
		|| !Resolve(*this, Runtime, TEXT("Next"), EnemyAUnitId, Result, TEXT("traceless next active"))
		|| !EndPlayerPhase(*this, Runtime))
	{
		return true;
	}
	TestEqual(TEXT("Po Lang remains the Finish candidate after its traceless next active"),
		Runtime.PendingBladeFinish.Rule,
		EGameXXKBladeFinishRule::TransferMarkBeforeCounter);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKYiShiChargeRetainsRemainingHandTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Counterflow.YiShiChargeRetainsRemainingHandNextRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKYiShiChargeRetainsRemainingHandTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerCounterflowRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.YiShiDuanJiang"), 0),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), 1),
		MakeCard(TEXT("KeepA"), TEXT("Profession.Blade.BaoDaoShouYe"), 2),
		MakeCard(TEXT("KeepB"), TEXT("Profession.Blade.LianXiGuiQiao"), 3)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Charge"), EnemyAUnitId, Result, TEXT("Yi Shi opener"))
		|| !Resolve(*this, Runtime, TEXT("Next"), EnemyAUnitId, Result, TEXT("hand-retention trigger"))
		|| !EndPlayerPhase(*this, Runtime))
	{
		return true;
	}
	TestNotNull(TEXT("Yi Shi retains KeepA through the enemy phase"), FindHandCard(Runtime, TEXT("KeepA")));
	TestNotNull(TEXT("Yi Shi retains KeepB through the enemy phase"), FindHandCard(Runtime, TEXT("KeepB")));
	if (!BeginNextPlayerRound(*this, Runtime))
	{
		return true;
	}
	TestNotNull(TEXT("Yi Shi keeps KeepA in next round's hand"), FindHandCard(Runtime, TEXT("KeepA")));
	TestNotNull(TEXT("Yi Shi keeps KeepB in next round's hand"), FindHandCard(Runtime, TEXT("KeepB")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKJieShiFinishReregistersFirstCounterVolleyTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Counterflow.JieShiFinishReregistersFirstCounterVolley",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKJieShiFinishReregistersFirstCounterVolleyTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerCounterflowRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.JieShiHuiFeng"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Finisher"), NAME_None, Result, TEXT("Jie Shi finisher"))
		|| !EndPlayerPhase(*this, Runtime))
	{
		return true;
	}
	const FGameXXKCardCombatUnit* Blade = FindUnit(Runtime, BladeUnitId);
	TestEqual(TEXT("Jie Shi Finish grants two Mark"),
		Blade ? GameXXKCardRules::GetCombatStatusStacks(*Blade, EGameXXKCardStatus::Mark) : INDEX_NONE,
		2);
	FGameXXKCardDamageResult Incoming;
	TArray<FGameXXKCardDamageResult> Reactions;
	if (!ResolveEnemyCard(*this, Runtime, Incoming, Reactions, TEXT("first Jie Shi enemy card")))
	{
		return true;
	}
	TestEqual(TEXT("the first Jie Shi volley fires its original Counter"), Reactions.Num(), 1);
	Blade = FindUnit(Runtime, BladeUnitId);
	TestEqual(TEXT("Jie Shi re-registers the consumed Counter once"),
		Blade ? GameXXKCardRules::GetCombatStatusStacks(*Blade, EGameXXKCardStatus::Counter) : INDEX_NONE,
		1);
	Reactions.Reset();
	if (!ResolveEnemyCard(*this, Runtime, Incoming, Reactions, TEXT("second Jie Shi enemy card")))
	{
		return true;
	}
	TestEqual(TEXT("the second Jie Shi volley fires the re-registered Counter"), Reactions.Num(), 1);
	Blade = FindUnit(Runtime, BladeUnitId);
	TestEqual(TEXT("Jie Shi does not re-register a third Counter"),
		Blade ? GameXXKCardRules::GetCombatStatusStacks(*Blade, EGameXXKCardStatus::Counter) : INDEX_NONE,
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKZhuYingFinishPreservesFirstTwoDodgesTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Counterflow.ZhuYingFinishMakesFirstTwoDodgesFree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKZhuYingFinishPreservesFirstTwoDodgesTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerCounterflowRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.ZhuYing"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Finisher"), EnemyAUnitId, Result, TEXT("Zhu Ying finisher"))
		|| !EndPlayerPhase(*this, Runtime))
	{
		return true;
	}
	for (int32 AttackIndex = 0; AttackIndex < 3; ++AttackIndex)
	{
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = EnemyAUnitId;
		Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		FGameXXKCardDamageResult Incoming;
		FString Error;
		if (!TestTrue(TEXT("Zhu Ying dodge probe resolves"),
			GameXXKCardRules::ResolveEnemyDirectAttack(
				Runtime,
				Context,
				BladeUnitId,
				5,
				Incoming,
				nullptr,
				&Error,
				true)))
		{
			return true;
		}
		TestTrue(TEXT("Zhu Ying has enough Agility to dodge the probe"), Incoming.bAvoidedByAgility);
		if (AttackIndex < 2)
		{
			TestEqual(TEXT("the protected dodge consumes no Agility"), Incoming.AgilityStacksConsumed, 0);
		}
		else
		{
			TestTrue(TEXT("the third dodge resumes normal Agility consumption"), Incoming.AgilityStacksConsumed > 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoLangFinishTransfersMarkBeforeCountersTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Counterflow.PoLangFinishTransfersMarkBeforeEachCounterVolley",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoLangFinishTransfersMarkBeforeCountersTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerCounterflowRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("SourceA"), TEXT("Profession.Blade.JieShiHuiFeng"), 0),
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.PoLangTuJin"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("SourceA"), NAME_None, Result, TEXT("first Counter source"))
		|| !Resolve(*this, Runtime, TEXT("Finisher"), EnemyAUnitId, Result, TEXT("Po Lang finisher"))
		|| !EndPlayerPhase(*this, Runtime))
	{
		return true;
	}
	FGameXXKCardCombatUnit* Blade = FindUnit(Runtime, BladeUnitId);
	TestEqual(TEXT("Po Lang begins the enemy phase with two Mark"),
		Blade ? GameXXKCardRules::GetCombatStatusStacks(*Blade, EGameXXKCardStatus::Mark) : INDEX_NONE,
		2);
	if (Blade)
	{
		GameXXKCardRules::AddCombatStatus(*Blade, EGameXXKCardStatus::Agility, 2);
	}
	FGameXXKCardDamageResult Incoming;
	TArray<FGameXXKCardDamageResult> Reactions;
	if (!ResolveEnemyCard(*this, Runtime, Incoming, Reactions, TEXT("Po Lang marked enemy card")))
	{
		return true;
	}
	TestEqual(TEXT("two independent Counter sources fire in the Po Lang volley"), Reactions.Num(), 2);
	for (int32 Index = 0; Index < Reactions.Num(); ++Index)
	{
		TestEqual(TEXT("each transferred Mark Counter receives the fixed fifteen-percent bonus"),
			Reactions[Index].MarkDamageBonusPercent,
			15);
		TestEqual(TEXT("each transferred Mark Counter consumes one Mark"), Reactions[Index].MarkStacksConsumed, 1);
	}
	Blade = FindUnit(Runtime, BladeUnitId);
	const FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, EnemyAUnitId);
	TestEqual(TEXT("Po Lang transfers both available Mark off the Blade"),
		Blade ? GameXXKCardRules::GetCombatStatusStacks(*Blade, EGameXXKCardStatus::Mark) : INDEX_NONE,
		0);
	TestEqual(TEXT("the two Counters consume both transferred enemy Mark"),
		Enemy ? GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Mark) : INDEX_NONE,
		0);
	TestEqual(TEXT("Po Lang Finish remains available for later volleys this enemy phase"),
		Runtime.PendingBladeFinish.Rule,
		EGameXXKBladeFinishRule::TransferMarkBeforeCounter);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKYiShiFinishMakesFirstCounterVolleyGroupTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Counterflow.YiShiFinishMakesFirstCounterVolleyHitAllEnemies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKYiShiFinishMakesFirstCounterVolleyGroupTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerCounterflowRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("SourceA"), TEXT("Profession.Blade.JieShiHuiFeng"), 0),
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.YiShiDuanJiang"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("SourceA"), NAME_None, Result, TEXT("first group-Counter source"))
		|| !Resolve(*this, Runtime, TEXT("Finisher"), EnemyAUnitId, Result, TEXT("Yi Shi finisher"))
		|| !EndPlayerPhase(*this, Runtime))
	{
		return true;
	}
	FGameXXKCardCombatUnit* Blade = FindUnit(Runtime, BladeUnitId);
	if (Blade)
	{
		GameXXKCardRules::AddCombatStatus(*Blade, EGameXXKCardStatus::Agility, 2);
	}
	FGameXXKCardDamageResult Incoming;
	TArray<FGameXXKCardDamageResult> Reactions;
	if (!ResolveEnemyCard(*this, Runtime, Incoming, Reactions, TEXT("Yi Shi group volley")))
	{
		return true;
	}
	TestEqual(TEXT("two Counter sources each hit both living enemies"), Reactions.Num(), 4);
	int32 EnemyAHits = 0;
	int32 EnemyBHits = 0;
	for (const FGameXXKCardDamageResult& Reaction : Reactions)
	{
		EnemyAHits += Reaction.ResolvedTargetUnitId == EnemyAUnitId ? 1 : 0;
		EnemyBHits += Reaction.ResolvedTargetUnitId == EnemyBUnitId ? 1 : 0;
		TestEqual(TEXT("every Yi Shi wave is audited as Counter damage"), Reaction.Cause, EGameXXKCardDamageCause::Counter);
	}
	TestEqual(TEXT("Enemy A receives one hit from each Counter source"), EnemyAHits, 2);
	TestEqual(TEXT("Enemy B receives one hit from each Counter source"), EnemyBHits, 2);
	TestEqual(TEXT("Yi Shi Finish is consumed after the first Counter volley"),
		Runtime.PendingBladeFinish.Rule,
		EGameXXKBladeFinishRule::None);
	return true;
}

#endif
