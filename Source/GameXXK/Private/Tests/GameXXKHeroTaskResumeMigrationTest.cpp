#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FName ResumeHeroId(TEXT("Hero"));
	const FName ResumeEnemyId(TEXT("Enemy"));

	TArray<FName> ResumeHeroLoadout()
	{
		return {
			TEXT("Hero.Mage.YanXuLiaoYuan"), TEXT("Hero.Mage.HanXuNingChuan"),
			TEXT("Hero.Mage.LeiXuYinTing"), TEXT("Hero.Mage.GuiXuTongXuan"),
			TEXT("Hero.Generic.QingFengYiShi"), TEXT("Hero.Generic.HeYuZhan"),
			TEXT("Hero.Generic.SuiYanJi"), TEXT("Hero.Generic.GuiYuanShu")};
	}

	FGameXXKCardCombatUnit MakeResumeUnit(const FName Id, const bool bEnemy)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = Id;
		Unit.Side = bEnemy ? EGameXXKCardTargetSide::Enemy : EGameXXKCardTargetSide::Party;
		Unit.Role = bEnemy ? EGameXXKCharacterRole::Invalid : EGameXXKCharacterRole::Hero;
		Unit.bLiving = true;
		Unit.HP = Unit.MaxHP = 500;
		Unit.Attack = 30;
		Unit.Defense = 10;
		Unit.Mana = Unit.MaxMana = bEnemy ? 0 : 30;
		Unit.Speed = 10;
		Unit.CombatLevel = 1;
		Unit.StableSortOrder = bEnemy ? 10 : 1;
		return Unit;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroTaskResumeMigrationTest,
	"GameXXK.SaveMigration.HeroTaskResume.LegacyEightCardTask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroTaskResumeMigrationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = GameXXKPermanentPartyTestFixtures::MakeStartedState();
	FString Error;
	const TArray<FName> Equipped = ResumeHeroLoadout();
	if (!TestTrue(TEXT("resume fixture initializes its card run"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error))
		|| !TestTrue(TEXT("resume fixture equips four Mage and four generic Hero cards"),
			FGameXXKCardBattleAdapter::SetHeroSelectedCards(State, Equipped, &Error)))
	{
		AddError(Error);
		return false;
	}

	TArray<FGameXXKCardInstance> Cards;
	for (int32 Index = 0; Index < Equipped.Num(); ++Index)
	{
		FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
		Card.InstanceId = FName(*FString::Printf(TEXT("ResumeHero.Card.%d"), Index));
		Card.SourceEntryId = FName(*FString::Printf(TEXT("ResumeHero.Entry.%d"), Index));
		Card.CardId = Equipped[Index];
		Card.OwnerUnitId = ResumeHeroId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.AcquisitionOrdinal = Index;
	}
	if (!TestTrue(TEXT("resume fixture starts a card battle"),
		GameXXKCardRules::InitializeCardBattleRuntime(
			State.CardRun.ActiveBattle, Cards,
			{MakeResumeUnit(ResumeHeroId, false), MakeResumeUnit(ResumeEnemyId, true)},
			EGameXXKCardTerrain::Plain, 340301)))
	{
		return false;
	}
	State.CardRun.bHasActiveCardBattle = true;
	State.CardRun.bLoadoutLockedForRoute = true;
	State.CardRun.ActiveBattleSourceNodeId = INDEX_NONE;
	State.CardRun.ActiveBattle.EquippedHeroCardIds = Equipped;
	State.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Player;
	State.CardRun.ActiveBattle.RoundNumber = 1;

	FGameXXKSaveState Baseline = UGameXXKMVPRules::MakeSaveState(State);
	FGameXXKSaveState RestoredBaseline;
	FGameXXKSaveMigrationReport BaselineReport;
	const bool bBaselineRestores = FGameXXKSaveMigration::MigrateToCurrent(Baseline, RestoredBaseline, BaselineReport);
	if (!TestTrue(FString::Printf(TEXT("control save without a legacy task restores: %s"), *BaselineReport.Error), bBaselineRestores))
	{
		return false;
	}

	for (const int32 SourceVersion : {33, 34})
	{
		for (int32 Scenario = 0; Scenario < 10; ++Scenario)
		{
			const int32 CursorCases = Scenario == 3 ? 9 : Scenario == 7 ? 4 : Scenario == 8 ? 2 : 1;
			for (int32 Cursor = 0; Cursor < CursorCases; ++Cursor)
			{
		FGameXXKSaveState Legacy = Baseline;
		Legacy.SaveVersion = SourceVersion;
		FGameXXKHeroSpellTaskRuntime& Task = Legacy.RuntimeState.CardRun.ActiveBattle.HeroSpellTask;
		Task.bActive = true;
		Task.LockedHeroCardIds = Equipped;
		Task.StarterOwnerUnitId = ResumeHeroId;
		Task.StarterReward = EGameXXKHeroSpellTaskReward::Fire;
		TArray<int32> PlayedIndices = {0};
		if (Scenario == 1) PlayedIndices = {0, 4, 1, 5};
		if (Scenario == 2 || Scenario == 6 || Scenario == 8 || Scenario == 9) PlayedIndices = {0, 1, 2, 3};
		if (Scenario == 3 || Scenario == 4 || Scenario == 7) PlayedIndices = {0, 4, 1, 5, 2, 6, 3, 7};
		TArray<FName> ExpectedCompleted;
		for (const int32 PlayedIndex : PlayedIndices)
		{
			Task.CompletedHeroCardIds.Add(Equipped[PlayedIndex]);
			FGameXXKResolvedCardSnapshot& FirstPlay = Task.FirstPlayOrder.AddDefaulted_GetRef();
			FirstPlay.CardId = Equipped[PlayedIndex];
			FirstPlay.OwnerUnitId = ResumeHeroId;
			FirstPlay.Quality = EGameXXKCardQuality::Common;
			FirstPlay.OriginalTargetUnitIds = {ResumeEnemyId};
			if (PlayedIndex < 4) ExpectedCompleted.Add(Equipped[PlayedIndex]);
		}
		if (Scenario == 7)
		{
			FGameXXKResolvedCardSnapshot& CorruptGeneric = Task.FirstPlayOrder[1];
			if (Cursor == 0) CorruptGeneric.OwnerUnitId = NAME_None;
			if (Cursor == 1) CorruptGeneric.Quality = static_cast<EGameXXKCardQuality>(255);
			if (Cursor == 2) CorruptGeneric.OriginalTargetUnitIds = {FName(TEXT("Unknown.Unit"))};
			if (Cursor == 3) CorruptGeneric.PaidManaCost = -1;
		}
		const bool bReplay = Scenario == 3 || Scenario == 4 || Scenario == 7;
		const int32 OldCursor = Scenario == 4 ? 3 : Scenario == 7 ? 2 : Cursor;
		int32 ExpectedCursor = 0;
		if (bReplay)
		{
			FGameXXKAutomaticResolutionQueue& Queue = Legacy.RuntimeState.CardRun.ActiveBattle.AutomaticResolutionQueue;
			Queue.bActive = true;
			Queue.Origin = EGameXXKCardResolutionOrigin::MageTaskReplay;
			Queue.PendingCards = Task.FirstPlayOrder;
			Queue.NextCardIndex = OldCursor;
			Queue.PendingReward = Task.StarterReward;
			Queue.RewardOwnerUnitId = ResumeHeroId;
			for (int32 Index = 0; Index < OldCursor; ++Index)
			{
				if (PlayedIndices[Index] < 4) ++ExpectedCursor;
			}
		}
		FGameXXKBattleDeckState& LegacyDeck = Legacy.RuntimeState.CardRun.ActiveBattle.Deck;
		if (Scenario == 9)
		{
			FGameXXKAutomaticResolutionQueue& Queue = Legacy.RuntimeState.CardRun.ActiveBattle.AutomaticResolutionQueue;
			Queue.bActive = true;
			Queue.Origin = EGameXXKCardResolutionOrigin::AutomaticReplay;
			Queue.PendingCards = {Task.FirstPlayOrder[0]};
		}
		if (Scenario == 4)
		{
			if (LegacyDeck.Hand.IsEmpty())
			{
				LegacyDeck.Hand.Add(LegacyDeck.DrawPile.Pop(EAllowShrinking::No));
			}
			LegacyDeck.PendingChoice.Kind = EGameXXKCardPendingChoiceKind::ForcedDiscard;
			LegacyDeck.PendingChoice.Candidates = LegacyDeck.Hand;
			LegacyDeck.PendingChoice.RequiredCount = 1;
			LegacyDeck.PendingChoice.RequiredDiscardCount = 1;
		}
		if (Scenario == 5 || Scenario == 6 || Scenario == 8 || Scenario == 9)
		{
			if (!LegacyDeck.Hand.IsEmpty())
			{
				LegacyDeck.DiscardPile.Add(LegacyDeck.Hand.Pop(EAllowShrinking::No));
			}
			LegacyDeck.PendingChoice.Kind = EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand;
			LegacyDeck.PendingChoice.RequiredCount = 1;
			LegacyDeck.PendingChoice.RequiredHandPickCount = 1;
			for (const FName SearchId : {Equipped[1], Equipped[4]})
			{
				const int32 HandIndex = LegacyDeck.Hand.IndexOfByPredicate([SearchId](const FGameXXKCardInstance& Card) { return Card.CardId == SearchId; });
				if (HandIndex != INDEX_NONE)
				{
					LegacyDeck.DiscardPile.Add(LegacyDeck.Hand[HandIndex]);
					LegacyDeck.Hand.RemoveAt(HandIndex);
				}
			}
			for (const TArray<FGameXXKCardInstance>* Zone : {&LegacyDeck.DrawPile, &LegacyDeck.DiscardPile})
			{
				for (const FGameXXKCardInstance& Card : *Zone)
				{
					if (Card.CardId == Equipped[4] || (Scenario == 5 && Card.CardId == Equipped[1]))
					{
						LegacyDeck.PendingChoice.Candidates.Add(Card);
					}
				}
			}
			LegacyDeck.PendingChoice.Candidates.Sort([](const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
			{
				return Left.AcquisitionOrdinal != Right.AcquisitionOrdinal
					? Left.AcquisitionOrdinal < Right.AcquisitionOrdinal
					: Left.InstanceId.LexicalLess(Right.InstanceId);
			});
			if (!TestTrue(TEXT("legacy search fixture exposes at least one real candidate"), !LegacyDeck.PendingChoice.Candidates.IsEmpty())) return false;
			if (Scenario == 8 && Cursor == 0) LegacyDeck.PendingChoice.RequiredCount = 0;
			if (Scenario == 8 && Cursor == 1) LegacyDeck.PendingChoice.Candidates[0].SourceEntryId = NAME_None;
		}

		const FGameXXKSaveState Before = Legacy;
		FGameXXKSaveState Migrated;
		FGameXXKSaveMigrationReport Report;
		const bool bRestored = FGameXXKSaveMigration::MigrateToCurrent(Legacy, Migrated, Report);
		if (Scenario == 7 || Scenario == 8)
		{
			TestFalse(FString::Printf(TEXT("v%d malformed legacy payload scenario %d variant %d is rejected before filtering"), SourceVersion, Scenario, Cursor), bRestored);
			TestFalse(TEXT("a malformed legacy task returns an actionable error"), Report.Error.IsEmpty());
			TestTrue(TEXT("rejecting a malformed legacy task leaves its source intact"), FGameXXKSaveState::StaticStruct()->CompareScriptStruct(&Legacy, &Before, PPF_None));
			continue;
		}
		if (!TestTrue(FString::Printf(TEXT("v%d legacy Hero task scenario %d cursor %d remains loadable: %s"), SourceVersion, Scenario, Cursor, *Report.Error), bRestored))
		{
			continue;
		}
		TestTrue(TEXT("migration leaves the source save unchanged"),
			FGameXXKSaveState::StaticStruct()->CompareScriptStruct(&Legacy, &Before, PPF_None));
		const FGameXXKCardBattleRuntime& Battle = Migrated.RuntimeState.CardRun.ActiveBattle;
		TestTrue(TEXT("migration preserves the active battle"), Migrated.RuntimeState.CardRun.bHasActiveCardBattle);
		TestTrue(TEXT("migration preserves the in-progress Hero task"), Battle.HeroSpellTask.bActive);
		TestEqual(TEXT("migration keeps only the four equipped Mage requirements"), Battle.HeroSpellTask.LockedHeroCardIds.Num(), 4);
		TestEqual(TEXT("migration preserves completed Mage cards in first-play order"), Battle.HeroSpellTask.CompletedHeroCardIds, ExpectedCompleted);
		TestEqual(TEXT("migration never grants Energy for loading a task"), Battle.Deck.SharedEnergy, Before.RuntimeState.CardRun.ActiveBattle.Deck.SharedEnergy);
		TestEqual(TEXT("migration never inserts a task reward"), Battle.AutomaticResolutionQueue.bActive, bReplay || Scenario == 9);
		TestEqual(TEXT("migration preserves combat random state"), Battle.CombatRandomState, Before.RuntimeState.CardRun.ActiveBattle.CombatRandomState);
		for (int32 UnitIndex = 0; UnitIndex < Battle.Units.Num(); ++UnitIndex)
		{
			TestTrue(TEXT("migration does not heal, damage, or change unit resources"),
				FGameXXKCardCombatUnit::StaticStruct()->CompareScriptStruct(&Battle.Units[UnitIndex], &Before.RuntimeState.CardRun.ActiveBattle.Units[UnitIndex], PPF_None));
		}
		if (bReplay)
		{
			TestEqual(TEXT("replay keeps only its four Mage snapshots"), Battle.AutomaticResolutionQueue.PendingCards.Num(), 4);
			TestEqual(TEXT("replay skips already executed Mage snapshots without executing another"), Battle.AutomaticResolutionQueue.NextCardIndex, ExpectedCursor);
			TestEqual(TEXT("the pending starter reward stays pending"), Battle.AutomaticResolutionQueue.PendingReward, EGameXXKHeroSpellTaskReward::Fire);
		}
		if (Scenario <= 4)
		{
			TestTrue(TEXT("migration preserves all deck zones and pending non-search choices"),
				FGameXXKBattleDeckState::StaticStruct()->CompareScriptStruct(&Battle.Deck, &Before.RuntimeState.CardRun.ActiveBattle.Deck, PPF_None));
		}
		if (Scenario == 5)
		{
			TestEqual(TEXT("search keeps its unfinished Mage option"), Battle.Deck.PendingChoice.Candidates.Num(), 1);
			if (Battle.Deck.PendingChoice.Candidates.Num() == 1)
			{
				TestEqual(TEXT("search no longer offers a generic Hero card"), Battle.Deck.PendingChoice.Candidates[0].CardId, Equipped[1]);
			}
		}
		if (Scenario == 6)
		{
			TestEqual(TEXT("search with no unfinished Mage options does not block input"), Battle.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::None);
		}
		if (Scenario == 9)
		{
			TestEqual(TEXT("a pending unrelated replay retains a manual search continuation"), Battle.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand);
			TestEqual(TEXT("the already earned legacy search retains its last option"), Battle.Deck.PendingChoice.Candidates.Num(), 1);
			TestTrue(TEXT("migration does not execute or replace an unrelated replay"), FGameXXKAutomaticResolutionQueue::StaticStruct()->CompareScriptStruct(&Battle.AutomaticResolutionQueue, &Before.RuntimeState.CardRun.ActiveBattle.AutomaticResolutionQueue, PPF_None));
		}
		FGameXXKSaveState Reloaded;
		FGameXXKSaveMigrationReport ReloadReport;
		const bool bReloaded = FGameXXKSaveMigration::MigrateToCurrent(Migrated, Reloaded, ReloadReport);
		TestTrue(FString::Printf(TEXT("migrated task reloads: %s"), *ReloadReport.Error), bReloaded);
		if (bReloaded)
		{
			TestTrue(TEXT("task migration is idempotent"), FGameXXKSaveState::StaticStruct()->CompareScriptStruct(&Migrated, &Reloaded, PPF_None));
		}
		if ((Scenario == 5 || Scenario == 9) && Battle.Deck.PendingChoice.Candidates.Num() == 1)
		{
			FGameXXKCardBattleRuntime Continued = Battle;
			TArray<FGameXXKCardPlayResult> Results;
			const bool bChosen = GameXXKCardRules::SubmitHeroTaskSearchChoice(
				Continued, Continued.Deck.PendingChoice.Candidates[0].InstanceId, Results, &Error);
			TestTrue(FString::Printf(TEXT("the migrated search choice can actually be submitted: %s"), *Error), bChosen);
		}
		if (Scenario == 3 && Cursor == 8)
		{
			FGameXXKCardBattleRuntime Continued = Battle;
			TArray<FGameXXKCardPlayResult> Results;
			const bool bContinued = GameXXKCardRules::ResumeAutomaticResolutionQueue(Continued, Results, &Error);
			TestTrue(FString::Printf(TEXT("the pre-existing pending reward resumes after explicit continuation: %s"), *Error), bContinued);
			if (bContinued)
			{
				TestFalse(TEXT("the original task completes after its one pending reward"), Continued.HeroSpellTask.bActive);
				const FGameXXKCardBattleRuntime Once = Continued;
				Results.Reset();
				TestTrue(TEXT("an already completed replay can be resumed as a no-op"), GameXXKCardRules::ResumeAutomaticResolutionQueue(Continued, Results, &Error));
				TestTrue(TEXT("a second continuation does not duplicate the reward"), FGameXXKCardBattleRuntime::StaticStruct()->CompareScriptStruct(&Once, &Continued, PPF_None));
			}
		}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroPendingModifierMigrationTest,
	"GameXXK.SaveMigration.HeroTaskResume.LegacyPendingModifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroPendingModifierMigrationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = GameXXKPermanentPartyTestFixtures::MakeStartedState();
	FString Error;
	TArray<FName> Equipped = ResumeHeroLoadout();
	Equipped[5] = TEXT("Hero.Formation.GuanShiLuoZi");
	Equipped[7] = TEXT("Hero.Formation.LianYingBuShi");
	if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error)
		|| !FGameXXKCardBattleAdapter::SetHeroSelectedCards(State, Equipped, &Error))
	{
		AddError(Error);
		return false;
	}
	TArray<FGameXXKCardInstance> Cards;
	for (int32 Index = 0; Index < Equipped.Num(); ++Index)
	{
		FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
		Card.InstanceId = FName(*FString::Printf(TEXT("ResumeModifier.Card.%d"), Index));
		Card.SourceEntryId = FName(*FString::Printf(TEXT("ResumeModifier.Entry.%d"), Index));
		Card.CardId = Equipped[Index];
		Card.OwnerUnitId = ResumeHeroId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.AcquisitionOrdinal = Index;
	}
	FGameXXKCardBattleRuntime& Initial = State.CardRun.ActiveBattle;
	if (!GameXXKCardRules::InitializeCardBattleRuntime(Initial, Cards,
		{MakeResumeUnit(ResumeHeroId, false), MakeResumeUnit(ResumeEnemyId, true)},
		EGameXXKCardTerrain::Plain, 340302, &Error))
	{
		AddError(Error);
		return false;
	}
	State.CardRun.bHasActiveCardBattle = true;
	State.CardRun.bLoadoutLockedForRoute = true;
	State.CardRun.ActiveBattleSourceNodeId = INDEX_NONE;
	Initial.EquippedHeroCardIds = Equipped;
	Initial.Deck.Hand = Cards;
	Initial.Deck.DrawPile.Reset();
	Initial.Deck.DiscardPile.Reset();
	Initial.Deck.ExhaustPile.Reset();
	Initial.Deck.SharedEnergy = 10;
	FGameXXKCardPlayResult Registration;
	if (!GameXXKCardRules::ResolveCardPlay(Initial, Cards[7].InstanceId, NAME_None, Registration, &Error))
	{
		AddError(Error);
		return false;
	}
	if (!TestEqual(TEXT("modifier fixture registers LianYing once"), Initial.Modifiers.Num(), 1)) return false;
	FGameXXKCardBattleModifierRuntime Discount = Initial.Modifiers[0];
	Discount.ModifierId = TEXT("Modifier.2");
	Discount.SourceCardInstanceId = Cards[3].InstanceId;
	Discount.SourceCardSnapshot = FGameXXKResolvedCardSnapshot();
	Discount.SourceCardSnapshot.CardId = Cards[3].CardId;
	Discount.SourceCardSnapshot.OwnerUnitId = ResumeHeroId;
	Discount.SourceCardSnapshot.Quality = EGameXXKCardQuality::Common;
	Discount.Definition = FGameXXKCardBattleModifier();
	Discount.Definition.Trigger = EGameXXKCardBattleModifierTrigger::OnCardPlayed;
	Discount.Definition.EffectType = EGameXXKCardEffectType::ModifyEnergyCost;
	Discount.Definition.Target = EGameXXKCardEffectTarget::PlayedCard;
	Discount.Definition.RecipientScope = EGameXXKCardModifierRecipientScope::SharedDeck;
	Discount.Definition.RecipientTarget = EGameXXKCardEffectTarget::PlayedCard;
	Discount.Definition.RequiredTriggeredRole = EGameXXKCharacterRole::Hero;
	Discount.Definition.RequiredTriggeredOwnerId = TEXT("Hero");
	Discount.Definition.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
	Discount.Definition.TriggeredAttackTargetScope = EGameXXKCardTriggeredAttackTargetScope::AnyTarget;
	Discount.Definition.Magnitude = -1;
	Discount.Definition.RemainingTriggers = 1;
	Discount.Definition.bPersistent = true;
	Initial.Modifiers.Add(MoveTemp(Discount));
	Initial.NextModifierOrdinal = 3;
	if (!TestTrue(FString::Printf(TEXT("current modifier fixture validates: %s"), *Error),
		GameXXKCardRules::ValidateCardBattleRuntime(Initial, &Error))) return false;
	const FGameXXKSaveState Baseline = UGameXXKMVPRules::MakeSaveState(State);
	for (int32 CaseIndex = 0; CaseIndex < 4; ++CaseIndex)
	{
		const int32 SourceVersion = 33 + CaseIndex / 2;
		const bool bPublishedTargetedStyle = CaseIndex % 2 != 0;
		FGameXXKSaveState Legacy = Baseline;
		Legacy.SaveVersion = SourceVersion;
		FGameXXKCardBattleRuntime& Old = Legacy.RuntimeState.CardRun.ActiveBattle;
		Old.Modifiers[0].Definition.Trigger = EGameXXKCardBattleModifierTrigger::AfterEachActiveCard;
		Old.Modifiers[0].Definition.bActivePlayOnly = true;
		if (bPublishedTargetedStyle)
		{
			Old.Modifiers[0].Definition.Target = EGameXXKCardEffectTarget::SelectedTarget;
			Old.Modifiers[0].Definition.RecipientScope = EGameXXKCardModifierRecipientScope::SelectedTarget;
			Old.Modifiers[0].Definition.RecipientTarget = EGameXXKCardEffectTarget::SelectedTarget;
			Old.Modifiers[0].Definition.Magnitude = 1;
			Old.Modifiers[0].Definition.MagnitudePolicy = EGameXXKCardMagnitudePolicy::Unscaled;
			Old.Modifiers[0].Definition.RareMagnitude = INDEX_NONE;
			Old.Modifiers[0].Definition.EpicMagnitude = INDEX_NONE;
			Old.Modifiers[0].Definition.RemainingTriggers = 3;
			Old.Modifiers[0].OriginalSelectedTargetUnitId = ResumeEnemyId;
			Old.Modifiers[0].RecipientUnitIds = {ResumeEnemyId};
			Old.Modifiers[0].SourceCardSnapshot.OriginalTargetUnitIds = {ResumeEnemyId};
		}
		Old.Modifiers[1].Definition.Expiry = EGameXXKCardModifierExpiry::EndOfCurrentRound;
		Old.Modifiers[1].Definition.RemainingTriggers = 0;
		FGameXXKCardBattleRuntime Expected = Old;
		Expected.Modifiers[0].Definition = Baseline.RuntimeState.CardRun.ActiveBattle.Modifiers[0].Definition;
		Expected.Modifiers[0].OriginalSelectedTargetUnitId = NAME_None;
		Expected.Modifiers[0].RecipientUnitIds.Reset();
		Expected.Modifiers[1].Definition.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
		Expected.Modifiers[1].Definition.RemainingTriggers = 1;
		FGameXXKSaveState Restored;
		FGameXXKSaveMigrationReport Report;
		if (!TestTrue(FString::Printf(TEXT("v%d pending modifiers migrate"), SourceVersion),
			FGameXXKSaveMigration::MigrateToCurrent(Legacy, Restored, Report)))
		{
			AddError(Report.Error);
			continue;
		}
		FGameXXKCardBattleRuntime& Battle = Restored.RuntimeState.CardRun.ActiveBattle;
		TestTrue(TEXT("migration changes only the obsolete timing policies and performs no effects"),
			FGameXXKCardBattleRuntime::StaticStruct()->CompareScriptStruct(&Expected, &Battle, PPF_None));
		FGameXXKSaveState Again;
		FGameXXKSaveMigrationReport AgainReport;
		TestTrue(TEXT("modifier migration can be repeated"), FGameXXKSaveMigration::MigrateToCurrent(Restored, Again, AgainReport));
		TestTrue(TEXT("repeated modifier migration is idempotent"),
			FGameXXKCardBattleRuntime::StaticStruct()->CompareScriptStruct(&Battle, &Again.RuntimeState.CardRun.ActiveBattle, PPF_None));
		FGameXXKCardPlayResult Played;
		if (!TestTrue(TEXT("ordinary Hero card resumes after loading"),
			GameXXKCardRules::ResolveCardPlay(Battle, Cards[6].InstanceId, ResumeEnemyId, Played, &Error))) continue;
		const FGameXXKCardCombatUnit* Enemy = Battle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == ResumeEnemyId; });
		TestEqual(TEXT("ordinary card does not spend the pending terrain benefit"), GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Burn), 0);
		TestEqual(TEXT("the first Hero consumes its one discount"), Battle.Deck.SharedEnergy, 9);
		TestEqual(TEXT("only the terrain override remains"), Battle.Modifiers.Num(), 1);
		if (!TestTrue(TEXT("a real terrain card resumes after loading"),
			GameXXKCardRules::ResolveCardPlay(Battle, Cards[5].InstanceId, ResumeEnemyId, Played, &Error))) continue;
		Enemy = Battle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == ResumeEnemyId; });
		TestEqual(TEXT("the next real Plain benefit runs twice"), GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Burn), 4);
		TestEqual(TEXT("the following Hero pays ordinary Energy"), Battle.Deck.SharedEnergy, 8);
		TestTrue(TEXT("both one-use modifiers finish"), Battle.Modifiers.IsEmpty());
	}
	return true;
}

#endif
