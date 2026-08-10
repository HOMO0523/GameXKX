#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHeroCounterBlockRuntimeTest
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
		Unit.MaxHP = FMath::Max(1, HP);
		Unit.Attack = Attack;
		Unit.Defense = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 20 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeCard()
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = TEXT("Counter.Foundation.Card");
		Card.CardId = TEXT("Route.General.PoJiaTuCi");
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = HeroUnitId;
		Card.SourceEntryId = TEXT("Counter.Foundation.Source");
		Card.AcquisitionOrdinal = 0;
		return Card;
	}

	FGameXXKBattleRuntimeUnit MakeLegacyUnit(const FGameXXKCardCombatUnit& Unit)
	{
		FGameXXKBattleRuntimeUnit Legacy;
		Legacy.Id = Unit.UnitId;
		Legacy.DisplayName = FText::FromName(Unit.UnitId);
		Legacy.HP = Unit.HP;
		Legacy.MaxHP = Unit.MaxHP;
		Legacy.MP = Unit.Mana;
		Legacy.MaxMP = Unit.MaxMana;
		Legacy.Attack = Unit.Attack;
		Legacy.Defense = Unit.Defense;
		Legacy.Speed = Unit.Speed;
		Legacy.Shield = Unit.Armor;
		Legacy.bEnemy = Unit.Side == EGameXXKCardTargetSide::Enemy;
		Legacy.bDefeated = !Unit.bLiving;
		Legacy.BattleSlotNumber = Unit.Side == EGameXXKCardTargetSide::Enemy ? 1 : INDEX_NONE;
		return Legacy;
	}

	bool MakeRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const bool bIncludeAlly = false,
		const int32 HeroHP = 100,
		const int32 HeroAttack = 12,
		const int32 EnemyHP = 200,
		const int32 EnemyAttack = 10,
		const int32 CombatSeed = 1)
	{
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(HeroUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, HeroHP, HeroAttack, 1)};
		if (bIncludeAlly)
		{
			Units.Add(MakeUnit(AllyUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 100, 9, 2));
		}
		Units.Add(MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, EnemyHP, EnemyAttack, 10));
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			{MakeCard()},
			Units,
			EGameXXKCardTerrain::Plain,
			51001,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("counter/block runtime failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Phase = EGameXXKCardBattlePhase::Enemy;
		OutRuntime.CombatRandomState = CombatSeed;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("counter/block runtime is invalid: %s"), *Error));
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

	void AddReaction(
		FGameXXKCardBattleRuntime& Runtime,
		const FName RecipientUnitId,
		const EGameXXKCardStatus StatusType,
		const FName GrantedByUnitId = HeroUnitId,
		const TCHAR* SourceCardInstanceId = TEXT("Reaction.Source"))
	{
		const int32 Ordinal = Runtime.NextReactionOrdinal++;
		FGameXXKReactionRuntime& Reaction = Runtime.Reactions.AddDefaulted_GetRef();
		Reaction.ReactionId = FName(*FString::Printf(TEXT("Reaction.%d"), Ordinal));
		Reaction.Status = StatusType;
		Reaction.RecipientUnitId = RecipientUnitId;
		Reaction.GrantedByUnitId = GrantedByUnitId;
		Reaction.SourceCardInstanceId = FName(SourceCardInstanceId);
		Reaction.RemainingTriggers = 1;
		Reaction.ExpireBeforePlayerRound = Runtime.RoundNumber + 1;
		GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, RecipientUnitId), StatusType, 1);
	}

	FGameXXKCardEnemyIntent MakeIntent(
		const TArray<FName>& TargetUnitIds,
		const int32 Damage,
		const int32 HitCount = 1)
	{
		FGameXXKCardEnemyIntent Intent;
		Intent.CardId = TEXT("Test.Enemy.Intent");
		Intent.CardDisplayName = TEXT("Test Intent");
		Intent.SourceUnitId = EnemyUnitId;
		Intent.SuggestedTargetUnitId = TargetUnitIds.IsEmpty() ? NAME_None : TargetUnitIds[0];
		Intent.Damage = Damage;
		Intent.Kind = TargetUnitIds.Num() > 1
			? EGameXXKCardDamageKind::GroupAttack
			: EGameXXKCardDamageKind::SingleTargetAttack;
		Intent.ResolutionOrder = 0;
		FGameXXKResolvedEnemyIntentEffect& Effect = Intent.Effects.AddDefaulted_GetRef();
		Effect.Type = EGameXXKEnemyIntentEffectType::DirectDamage;
		Effect.TargetUnitIds = TargetUnitIds;
		Effect.Magnitude = Damage;
		Effect.BaseMagnitude = Damage;
		Effect.HitCount = HitCount;
		Effect.TargetRule = TargetUnitIds.Num() > 1
			? EGameXXKEnemyIntentTargetRule::AllLivingParty
			: EGameXXKEnemyIntentTargetRule::LowestHealthParty;
		return Intent;
	}

	FGameXXKRuntimeState MakeState(
		FGameXXKCardBattleRuntime Runtime,
		FGameXXKCardEnemyIntent Intent)
	{
		FGameXXKRuntimeState State;
		State.bHasActiveBattle = true;
		State.CardRun.bHasActiveCardBattle = true;
		State.CardRun.ActiveBattleSourceNodeId = 1;
		State.CardRun.ActiveBattle = MoveTemp(Runtime);
		State.CardRun.EnemyIntents = {MoveTemp(Intent)};
		State.CardRun.NextEnemyIntentIndex = 0;
		for (const FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
		{
			(Unit.Side == EGameXXKCardTargetSide::Party ? State.ActiveBattleParty : State.ActiveBattleEnemies)
				.Add(MakeLegacyUnit(Unit));
		}
		return State;
	}

	bool ResolveSavedIntent(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		TArray<FGameXXKCardDamageResult>& OutResults,
		const TCHAR* Context)
	{
		FGameXXKCardEnemyIntent ResolvedIntent;
		bool bFinished = false;
		FString Error;
		const bool bResolved = FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(
			State,
			ResolvedIntent,
			OutResults,
			bFinished,
			&Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves: %s"), Context, *Error), bResolved);
		if (bResolved)
		{
			Test.TestTrue(FString::Printf(TEXT("%s consumes the only saved intent"), Context), bFinished);
		}
		return bResolved;
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

	const FGameXXKCardDamageResult* FindCause(
		const TArray<FGameXXKCardDamageResult>& Results,
		const EGameXXKCardDamageCause Cause)
	{
		return Results.FindByPredicate([Cause](const FGameXXKCardDamageResult& Result)
		{
			return Result.Cause == Cause;
		});
	}

	int32 NextRandomState(const int32 State)
	{
		return static_cast<int32>(static_cast<uint32>(State) * 196314165u + 907633515u);
	}

	void AddEnemyReflectModifier(FGameXXKCardBattleRuntime& Runtime)
	{
		FGameXXKCardBattleModifierRuntime& Modifier = Runtime.Modifiers.AddDefaulted_GetRef();
		Modifier.ModifierId = TEXT("Enemy.Recursive.Counter");
		Modifier.SourceCardInstanceId = Runtime.Deck.ActiveInstanceIds[0];
		Modifier.SourceUnitId = EnemyUnitId;
		Modifier.RecipientUnitIds = {EnemyUnitId};
		Modifier.Definition.Trigger = EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound;
		Modifier.Definition.EffectType = EGameXXKCardEffectType::DamagePercentAttack;
		Modifier.Definition.Target = EGameXXKCardEffectTarget::Attacker;
		Modifier.Definition.RecipientScope = EGameXXKCardModifierRecipientScope::CardOwner;
		Modifier.Definition.RecipientTarget = EGameXXKCardEffectTarget::CardOwner;
		Modifier.Definition.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
		Modifier.Definition.Magnitude = 100;
		Modifier.Definition.RemainingTriggers = 1;
		Modifier.Definition.bPersistent = true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRegisterReactionCardCreatesSourceTest,
	"GameXXK.Data.HeroCards.CounterBlock.RegisterReactionCardCreatesIndependentSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRegisterReactionCardCreatesSourceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	TArray<FGameXXKCardCombatUnit> Units = {
		MakeUnit(HeroUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 12, 1),
		MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 200, 10, 10)};
	FGameXXKCardInstance Card = MakeCard();
	Card.CardId = TEXT("Hero.Generic.HengJianShouShi");
	FGameXXKCardBattleRuntime Runtime;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("reaction-card runtime initializes: %s"), *Error),
		GameXXKCardRules::InitializeCardBattleRuntime(Runtime, {Card}, Units, EGameXXKCardTerrain::Plain, 51000, &Error)))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!TestTrue(FString::Printf(TEXT("Heng Jian registers its Block source: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, Runtime.Deck.Hand[0].InstanceId, HeroUnitId, Result, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("RegisterReaction creates one independent record"), Runtime.Reactions.Num(), 1);
	if (Runtime.Reactions.Num() == 1)
	{
		const FGameXXKReactionRuntime& Reaction = Runtime.Reactions[0];
		TestEqual(TEXT("the registered source is Block"), Reaction.Status, EGameXXKCardStatus::Block);
		TestEqual(TEXT("the registered source belongs to the selected recipient"), Reaction.RecipientUnitId, HeroUnitId);
		TestEqual(TEXT("the registered source records its granting card owner"), Reaction.GrantedByUnitId, HeroUnitId);
		TestEqual(TEXT("the registered source records its exact card instance"), Reaction.SourceCardInstanceId, Card.InstanceId);
		TestEqual(TEXT("the registered source expires before the next player round"), Reaction.ExpireBeforePlayerRound, 2);
	}
	TestEqual(TEXT("the visible Block count mirrors the independent record"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Block), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCounterDealsCurrentAttackTest,
	"GameXXK.Data.HeroCards.CounterBlock.CounterDealsCurrentAttackAndConsumesOneSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCounterDealsCurrentAttackTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!MakeRuntime(*this, Runtime)) return false;
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Counter);
	FGameXXKRuntimeState State = MakeState(MoveTemp(Runtime), MakeIntent({HeroUnitId}, 3));
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveSavedIntent(*this, State, Results, TEXT("Counter intent"))) return true;
	TestEqual(TEXT("one Counter packet follows the enemy packet"), CountCause(Results, EGameXXKCardDamageCause::Counter), 1);
	const FGameXXKCardDamageResult* Counter = FindCause(Results, EGameXXKCardDamageCause::Counter);
	if (Counter)
	{
		TestEqual(TEXT("Counter deals the recipient's current Attack"), Counter->BaseRequestedDamage, 12);
		TestEqual(TEXT("Counter is explicitly reaction-origin damage"), Counter->ResolutionOrigin, EGameXXKCardResolutionOrigin::Reaction);
	}
	TestEqual(TEXT("one Counter source is consumed"), State.CardRun.ActiveBattle.Reactions.Num(), 0);
	TestEqual(TEXT("visible Counter synchronizes to zero"), Status(State.CardRun.ActiveBattle, HeroUnitId, EGameXXKCardStatus::Counter), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBlockDealsAttackPlusArmorTest,
	"GameXXK.Data.HeroCards.CounterBlock.BlockDealsCurrentAttackPlusCurrentArmorWithoutConsumingArmor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBlockDealsAttackPlusArmorTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!MakeRuntime(*this, Runtime)) return false;
	FindUnit(Runtime, HeroUnitId)->Armor = 7;
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Block);
	FGameXXKRuntimeState State = MakeState(MoveTemp(Runtime), MakeIntent({HeroUnitId}, 2));
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveSavedIntent(*this, State, Results, TEXT("Block intent"))) return true;
	TestEqual(TEXT("one Block packet follows the enemy packet"), CountCause(Results, EGameXXKCardDamageCause::Block), 1);
	const FGameXXKCardDamageResult* Block = FindCause(Results, EGameXXKCardDamageCause::Block);
	if (Block)
	{
		TestEqual(TEXT("Block uses Attack plus the five post-hit Armor"), Block->BaseRequestedDamage, 17);
	}
	TestEqual(TEXT("Block never consumes the remaining Armor"), FindUnit(State.CardRun.ActiveBattle, HeroUnitId)->Armor, 5);
	TestEqual(TEXT("the Block source is consumed"), State.CardRun.ActiveBattle.Reactions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCounterAndBlockBothResolveTest,
	"GameXXK.Data.HeroCards.CounterBlock.CounterAndBlockBothResolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCounterAndBlockBothResolveTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!MakeRuntime(*this, Runtime)) return false;
	FindUnit(Runtime, HeroUnitId)->Armor = 10;
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Counter);
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Block);
	FGameXXKRuntimeState State = MakeState(MoveTemp(Runtime), MakeIntent({HeroUnitId}, 1));
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveSavedIntent(*this, State, Results, TEXT("combined reaction intent"))) return true;
	TestEqual(TEXT("combined reaction emits one Counter"), CountCause(Results, EGameXXKCardDamageCause::Counter), 1);
	TestEqual(TEXT("combined reaction emits one Block"), CountCause(Results, EGameXXKCardDamageCause::Block), 1);
	TestEqual(TEXT("enemy packet plus two reactions are audited"), Results.Num(), 3);
	TestEqual(TEXT("both reaction records are consumed"), State.CardRun.ActiveBattle.Reactions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTwoReactionSourcesResolveOnceTest,
	"GameXXK.Data.HeroCards.CounterBlock.TwoSourcesEachResolveOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTwoReactionSourcesResolveOnceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!MakeRuntime(*this, Runtime, true)) return false;
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Counter, HeroUnitId, TEXT("Reaction.Source.A"));
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Counter, AllyUnitId, TEXT("Reaction.Source.B"));
	FGameXXKRuntimeState State = MakeState(MoveTemp(Runtime), MakeIntent({HeroUnitId}, 1));
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveSavedIntent(*this, State, Results, TEXT("two-source reaction intent"))) return true;
	TestEqual(TEXT("two independent Counter sources each resolve once"), CountCause(Results, EGameXXKCardDamageCause::Counter), 2);
	TestEqual(TEXT("both source records are consumed"), State.CardRun.ActiveBattle.Reactions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKThreeHitReactionBoundaryTest,
	"GameXXK.Data.HeroCards.CounterBlock.ThreeHitEnemyCardDoesNotTripleOneSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKThreeHitReactionBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!MakeRuntime(*this, Runtime, false, 100, 12, 300, 10, 1)) return false;
	FindUnit(Runtime, HeroUnitId)->Armor = 5;
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Counter, HeroUnitId, TEXT("Counter.A"));
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Counter, HeroUnitId, TEXT("Counter.B"));
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Block, HeroUnitId, TEXT("Block.A"));
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Block, HeroUnitId, TEXT("Block.B"));
	const int32 ExpectedRandomState = NextRandomState(NextRandomState(NextRandomState(1)));
	FGameXXKRuntimeState State = MakeState(MoveTemp(Runtime), MakeIntent({HeroUnitId}, 1, 3));
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveSavedIntent(*this, State, Results, TEXT("three-hit reaction intent"))) return true;
	TestEqual(TEXT("three-hit intent audits three direct packets plus four reaction sources"), Results.Num(), 7);
	TestEqual(TEXT("Counter2 resolves exactly twice, not once per hit"), CountCause(Results, EGameXXKCardDamageCause::Counter), 2);
	TestEqual(TEXT("Block2 resolves exactly twice, not once per hit"), CountCause(Results, EGameXXKCardDamageCause::Block), 2);
	TestEqual(TEXT("every direct hit advances the independent combat stream once"), State.CardRun.ActiveBattle.CombatRandomState, ExpectedRandomState);
	TestEqual(TEXT("all four source records are consumed"), State.CardRun.ActiveBattle.Reactions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPerfectDodgeReactionTest,
	"GameXXK.Data.HeroCards.CounterBlock.PerfectDodgeStillAllowsRegisteredReaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPerfectDodgeReactionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!MakeRuntime(*this, Runtime, false, 100, 12, 200, 10, 3)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Agility, 1);
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Counter);
	FGameXXKRuntimeState State = MakeState(MoveTemp(Runtime), MakeIntent({HeroUnitId}, 20));
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveSavedIntent(*this, State, Results, TEXT("perfect-dodge intent"))) return true;
	TestEqual(TEXT("perfect dodge plus reaction produces two audits"), Results.Num(), 2);
	if (!Results.IsEmpty())
	{
		TestTrue(TEXT("roll ten is a perfect Agility dodge"), Results[0].bPerfectAgilityDodge);
		TestTrue(TEXT("perfect Agility avoids the packet"), Results[0].bAvoidedByAgility);
		TestEqual(TEXT("perfect dodge consumes one Agility"), Results[0].AgilityStacksConsumed, 1);
		TestEqual(TEXT("runtime stamps the deterministic roll"), Results[0].AgilityRollPercent, 10);
	}
	TestEqual(TEXT("a perfect dodge still permits one Counter"), CountCause(Results, EGameXXKCardDamageCause::Counter), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNormalAgilityDodgeReactionTest,
	"GameXXK.Data.HeroCards.CounterBlock.NormalAgilityDodgeStillAllowsRegisteredReaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNormalAgilityDodgeReactionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!MakeRuntime(*this, Runtime, false, 100, 12, 200, 10, 1)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Agility, 2);
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Counter);
	FGameXXKRuntimeState State = MakeState(MoveTemp(Runtime), MakeIntent({HeroUnitId}, 20));
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveSavedIntent(*this, State, Results, TEXT("normal-dodge intent"))) return true;
	if (!Results.IsEmpty())
	{
		TestTrue(TEXT("failed perfect roll still spends two layers to dodge"), Results[0].bAvoidedByAgility);
		TestFalse(TEXT("roll eighty is not perfect"), Results[0].bPerfectAgilityDodge);
		TestEqual(TEXT("normal dodge consumes two Agility"), Results[0].AgilityStacksConsumed, 2);
		TestEqual(TEXT("normal dodge records roll eighty"), Results[0].AgilityRollPercent, 80);
	}
	TestEqual(TEXT("a normal dodge still permits one Counter"), CountCause(Results, EGameXXKCardDamageCause::Counter), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFailedOneAgilityTest,
	"GameXXK.Data.HeroCards.CounterBlock.FailedOneStackAgilityLeavesTheStackAndTakesTheHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFailedOneAgilityTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!MakeRuntime(*this, Runtime, false, 100, 12, 200, 10, 1)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Agility, 1);
	FGameXXKRuntimeState State = MakeState(MoveTemp(Runtime), MakeIntent({HeroUnitId}, 20));
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveSavedIntent(*this, State, Results, TEXT("failed-one-layer intent"))) return true;
	if (!Results.IsEmpty())
	{
		TestFalse(TEXT("one layer with roll eighty does not dodge"), Results[0].bAvoidedByAgility);
		TestEqual(TEXT("failed one-layer dodge consumes nothing"), Results[0].AgilityStacksConsumed, 0);
		TestEqual(TEXT("failed one-layer dodge records roll eighty"), Results[0].AgilityRollPercent, 80);
		TestEqual(TEXT("failed one-layer dodge takes the full hit"), Results[0].HealthDamage, 20);
	}
	TestEqual(TEXT("failed one-layer dodge preserves its Agility"), Status(State.CardRun.ActiveBattle, HeroUnitId, EGameXXKCardStatus::Agility), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKAgilityCapacityTest,
	"GameXXK.Data.HeroCards.CounterBlock.AgilityAcceptsMoreThanTheLegacyTwoStackCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKAgilityCapacityTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!MakeRuntime(*this, Runtime)) return false;
	TestEqual(TEXT("Agility accepts all five requested layers"),
		GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Agility, 5), 5);
	TestEqual(TEXT("Agility stores more than the legacy cap"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Agility), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGroupIntentNoReactionTest,
	"GameXXK.Data.HeroCards.CounterBlock.GroupIntentNeverTriggers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGroupIntentNoReactionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!MakeRuntime(*this, Runtime, true)) return false;
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Counter);
	AddReaction(Runtime, AllyUnitId, EGameXXKCardStatus::Block, AllyUnitId);
	FGameXXKRuntimeState State = MakeState(MoveTemp(Runtime), MakeIntent({HeroUnitId, AllyUnitId}, 3));
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveSavedIntent(*this, State, Results, TEXT("group intent"))) return true;
	TestEqual(TEXT("group intent emits only its two target packets"), Results.Num(), 2);
	TestEqual(TEXT("group intent emits no Counter"), CountCause(Results, EGameXXKCardDamageCause::Counter), 0);
	TestEqual(TEXT("group intent emits no Block"), CountCause(Results, EGameXXKCardDamageCause::Block), 0);
	TestEqual(TEXT("group intent leaves living recipients' reactions unused"), State.CardRun.ActiveBattle.Reactions.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKReactionNoRecursionTest,
	"GameXXK.Data.HeroCards.CounterBlock.ReactionDamageCannotTriggerReactionRecursively",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKReactionNoRecursionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!MakeRuntime(*this, Runtime)) return false;
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Counter);
	AddEnemyReflectModifier(Runtime);
	FGameXXKRuntimeState State = MakeState(MoveTemp(Runtime), MakeIntent({HeroUnitId}, 1));
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveSavedIntent(*this, State, Results, TEXT("non-recursive reaction intent"))) return true;
	TestEqual(TEXT("the enemy hit and one Counter are the only packets"), Results.Num(), 2);
	TestEqual(TEXT("reaction damage does not consume the enemy's direct-hit modifier"), State.CardRun.ActiveBattle.Modifiers.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKUnusedReactionExpiryTest,
	"GameXXK.Data.HeroCards.CounterBlock.UnusedReactionExpiresBeforeNextPlayerRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKUnusedReactionExpiryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!MakeRuntime(*this, Runtime)) return false;
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Counter);
	TArray<FGameXXKCardDamageResult> EndPhaseResults;
	FString Error;
	TestTrue(FString::Printf(TEXT("the next player round begins: %s"), *Error),
		GameXXKCardRules::BeginNextPlayerCardRound(Runtime, EndPhaseResults, &Error));
	TestEqual(TEXT("unused reaction records expire before the next player round"), Runtime.Reactions.Num(), 0);
	TestEqual(TEXT("expired visible Counter synchronizes to zero"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Counter), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLethalSimultaneousReactionTest,
	"GameXXK.Data.HeroCards.CounterBlock.LethalEnemyHitAndLethalReactionProducesVictory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLethalSimultaneousReactionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!MakeRuntime(*this, Runtime, false, 5, 10, 10, 10, 1)) return false;
	AddReaction(Runtime, HeroUnitId, EGameXXKCardStatus::Counter);
	FGameXXKRuntimeState State = MakeState(MoveTemp(Runtime), MakeIntent({HeroUnitId}, 10));
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveSavedIntent(*this, State, Results, TEXT("simultaneous lethal intent"))) return true;
	TestEqual(TEXT("the enemy attack defeats the hero"), FindUnit(State.CardRun.ActiveBattle, HeroUnitId)->HP, 0);
	TestEqual(TEXT("the queued defeated hero still defeats the enemy"), FindUnit(State.CardRun.ActiveBattle, EnemyUnitId)->HP, 0);
	TestEqual(TEXT("simultaneous elimination resolves as player victory"), State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Victory);
	TestEqual(TEXT("the lethal Counter is still audited"), CountCause(Results, EGameXXKCardDamageCause::Counter), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRedirectFinalRecipientReactionTest,
	"GameXXK.Data.HeroCards.CounterBlock.RedirectUsesTheFinalSingleTargetRecipient",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRedirectFinalRecipientReactionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCounterBlockRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!MakeRuntime(*this, Runtime, true)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, AllyUnitId), EGameXXKCardStatus::RedirectSingleTargetEnemyAttack, 1);
	AddReaction(Runtime, AllyUnitId, EGameXXKCardStatus::Block, AllyUnitId);
	FGameXXKRuntimeState State = MakeState(MoveTemp(Runtime), MakeIntent({HeroUnitId}, 4));
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveSavedIntent(*this, State, Results, TEXT("redirected reaction intent"))) return true;
	if (!Results.IsEmpty())
	{
		TestEqual(TEXT("the direct packet resolves on the redirecting ally"), Results[0].ResolvedTargetUnitId, AllyUnitId);
		TestTrue(TEXT("the direct packet records redirection"), Results[0].bRedirected);
	}
	TestEqual(TEXT("only the final recipient's Block triggers"), CountCause(Results, EGameXXKCardDamageCause::Block), 1);
	TestEqual(TEXT("the original selected hero takes no damage"), FindUnit(State.CardRun.ActiveBattle, HeroUnitId)->HP, 100);
	return true;
}

#endif
