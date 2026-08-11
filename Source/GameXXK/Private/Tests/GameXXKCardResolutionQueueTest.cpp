#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKCardResolutionQueueTest
{
	const FName HeroUnitId(TEXT("Hero"));
	const FName EnemyUnitId(TEXT("Enemy"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 HP,
		const int32 MaxHP,
		const int32 Attack,
		const int32 Mana,
		const int32 MaxMana,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = HP > 0;
		Unit.HP = HP;
		Unit.MaxHP = MaxHP;
		Unit.Attack = Attack;
		Unit.Mana = Mana;
		Unit.MaxMana = MaxMana;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKBattleRuntimeUnit MakeLegacyUnit(
		const FName UnitId,
		const bool bEnemy,
		const FName EnemyDefinitionId = NAME_None)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = UnitId;
		Unit.DisplayName = FText::FromName(UnitId);
		Unit.HP = bEnemy ? 46 : 100;
		Unit.MaxHP = Unit.HP;
		Unit.MP = bEnemy ? 0 : 20;
		Unit.MaxMP = Unit.MP;
		Unit.Attack = bEnemy ? 8 : 20;
		Unit.Defense = 0;
		Unit.Speed = bEnemy ? 8 : 10;
		Unit.bEnemy = bEnemy;
		Unit.EnemyDefinitionId = EnemyDefinitionId;
		Unit.BattleSlotNumber = bEnemy ? 1 : INDEX_NONE;
		Unit.CombatLevel = 1;
		return Unit;
	}

	TArray<FGameXXKCardInstance> MakeInstances(
		const FName CardId,
		const int32 Count,
		const FName OwnerUnitId = HeroUnitId)
	{
		TArray<FGameXXKCardInstance> Instances;
		Instances.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGameXXKCardInstance& Instance = Instances.AddDefaulted_GetRef();
			Instance.InstanceId = FName(*FString::Printf(TEXT("Foundation.Instance.%d"), Index));
			Instance.CardId = CardId;
			Instance.CurrentQuality = EGameXXKCardQuality::Common;
			Instance.OwnerUnitId = OwnerUnitId;
			Instance.SourceEntryId = FName(*FString::Printf(TEXT("Foundation.Entry.%d"), Index));
			Instance.AcquisitionOrdinal = Index;
		}
		return Instances;
	}

	FGameXXKResolvedCardSnapshot MakeSnapshot(
		const FName CardId,
		TArray<FName> OriginalTargetUnitIds = {})
	{
		FGameXXKResolvedCardSnapshot Snapshot;
		Snapshot.CardId = CardId;
		Snapshot.Quality = EGameXXKCardQuality::Common;
		Snapshot.OwnerUnitId = HeroUnitId;
		Snapshot.OriginalTargetUnitIds = MoveTemp(OriginalTargetUnitIds);
		return Snapshot;
	}

	bool InitializeRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		TArray<FGameXXKCardCombatUnit> Units,
		const int32 CardCount = 10,
		const FName DeckCardId = TEXT("Route.General.PoJiaTuCi"),
		const int32 Seed = 31001)
	{
		FString Error;
		const bool bInitialized = GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			MakeInstances(DeckCardId, CardCount),
			Units,
			EGameXXKCardTerrain::Plain,
			Seed,
			&Error);
		Test.TestTrue(FString::Printf(TEXT("foundation runtime initializes: %s"), *Error), bInitialized);
		return bInitialized;
	}

	TArray<FGameXXKCardCombatUnit> MakeBasicUnits(
		const int32 EnemyHP = 100,
		const int32 HeroHP = 100,
		const int32 HeroAttack = 20)
	{
		return {
			MakeUnit(HeroUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, HeroHP, 100, HeroAttack, 20, 20, 1),
			MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, EnemyHP, 100, 10, 0, 0, 10)
		};
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

	void SeedQueue(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TArray<FGameXXKResolvedCardSnapshot>& Snapshots,
		const EGameXXKCardResolutionOrigin Origin = EGameXXKCardResolutionOrigin::AutomaticReplay)
	{
		InOutRuntime.AutomaticResolutionQueue = FGameXXKAutomaticResolutionQueue();
		InOutRuntime.AutomaticResolutionQueue.bActive = true;
		InOutRuntime.AutomaticResolutionQueue.Origin = Origin;
		InOutRuntime.AutomaticResolutionQueue.PendingCards = Snapshots;
	}

	bool IsQueueDefault(const FGameXXKAutomaticResolutionQueue& Queue)
	{
		return !Queue.bActive
			&& Queue.Origin == EGameXXKCardResolutionOrigin::Invalid
			&& Queue.PendingCards.IsEmpty()
			&& Queue.NextCardIndex == 0
			&& Queue.PendingReward == EGameXXKHeroSpellTaskReward::None
			&& Queue.RewardOwnerUnitId.IsNone();
	}

	bool ZoneContains(const TArray<FGameXXKCardInstance>& Zone, const FName InstanceId)
	{
		return Zone.ContainsByPredicate([InstanceId](const FGameXXKCardInstance& Instance)
		{
			return Instance.InstanceId == InstanceId;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKExhaustedActiveCardMovesToExhaustAndNeverReshufflesTest,
	"GameXXK.Data.HeroCards.Foundation.ExhaustedActiveCardMovesToExhaustAndNeverReshuffles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKExhaustedActiveCardMovesToExhaustAndNeverReshufflesTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardResolutionQueueTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!InitializeRuntime(*this, Runtime, MakeBasicUnits(), 6, TEXT("Hero.Generic.NingShenTuNa"), 31011))
	{
		return false;
	}
	const FName PlayedInstanceId = Runtime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayResult Result;
	FString Error;
	TestTrue(FString::Printf(TEXT("the exhaust card resolves: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(Runtime, PlayedInstanceId, NAME_None, Result, &Error));
	EGameXXKCardZone PlayedZone = EGameXXKCardZone::Invalid;
	TestNotNull(TEXT("the exhausted instance remains findable in the battle ledger"),
		GameXXKCardRules::FindInstance(Runtime.Deck, PlayedInstanceId, PlayedZone));
	TestEqual(TEXT("the resolved exhaust card enters ExhaustPile"), PlayedZone, EGameXXKCardZone::ExhaustPile);
	TestEqual(TEXT("one active play is counted"), Runtime.ActiveCardsPlayedThisRound, 1);
	TestEqual(TEXT("last active card snapshots the exhaust card"), Runtime.LastActiveCard.CardId, FName(TEXT("Hero.Generic.NingShenTuNa")));
	TestEqual(TEXT("active result is explicitly stamped"), Result.ResolutionOrigin, EGameXXKCardResolutionOrigin::ActivePlay);

	TArray<FName> RemainingHandIds;
	for (const FGameXXKCardInstance& Instance : Runtime.Deck.Hand)
	{
		RemainingHandIds.Add(Instance.InstanceId);
	}
	for (const FName InstanceId : RemainingHandIds)
	{
		TestTrue(TEXT("remaining hand card can move to discard"), GameXXKCardRules::MoveHandCardToDiscard(Runtime.Deck, InstanceId, &Error));
	}
	TestTrue(TEXT("drawing through every recyclable card succeeds"), GameXXKCardRules::DrawCards(Runtime.Deck, 99, 0, &Error));
	TestTrue(TEXT("exhausted instance never enters draw"), !ZoneContains(Runtime.Deck.DrawPile, PlayedInstanceId));
	TestTrue(TEXT("exhausted instance never enters hand"), !ZoneContains(Runtime.Deck.Hand, PlayedInstanceId));
	TestTrue(TEXT("exhausted instance never enters discard"), !ZoneContains(Runtime.Deck.DiscardPile, PlayedInstanceId));
	TestTrue(TEXT("exhausted instance remains isolated from reshuffle"), ZoneContains(Runtime.Deck.ExhaustPile, PlayedInstanceId));
	TestTrue(FString::Printf(TEXT("the exhaust-aware deck remains valid: %s"), *Error), GameXXKCardRules::ValidateDeckState(Runtime.Deck, &Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKAutomaticReplayPaysNoCostAndMovesNoCardTest,
	"GameXXK.Data.HeroCards.Foundation.AutomaticReplayPaysNoCostAndMovesNoCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKAutomaticReplayPaysNoCostAndMovesNoCardTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardResolutionQueueTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!InitializeRuntime(*this, Runtime, MakeBasicUnits()))
	{
		return false;
	}
	Runtime.Deck.SharedEnergy = 0;
	const FGameXXKBattleDeckState DeckBefore = Runtime.Deck;
	SeedQueue(Runtime, {MakeSnapshot(TEXT("Route.General.PoJiaTuCi"), {EnemyUnitId})});
	TArray<FGameXXKCardPlayResult> Results;
	FString Error;
	TestTrue(FString::Printf(TEXT("automatic replay resolves at zero energy: %s"), *Error),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	TestEqual(TEXT("automatic replay reports once"), Results.Num(), 1);
	TestEqual(TEXT("automatic replay never pays shared energy"), Runtime.Deck.SharedEnergy, 0);
	TestTrue(
		TEXT("automatic replay leaves every deck zone and resource unchanged"),
		FGameXXKBattleDeckState::StaticStruct()->CompareScriptStruct(&Runtime.Deck, &DeckBefore, PPF_None));
	TestEqual(TEXT("automatic replay deals the stored card's base attack"), FindUnit(Runtime, EnemyUnitId)->HP, 80);
	if (Results.Num() == 1 && Results[0].DamageResults.Num() == 1)
	{
		TestEqual(TEXT("automatic play result has explicit origin"), Results[0].ResolutionOrigin, EGameXXKCardResolutionOrigin::AutomaticReplay);
		TestEqual(TEXT("automatic damage result has explicit origin"), Results[0].DamageResults[0].ResolutionOrigin, EGameXXKCardResolutionOrigin::AutomaticReplay);
	}
	TestTrue(TEXT("completed automatic replay clears its queue"), IsQueueDefault(Runtime.AutomaticResolutionQueue));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKAutomaticReplayDoesNotIncrementActivePlayCountTest,
	"GameXXK.Data.HeroCards.Foundation.AutomaticReplayDoesNotIncrementActivePlayCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKAutomaticReplayDoesNotIncrementActivePlayCountTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardResolutionQueueTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!InitializeRuntime(*this, Runtime, MakeBasicUnits()))
	{
		return false;
	}
	Runtime.ActiveCardsPlayedThisRound = 4;
	Runtime.LastActiveCard = MakeSnapshot(TEXT("Hero.Generic.GuanXi"));
	FGameXXKCardBattleModifierRuntime& NextAttackModifier = Runtime.Modifiers.AddDefaulted_GetRef();
	NextAttackModifier.ModifierId = TEXT("Foundation.ActiveOnly.NextAttack");
	NextAttackModifier.SourceCardInstanceId = Runtime.Deck.ActiveInstanceIds[0];
	NextAttackModifier.SourceUnitId = HeroUnitId;
	NextAttackModifier.RecipientUnitIds = {HeroUnitId};
	NextAttackModifier.Definition.Trigger = EGameXXKCardBattleModifierTrigger::OnNextAttack;
	NextAttackModifier.Definition.EffectType = EGameXXKCardEffectType::BonusDamagePercent;
	NextAttackModifier.Definition.Target = EGameXXKCardEffectTarget::PlayedCard;
	NextAttackModifier.Definition.RecipientScope = EGameXXKCardModifierRecipientScope::CardOwner;
	NextAttackModifier.Definition.RecipientTarget = EGameXXKCardEffectTarget::CardOwner;
	NextAttackModifier.Definition.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
	NextAttackModifier.Definition.Magnitude = 40;
	NextAttackModifier.Definition.RemainingTriggers = 1;
	NextAttackModifier.Definition.bPersistent = true;
	TestEqual(TEXT("fixture grants one active-play next-attack status"),
		GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::NextAttackBonus, 1), 1);
	const FGameXXKResolvedCardSnapshot LastActiveBefore = Runtime.LastActiveCard;
	SeedQueue(Runtime, {MakeSnapshot(TEXT("Route.General.PoJiaTuCi"), {EnemyUnitId})});
	TArray<FGameXXKCardPlayResult> Results;
	FString Error;
	TestTrue(FString::Printf(TEXT("automatic replay resolves: %s"), *Error),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	TestEqual(TEXT("automatic replay does not increment active-play count"), Runtime.ActiveCardsPlayedThisRound, 4);
	TestEqual(TEXT("automatic replay does not replace last active CardId"), Runtime.LastActiveCard.CardId, LastActiveBefore.CardId);
	TestEqual(TEXT("automatic replay does not replace last active owner"), Runtime.LastActiveCard.OwnerUnitId, LastActiveBefore.OwnerUnitId);
	TestTrue(TEXT("automatic replay does not replace last active targets"), Runtime.LastActiveCard.OriginalTargetUnitIds == LastActiveBefore.OriginalTargetUnitIds);
	TestEqual(TEXT("automatic replay does not consume active next-attack modifiers"), Runtime.Modifiers.Num(), 1);
	TestEqual(TEXT("automatic replay does not consume active next-attack status"),
		GameXXKCardRules::GetCombatStatusStacks(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::NextAttackBonus), 1);
	TestEqual(TEXT("automatic replay ignores the active-only attack bonus"), FindUnit(Runtime, EnemyUnitId)->HP, 80);
	TestEqual(TEXT("automatic replay does not apply the active-only on-hit Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*FindUnit(Runtime, EnemyUnitId), EGameXXKCardStatus::Mark), 0);

	FGameXXKCardBattleModifierRuntime& NextHealingModifier = Runtime.Modifiers.AddDefaulted_GetRef();
	NextHealingModifier.ModifierId = TEXT("Foundation.ActiveOnly.NextHealing");
	NextHealingModifier.SourceCardInstanceId = Runtime.Deck.ActiveInstanceIds[0];
	NextHealingModifier.SourceUnitId = HeroUnitId;
	NextHealingModifier.RecipientUnitIds = {HeroUnitId};
	NextHealingModifier.Definition.Trigger = EGameXXKCardBattleModifierTrigger::OnNextHealing;
	NextHealingModifier.Definition.EffectType = EGameXXKCardEffectType::ModifyHealingPercent;
	NextHealingModifier.Definition.Target = EGameXXKCardEffectTarget::PlayedCard;
	NextHealingModifier.Definition.RecipientScope = EGameXXKCardModifierRecipientScope::CardOwner;
	NextHealingModifier.Definition.RecipientTarget = EGameXXKCardEffectTarget::CardOwner;
	NextHealingModifier.Definition.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
	NextHealingModifier.Definition.Magnitude = 50;
	NextHealingModifier.Definition.RemainingTriggers = 1;
	NextHealingModifier.Definition.bPersistent = true;
	FindUnit(Runtime, HeroUnitId)->HP = 50;
	SeedQueue(Runtime, {MakeSnapshot(TEXT("Route.General.ZhiXueSan"), {HeroUnitId})});
	TArray<FGameXXKCardPlayResult> HealingResults;
	TestTrue(TEXT("automatic healing replay resolves"),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, HealingResults, &Error));
	TestEqual(TEXT("automatic replay does not consume active next-healing modifiers"), Runtime.Modifiers.Num(), 2);
	TestEqual(TEXT("automatic replay ignores the active-only healing bonus"), FindUnit(Runtime, HeroUnitId)->HP, 62);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDeadOriginalEnemyFallsBackByStableOrderTest,
	"GameXXK.Data.HeroCards.Foundation.DeadOriginalEnemyFallsBackByStableOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDeadOriginalEnemyFallsBackByStableOrderTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardResolutionQueueTest;
	const FName DeadEnemyId(TEXT("Enemy.Dead"));
	const FName FirstEnemyId(TEXT("Enemy.First"));
	const FName SecondEnemyId(TEXT("Enemy.Second"));
	TArray<FGameXXKCardCombatUnit> Units = {
		MakeUnit(HeroUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 10, 20, 20, 1),
		MakeUnit(DeadEnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 0, 100, 10, 0, 0, 20),
		MakeUnit(FirstEnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 5),
		MakeUnit(SecondEnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10)
	};
	FGameXXKCardBattleRuntime Runtime;
	if (!InitializeRuntime(*this, Runtime, MoveTemp(Units)))
	{
		return false;
	}
	SeedQueue(Runtime, {MakeSnapshot(TEXT("Route.General.PoJiaTuCi"), {DeadEnemyId})});
	TArray<FGameXXKCardPlayResult> Results;
	FString Error;
	TestTrue(FString::Printf(TEXT("dead-target replay resolves: %s"), *Error),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	TestEqual(TEXT("first living same-side target takes fallback damage"), FindUnit(Runtime, FirstEnemyId)->HP, 90);
	TestEqual(TEXT("later stable target remains untouched"), FindUnit(Runtime, SecondEnemyId)->HP, 100);
	TestEqual(TEXT("dead original target remains dead"), FindUnit(Runtime, DeadEnemyId)->HP, 0);
	if (Results.Num() == 1)
	{
		TestTrue(TEXT("result reports the stable fallback target"), Results[0].TargetUnitIds == TArray<FName>{FirstEnemyId});
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLethalFallbackSkipsLaterSelectedTargetEffectsTest,
	"GameXXK.Data.HeroCards.Foundation.LethalFallbackSkipsLaterSelectedTargetEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLethalFallbackSkipsLaterSelectedTargetEffectsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardResolutionQueueTest;
	const FName YueBaiUnitId(TEXT("Npc.YueBai"));
	const FName DeadEnemyId(TEXT("Enemy.Dead"));
	const FName LethalFallbackEnemyId(TEXT("Enemy.LethalFallback"));
	const FName SurvivingEnemyId(TEXT("Enemy.Survivor"));
	TArray<FGameXXKCardCombatUnit> Units = {
		MakeUnit(HeroUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 10, 20, 20, 1),
		MakeUnit(YueBaiUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 100, 100, 20, 20, 20, 2),
		MakeUnit(DeadEnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 0, 100, 10, 0, 0, 20),
		MakeUnit(LethalFallbackEnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 15, 100, 10, 0, 0, 5),
		MakeUnit(SurvivingEnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10)
	};
	FGameXXKCardBattleRuntime Runtime;
	if (!InitializeRuntime(*this, Runtime, MoveTemp(Units)))
	{
		return false;
	}
	FGameXXKResolvedCardSnapshot Snapshot;
	Snapshot.CardId = TEXT("Npc.YueBai.YueBaiZhaoYe");
	Snapshot.Quality = EGameXXKCardQuality::Common;
	Snapshot.OwnerUnitId = YueBaiUnitId;
	Snapshot.OriginalTargetUnitIds = {DeadEnemyId};
	SeedQueue(Runtime, {Snapshot});

	TArray<FGameXXKCardPlayResult> Results;
	FString Error;
	const bool bResolved = GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error);
	TestTrue(FString::Printf(TEXT("a lethal fallback target does not invalidate later effects in the same automatic card: %s"), *Error), bResolved);
	if (!bResolved)
	{
		return true;
	}
	TestEqual(TEXT("the stable fallback target keeps the committed lethal attack"), FindUnit(Runtime, LethalFallbackEnemyId)->HP, 0);
	TestEqual(TEXT("the later living enemy is not retargeted mid-card"), FindUnit(Runtime, SurvivingEnemyId)->HP, 100);
	TestEqual(TEXT("the automatic replay reports exactly one committed card"), Results.Num(), 1);
	if (Results.Num() == 1)
	{
		TestTrue(TEXT("the replay audit keeps the initially resolved fallback target"), Results[0].TargetUnitIds == TArray<FName>{LethalFallbackEnemyId});
		TestEqual(TEXT("only the lethal direct packet resolves after the target dies"), Results[0].DamageResults.Num(), 1);
	}
	TestTrue(TEXT("the completed lethal fallback replay clears its queue"), IsQueueDefault(Runtime.AutomaticResolutionQueue));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNoLegalFallbackSkipsOnlyTargetDependentEffectsTest,
	"GameXXK.Data.HeroCards.Foundation.NoLegalFallbackSkipsOnlyTargetDependentEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNoLegalFallbackSkipsOnlyTargetDependentEffectsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardResolutionQueueTest;
	TArray<FGameXXKCardCombatUnit> Units = {
		MakeUnit(HeroUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 20, 20, 1),
		MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 0, 100, 10, 0, 0, 10)
	};
	FGameXXKCardBattleRuntime Runtime;
	if (!InitializeRuntime(*this, Runtime, MoveTemp(Units)))
	{
		return false;
	}
	SeedQueue(Runtime, {MakeSnapshot(TEXT("Hero.Generic.QingFengYiShi"), {EnemyUnitId})});
	TArray<FGameXXKCardPlayResult> Results;
	FString Error;
	TestTrue(FString::Printf(TEXT("no-fallback replay still resolves non-target effects: %s"), *Error),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	TestEqual(TEXT("target-dependent attack is omitted"), Results.Num() == 1 ? Results[0].DamageResults.Num() : -1, 0);
	TestEqual(TEXT("shared-deck discount still registers"), Runtime.Modifiers.Num(), 1);
	TestEqual(TEXT("the completed event now evaluates terminal victory"), Runtime.Phase, EGameXXKCardBattlePhase::Victory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKForcedDiscardPausesAndResumesReplayQueueTest,
	"GameXXK.Data.HeroCards.Foundation.ForcedDiscardPausesAndResumesReplayQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKForcedDiscardPausesAndResumesReplayQueueTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardResolutionQueueTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!InitializeRuntime(*this, Runtime, MakeBasicUnits(10)))
	{
		return false;
	}
	SeedQueue(Runtime, {
		MakeSnapshot(TEXT("Hero.Mage.GuiXuTongXuan")),
		MakeSnapshot(TEXT("Route.General.PoJiaTuCi"), {EnemyUnitId})
	});
	TArray<FGameXXKCardPlayResult> InitialResults;
	FString Error;
	TestTrue(FString::Printf(TEXT("queue runs until forced discard: %s"), *Error),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, InitialResults, &Error));
	TestTrue(TEXT("queue remains active while forced discard is open"), Runtime.AutomaticResolutionQueue.bActive);
	TestEqual(TEXT("queue advances before the first replay resolves"), Runtime.AutomaticResolutionQueue.NextCardIndex, 1);
	TestEqual(TEXT("only the first replay has reported"), InitialResults.Num(), 1);
	TestEqual(TEXT("second replay has not damaged its target"), FindUnit(Runtime, EnemyUnitId)->HP, 10);
	TestEqual(TEXT("forced discard is the blocking choice"), Runtime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::ForcedDiscard);
	const FName DiscardedId = Runtime.Deck.PendingChoice.Candidates[0].InstanceId;
	TArray<FGameXXKCardPlayResult> ResumedResults;
	TestTrue(FString::Printf(TEXT("forced discard resumes the queued lethal replay: %s"), *Error),
		GameXXKCardRules::SubmitForcedDiscard(Runtime, {DiscardedId}, &Error, &ResumedResults));
	TestEqual(TEXT("only the newly resumed replay is exposed"), ResumedResults.Num(), 1);
	TestEqual(TEXT("resumed lethal replay resolves exactly once"), FindUnit(Runtime, EnemyUnitId)->HP, 0);
	TestTrue(TEXT("the fully resumed queue resets to default"), IsQueueDefault(Runtime.AutomaticResolutionQueue));
	TestEqual(TEXT("terminal evaluation occurs after resumed queue completion"), Runtime.Phase, EGameXXKCardBattlePhase::Victory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKInsightPausesAndResumesReplayQueueTest,
	"GameXXK.Data.HeroCards.Foundation.InsightPausesAndResumesReplayQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKInsightPausesAndResumesReplayQueueTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardResolutionQueueTest;
	FGameXXKCardBattleRuntime BaseRuntime;
	if (!InitializeRuntime(*this, BaseRuntime, MakeBasicUnits(10)))
	{
		return false;
	}
	SeedQueue(BaseRuntime, {
		MakeSnapshot(TEXT("Route.Rare.GuJuanCanZhang")),
		MakeSnapshot(TEXT("Route.General.PoJiaTuCi"), {EnemyUnitId})
	});

	FGameXXKCardBattleRuntime SubmitRuntime = BaseRuntime;
	TArray<FGameXXKCardPlayResult> InitialResults;
	FString Error;
	TestTrue(FString::Printf(TEXT("insight queue pauses: %s"), *Error),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(SubmitRuntime, InitialResults, &Error));
	TestTrue(TEXT("insight keeps the queue active"), SubmitRuntime.AutomaticResolutionQueue.bActive);
	TestEqual(TEXT("insight advances the queue before pausing"), SubmitRuntime.AutomaticResolutionQueue.NextCardIndex, 1);
	TestEqual(TEXT("insight leaves later lethal damage pending"), FindUnit(SubmitRuntime, EnemyUnitId)->HP, 10);
	TestEqual(TEXT("insight opens the expected choice"), SubmitRuntime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::InsightChooseToHand);
	const TArray<FGameXXKCardInstance> SubmitCandidates = SubmitRuntime.Deck.PendingChoice.Candidates;
	TestTrue(TEXT("insight exposes at least one safe candidate"), !SubmitCandidates.IsEmpty());
	if (SubmitCandidates.IsEmpty())
	{
		return false;
	}
	TArray<FName> RemainingOrder;
	for (int32 Index = 1; Index < SubmitCandidates.Num(); ++Index)
	{
		RemainingOrder.Add(SubmitCandidates[Index].InstanceId);
	}
	TArray<FGameXXKCardPlayResult> SubmitResumedResults;
	TestTrue(FString::Printf(TEXT("submitting insight resumes the later replay: %s"), *Error),
		GameXXKCardRules::SubmitInsightChoice(
			SubmitRuntime,
			SubmitCandidates[0].InstanceId,
			RemainingOrder,
			&Error,
			&SubmitResumedResults));
	TestEqual(TEXT("insight submission exposes only the resumed replay"), SubmitResumedResults.Num(), 1);
	TestEqual(TEXT("insight submission resumes lethal damage exactly once"), FindUnit(SubmitRuntime, EnemyUnitId)->HP, 0);
	TestTrue(TEXT("submitted insight clears the queue"), IsQueueDefault(SubmitRuntime.AutomaticResolutionQueue));

	FGameXXKCardBattleRuntime CancelRuntime = BaseRuntime;
	TArray<FGameXXKCardPlayResult> CancelInitialResults;
	TestTrue(TEXT("second insight fixture pauses"), GameXXKCardRules::ResumeAutomaticResolutionQueue(CancelRuntime, CancelInitialResults, &Error));
	const FName DrawTopBeforeCancel = GameXXKCardRules::GetDrawPileTop(CancelRuntime.Deck)->InstanceId;
	TArray<FGameXXKCardPlayResult> CancelResumedResults;
	TestTrue(FString::Printf(TEXT("cancelling insight resumes the later replay: %s"), *Error),
		GameXXKCardRules::CancelInsight(CancelRuntime, &Error, &CancelResumedResults));
	TestEqual(TEXT("insight cancellation preserves the inspected draw top"), GameXXKCardRules::GetDrawPileTop(CancelRuntime.Deck)->InstanceId, DrawTopBeforeCancel);
	TestEqual(TEXT("insight cancellation exposes only the resumed replay"), CancelResumedResults.Num(), 1);
	TestEqual(TEXT("insight cancellation resumes lethal damage exactly once"), FindUnit(CancelRuntime, EnemyUnitId)->HP, 0);
	TestTrue(TEXT("cancelled insight clears the queue"), IsQueueDefault(CancelRuntime.AutomaticResolutionQueue));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTerminalPhaseWaitsForTheWholeQueuedSequenceTest,
	"GameXXK.Data.HeroCards.Foundation.TerminalPhaseWaitsForTheWholeQueuedSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTerminalPhaseWaitsForTheWholeQueuedSequenceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardResolutionQueueTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!InitializeRuntime(*this, Runtime, MakeBasicUnits(10)))
	{
		return false;
	}
	SeedQueue(Runtime, {
		MakeSnapshot(TEXT("Route.General.PoJiaTuCi"), {EnemyUnitId}),
		MakeSnapshot(TEXT("Route.Rare.GuJuanCanZhang"))
	});
	TArray<FGameXXKCardPlayResult> Results;
	FString Error;
	TestTrue(FString::Printf(TEXT("lethal then insight queue pauses: %s"), *Error),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	TestEqual(TEXT("the first replay kills the final enemy"), FindUnit(Runtime, EnemyUnitId)->HP, 0);
	TestEqual(TEXT("terminal phase stays deferred while the queued choice is open"), Runtime.Phase, EGameXXKCardBattlePhase::Player);
	TestTrue(TEXT("the completed snapshots remain represented by an active queue"), Runtime.AutomaticResolutionQueue.bActive);
	TestEqual(TEXT("both queued snapshots advanced before the choice pause"), Runtime.AutomaticResolutionQueue.NextCardIndex, 2);
	TArray<FGameXXKCardPlayResult> ResumedResults;
	TestTrue(TEXT("cancelling the terminal insight resumes the empty tail"), GameXXKCardRules::CancelInsight(Runtime, &Error, &ResumedResults));
	TestTrue(TEXT("no unprocessed snapshot is reported after terminal insight cancellation"), ResumedResults.IsEmpty());
	TestTrue(TEXT("terminal queue resets only after the choice clears"), IsQueueDefault(Runtime.AutomaticResolutionQueue));
	TestEqual(TEXT("victory is evaluated at the whole-queue boundary"), Runtime.Phase, EGameXXKCardBattlePhase::Victory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSimultaneousEliminationChoosesPlayerVictoryTest,
	"GameXXK.Data.HeroCards.Foundation.SimultaneousEliminationChoosesPlayerVictory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSimultaneousEliminationChoosesPlayerVictoryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardResolutionQueueTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!InitializeRuntime(*this, Runtime, MakeBasicUnits(10, 8, 10)))
	{
		return false;
	}
	FGameXXKCardDamageContext EnemyHit;
	EnemyHit.SourceUnitId = HeroUnitId;
	EnemyHit.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardDamageResult EnemyDamage;
	FString Error;
	TestTrue(FString::Printf(TEXT("same-boundary lethal enemy hit resolves: %s"), *Error),
		GameXXKCardRules::ApplyCombatDirectDamage(
			Runtime.Units,
			Runtime.GuardLinks,
			EnemyHit,
			EnemyUnitId,
			10,
			EnemyDamage,
			&Error));

	FGameXXKCardDamageContext SelfLoss;
	SelfLoss.SourceUnitId = HeroUnitId;
	SelfLoss.Kind = EGameXXKCardDamageKind::SelfHealthLoss;
	FGameXXKCardDamageResult PartyDamage;
	TestTrue(FString::Printf(TEXT("same-boundary lethal party self-loss resolves: %s"), *Error),
		GameXXKCardRules::ApplyCombatDirectDamage(
			Runtime.Units,
			Runtime.GuardLinks,
			SelfLoss,
			HeroUnitId,
			8,
			PartyDamage,
			&Error));
	GameXXKCardRules::RefreshCombatTerminalPhase(Runtime);
	TestEqual(TEXT("the final enemy is defeated"), FindUnit(Runtime, EnemyUnitId)->HP, 0);
	TestEqual(TEXT("the final party member is defeated in the same resolution boundary"), FindUnit(Runtime, HeroUnitId)->HP, 0);
	TestEqual(TEXT("simultaneous elimination resolves as player victory"), Runtime.Phase, EGameXXKCardBattlePhase::Victory);
	TestTrue(TEXT("simultaneous-elimination queue clears"), IsQueueDefault(Runtime.AutomaticResolutionQueue));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTerminalPhaseWaitsForCompleteEnemyIntentTest,
	"GameXXK.Data.HeroCards.Foundation.TerminalPhaseWaitsForCompleteEnemyIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTerminalPhaseWaitsForCompleteEnemyIntentTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardResolutionQueueTest;
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("enemy-intent boundary fixture initializes its card run"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {MakeLegacyUnit(TEXT("Player"), false)};
	State.ActiveBattleEnemies = {MakeLegacyUnit(TEXT("Enemy.Rooster.P1"), true, TEXT("Enemy.Ch1.Rooster"))};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 31002;
	TestTrue(FString::Printf(TEXT("enemy-intent boundary fixture begins: %s"), *Error),
		FGameXXKCardBattleAdapter::BeginCardBattle(
			State,
			EGameXXKNodeKind::Battle,
			EGameXXKCardTerrain::Plain,
			31002,
			&Error));
	FGameXXKCardCombatUnit* Player = FindUnit(State.CardRun.ActiveBattle, TEXT("Player"));
	TestNotNull(TEXT("enemy-intent boundary fixture retains its player unit"), Player);
	if (!Player || State.CardRun.EnemyIntents.IsEmpty())
	{
		return false;
	}
	Player->HP = 1;
	Player->Armor = 0;
	Player->Defense = 0;
	TArray<FGameXXKCardDamageResult> EndPhaseResults;
	TestTrue(FString::Printf(TEXT("enemy-intent boundary fixture enters enemy phase: %s"), *Error),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, EndPhaseResults, &Error));

	FGameXXKCardEnemyIntent Intent = State.CardRun.EnemyIntents[0];
	Intent.Effects.Reset();
	FGameXXKResolvedEnemyIntentEffect& LethalEffect = Intent.Effects.AddDefaulted_GetRef();
	LethalEffect.Type = EGameXXKEnemyIntentEffectType::DirectDamage;
	LethalEffect.TargetUnitIds = {TEXT("Player")};
	LethalEffect.Magnitude = 10;
	LethalEffect.HitCount = 1;
	FGameXXKResolvedEnemyIntentEffect& FollowupEffect = Intent.Effects.AddDefaulted_GetRef();
	FollowupEffect.Type = EGameXXKEnemyIntentEffectType::IncreaseNextCardEnergy;
	FollowupEffect.Magnitude = 1;
	State.CardRun.EnemyIntents = {Intent};
	State.CardRun.NextEnemyIntentIndex = 0;

	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	TestTrue(FString::Printf(TEXT("the full saved intent resolves before terminal evaluation: %s"), *Error),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(
			State,
			ResolvedIntent,
			IntentResults,
			bIntentsFinished,
			&Error));
	TestEqual(TEXT("the lethal packet is still audited"), IntentResults.Num(), 1);
	TestEqual(TEXT("terminal cleanup removes the future-hand surcharge only after that later effect resolves"),
		State.CardRun.ActiveBattle.PendingNextPlayerHandEnergySurcharge, 0);
	TestEqual(TEXT("terminal defeat is committed only after the complete intent"),
		State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Defeat);
	TestTrue(TEXT("the complete one-enemy intent advances its saved cursor"), bIntentsFinished);
	return true;
}

#endif
