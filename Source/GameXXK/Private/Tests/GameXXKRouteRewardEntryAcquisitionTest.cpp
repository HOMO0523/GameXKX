#include "GameXXKCardBattleAdapter.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKRouteCardRecipe.h"
#include "GameXXKRunDeckRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKRouteRewardEntryAcquisitionTest
{
	const FName PlayerUnitId(TEXT("Player"));
	const FName DuplicateCardId(TEXT("Route.General.TuNaJue"));

	int32 GetRewardSourceNodeId(const FGameXXKRuntimeState& State)
	{
		return State.CardRun.ActiveBattleSourceNodeId >= 0
			? State.CardRun.ActiveBattleSourceNodeId
			: State.DungeonNodeIndex;
	}

	FString ExportState(const FGameXXKRuntimeState& State)
	{
		FString Exported;
		FGameXXKRuntimeState::StaticStruct()->ExportText(Exported, &State, nullptr, nullptr, PPF_None, nullptr);
		return Exported;
	}

	bool StartRewardReadyState(
		FGameXXKRuntimeState& OutState,
		const EGameXXKNodeKind NodeKind = EGameXXKNodeKind::Battle,
		const int32 RootSeed = 0x2468)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		if (!UGameXXKMVPRules::OpenWorldMap(OutState)
			|| !UGameXXKMVPRules::EnterWorldRegion(OutState, UGameXXKMVPRules::RegionQingshan())
			|| !UGameXXKMVPRules::AcceptTownQuest(OutState))
		{
			return false;
		}

		OutState.RouteSeed = RootSeed;
		if (!UGameXXKMVPRules::EnterDungeon(OutState))
		{
			return false;
		}

		OutState.bHasGeneratedRouteMap = false;
		OutState.RouteMapNodes.Reset();
		OutState.RouteMapEdges.Reset();
		OutState.ReachableRouteNodeIds.Reset();
		const int32 TargetNodeIndex = NodeKind == EGameXXKNodeKind::Boss
			? UGameXXKMVPRules::GetFixedDungeonNodes(0).IndexOfByKey(EGameXXKNodeKind::Boss)
			: 1;
		if (TargetNodeIndex == INDEX_NONE
			|| !(OutState.DungeonNodeIndex = TargetNodeIndex, true)
			|| !UGameXXKMVPRules::AdvanceDungeonNode(OutState, NodeKind))
		{
			return false;
		}

		for (FGameXXKCardCombatUnit& Unit : OutState.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Enemy)
			{
				Unit.HP = 0;
				Unit.bLiving = false;
			}
		}
		OutState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
		OutState.CardRun.PendingReward = FGameXXKPendingRouteCardReward();
		OutState.CardRun.bActiveBattleRewardResolved = false;
		return OutState.bDungeonActive
			&& OutState.CardRun.bLoadoutLockedForRoute
			&& OutState.CardRun.bHasActiveCardBattle
			&& GetRewardSourceNodeId(OutState) >= 0
			&& OutState.CardRun.RouteProgress.RootSeed == RootSeed
			&& OutState.CardRun.RouteCardEntries.Num() == FGameXXKRouteCardRecipe::BaseEntryCount
			&& OutState.CardRun.RouteCardIds.IsEmpty();
	}

	bool SetPendingBossReward(FGameXXKRuntimeState& State, FName& OutBossCardId, const int32 ChoiceSeed = 0x5A17)
	{
		OutBossCardId = NAME_None;
		FString Error;
		if (!FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(
			State,
			EGameXXKNodeKind::Boss,
			GetRewardSourceNodeId(State),
			ChoiceSeed,
			&Error))
		{
			return false;
		}
		for (const FGameXXKBattleRewardOption& Option : State.CardRun.PendingReward.Options)
		{
			if (Option.Kind == EGameXXKBattleRewardKind::BossCard)
			{
				OutBossCardId = Option.CardId;
			}
		}
		State.CardRun.bActiveBattleRewardResolved = false;
		return !OutBossCardId.IsNone();
	}

	bool AddRouteEntry(
		FGameXXKRuntimeState& State,
		const FName CardId,
		const EGameXXKCardQuality Quality,
		const bool bConsumesRouteCapacity = true,
		const EGameXXKRouteCardSourceKind SourceKind = EGameXXKRouteCardSourceKind::RouteReward)
	{
		FGameXXKRouteCardEntry Entry;
		Entry.CardId = CardId;
		Entry.CurrentQuality = Quality;
		Entry.SourceKind = SourceKind;
		Entry.OwnerUnitId = PlayerUnitId;
		Entry.bTemporaryRouteCard = SourceKind != EGameXXKRouteCardSourceKind::RouteBase;
		Entry.bConsumesRouteCapacity = bConsumesRouteCapacity;
		Entry.AcquisitionOrdinal = State.CardRun.NextRouteCardEntryOrdinal;
		if (!FGameXXKRouteCardRecipe::MakeStableEntryId(
			State.CardRun.RouteProgress.RootSeed,
			Entry.AcquisitionOrdinal,
			Entry.EntryId))
		{
			return false;
		}
		State.CardRun.RouteCardEntries.Add(MoveTemp(Entry));
		++State.CardRun.NextRouteCardEntryOrdinal;
		return true;
	}

	int32 CountCapacityEntries(const FGameXXKRuntimeState& State)
	{
		int32 Count = 0;
		for (const FGameXXKRouteCardEntry& Entry : State.CardRun.RouteCardEntries)
		{
			Count += Entry.bConsumesRouteCapacity ? 1 : 0;
		}
		return Count;
	}

	int32 CountEntriesByCardId(const FGameXXKRuntimeState& State, const FName CardId)
	{
		int32 Count = 0;
		for (const FGameXXKRouteCardEntry& Entry : State.CardRun.RouteCardEntries)
		{
			Count += Entry.CardId == CardId ? 1 : 0;
		}
		return Count;
	}

	const FGameXXKRouteCardEntry* FindEntry(const FGameXXKRuntimeState& State, const FName EntryId)
	{
		return State.CardRun.RouteCardEntries.FindByPredicate([EntryId](const FGameXXKRouteCardEntry& Entry)
		{
			return Entry.EntryId == EntryId;
		});
	}

	bool EntriesEqual(
		const TArray<FGameXXKRouteCardEntry>& Left,
		const TArray<FGameXXKRouteCardEntry>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!FGameXXKRouteCardEntry::StaticStruct()->CompareScriptStruct(
				&Left[Index],
				&Right[Index],
				PPF_None))
			{
				return false;
			}
		}
		return true;
	}

	bool ActiveBattlesEqual(
		const FGameXXKCardBattleRuntime& Left,
		const FGameXXKCardBattleRuntime& Right)
	{
		return FGameXXKCardBattleRuntime::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

	bool EnemyIntentsEqual(
		const TArray<FGameXXKCardEnemyIntent>& Left,
		const TArray<FGameXXKCardEnemyIntent>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!FGameXXKCardEnemyIntent::StaticStruct()->CompareScriptStruct(&Left[Index], &Right[Index], PPF_None))
			{
				return false;
			}
		}
		return true;
	}

	bool FillCapacity(
		FGameXXKRuntimeState& State,
		const TSet<FName>& ExcludedCardIds = {})
	{
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (CountCapacityEntries(State) >= FGameXXKRunDeckRules::MaxRouteCardCapacity)
			{
				return true;
			}
			// Boss-pool cards stay out of capacity fixtures so the generated boss-card
			// option never merges or loses pool eligibility.
			if (Definition.Owner == EGameXXKCardOwner::Route
				&& !ExcludedCardIds.Contains(Definition.Id)
				&& !Definition.AcquisitionKey.ToString().StartsWith(TEXT("Route.Boss.")))
			{
				if (!AddRouteEntry(State, Definition.Id, EGameXXKCardQuality::Epic))
				{
					return false;
				}
			}
		}
		return CountCapacityEntries(State) == FGameXXKRunDeckRules::MaxRouteCardCapacity;
	}

	bool PreviewsEqual(
		const FGameXXKRouteCardAcquisitionPreview& Left,
		const FGameXXKRouteCardAcquisitionPreview& Right)
	{
		return Left.Decision == Right.Decision
			&& Left.Merge.bWillMerge == Right.Merge.bWillMerge
			&& Left.Merge.SurvivorEntryId == Right.Merge.SurvivorEntryId
			&& Left.Merge.ConsumedEntryIds == Right.Merge.ConsumedEntryIds
			&& Left.Merge.FinalQuality == Right.Merge.FinalQuality
			&& Left.Merge.TemporaryCountDelta == Right.Merge.TemporaryCountDelta
			&& Left.Merge.CapacityDelta == Right.Merge.CapacityDelta
			&& Left.CapacityBefore == Right.CapacityBefore
			&& Left.CapacityAfter == Right.CapacityAfter
			&& Left.ReplacementEntryId == Right.ReplacementEntryId
			&& Left.EligibleReplacementEntryIds == Right.EligibleReplacementEntryIds;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardEntryPreviewAndDirectCommitTest,
	"GameXXK.Integration.CardRoute.RewardEntryAcquisition.PreviewAndDirectCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardEntryPreviewAndDirectCommitTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteRewardEntryAcquisitionTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("preview fixture enters a canonical reward-ready boss route"), StartRewardReadyState(State, EGameXXKNodeKind::Boss)))
	{
		return false;
	}
	FName BossCardId;
	if (!TestTrue(TEXT("the boss tiered offer names its boss-card option"), SetPendingBossReward(State, BossCardId)))
	{
		return false;
	}
	State.CardRun.RouteRandomSeed = 0x13572468;
	TestTrue(TEXT("the route RNG sentinel differs from stable identity authority"),
		State.CardRun.RouteRandomSeed != State.CardRun.RouteProgress.RootSeed);

	const FString BeforePreview = ExportState(State);
	const int32 NextEntryBefore = State.CardRun.NextRouteCardEntryOrdinal;
	const int32 AcquisitionCountBefore = State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount;
	FName ExpectedEntryId;
	FString Error;
	TestTrue(TEXT("the expected candidate stable ID resolves from root seed and the dedicated ordinal"),
		FGameXXKRouteCardRecipe::MakeStableEntryId(
			State.CardRun.RouteProgress.RootSeed,
			NextEntryBefore,
			ExpectedEntryId,
			&Error));
	TestEqual(TEXT("the stable candidate ID literally derives from RootSeed and ordinal, not RouteRandomSeed"),
		ExpectedEntryId,
		FName(TEXT("RouteEntry.00002468.00000012")));

	FGameXXKRouteCardAcquisitionPreview FirstPreview;
	FGameXXKRouteCardAcquisitionPreview SecondPreview;
	TestTrue(FString::Printf(TEXT("the first pending boss-reward preview succeeds: %s"), *Error),
		FGameXXKCardBattleAdapter::PreviewPendingRouteReward(
			State,
			BossCardId,
			NAME_None,
			FirstPreview,
			&Error));
	TestEqual(TEXT("the first preview is byte-pure over the complete runtime"), ExportState(State), BeforePreview);
	TestTrue(TEXT("the repeated pending boss-reward preview succeeds"),
		FGameXXKCardBattleAdapter::PreviewPendingRouteReward(
			State,
			BossCardId,
			NAME_None,
			SecondPreview,
			&Error));
	TestEqual(TEXT("the repeated preview remains byte-pure over the complete runtime"), ExportState(State), BeforePreview);
	TestTrue(TEXT("repeated previews are deterministic"), PreviewsEqual(FirstPreview, SecondPreview));
	TestEqual(TEXT("a non-full acquisition can commit without replacement"), FirstPreview.Decision, EGameXXKRouteCardAcquisitionDecision::CanCommit);
	TestFalse(TEXT("the distinct direct boss candidate does not merge"), FirstPreview.Merge.bWillMerge);
	TestEqual(TEXT("the preview exposes the exact stable candidate ID"), FirstPreview.Merge.SurvivorEntryId, ExpectedEntryId);
	const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(BossCardId);
	TestNotNull(TEXT("the direct boss reward candidate is catalog-backed"), Definition);
	if (Definition)
	{
		TestEqual(TEXT("the preview uses catalog BaseQuality"), FirstPreview.Merge.FinalQuality, Definition->BaseQuality);
	}
	TestEqual(TEXT("preview sees no acquired capacity before commit"), FirstPreview.CapacityBefore, 0);
	TestEqual(TEXT("preview sees one acquired slot after direct commit"), FirstPreview.CapacityAfter, 1);
	TestEqual(TEXT("preview never advances the entry ordinal"), State.CardRun.NextRouteCardEntryOrdinal, NextEntryBefore);
	TestEqual(TEXT("preview never advances acquisition history"), State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, AcquisitionCountBefore);

	const FGameXXKCardBattleRuntime ActiveBattleBeforeCommit = State.CardRun.ActiveBattle;
	const TArray<FGameXXKCardEnemyIntent> EnemyIntentsBeforeCommit = State.CardRun.EnemyIntents;
	const int32 IntentCursorBeforeCommit = State.CardRun.NextEnemyIntentIndex;
	TestTrue(FString::Printf(TEXT("the boss reward commits through the shared acquisition rules: %s"), *Error),
		FGameXXKCardBattleAdapter::CommitBossCardReward(State, BossCardId, NAME_None, &Error));
	TestTrue(TEXT("direct boss commit preserves the complete active battle snapshot"),
		ActiveBattlesEqual(State.CardRun.ActiveBattle, ActiveBattleBeforeCommit));
	TestTrue(TEXT("direct boss commit preserves all saved enemy intents"),
		EnemyIntentsEqual(State.CardRun.EnemyIntents, EnemyIntentsBeforeCommit));
	TestEqual(TEXT("direct boss commit preserves the enemy-intent cursor"),
		State.CardRun.NextEnemyIntentIndex,
		IntentCursorBeforeCommit);
	TestEqual(TEXT("a direct commit advances the dedicated entry ordinal exactly once"), State.CardRun.NextRouteCardEntryOrdinal, NextEntryBefore + 1);
	TestEqual(TEXT("CommitAcquisition advances acquisition history exactly once"),
		State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount,
		AcquisitionCountBefore + 1);
	const FGameXXKRouteCardEntry* AddedEntry = FindEntry(State, ExpectedEntryId);
	TestNotNull(TEXT("the committed direct boss reward retains the previewed stable entry"), AddedEntry);
	if (AddedEntry && Definition)
	{
		TestEqual(TEXT("the committed entry retains the selected CardId"), AddedEntry->CardId, BossCardId);
		TestEqual(TEXT("the committed entry starts at catalog BaseQuality"), AddedEntry->CurrentQuality, Definition->BaseQuality);
		TestEqual(TEXT("the committed entry records route-reward provenance"), AddedEntry->SourceKind, EGameXXKRouteCardSourceKind::RouteReward);
		TestEqual(TEXT("the committed entry belongs to the player"), AddedEntry->OwnerUnitId, PlayerUnitId);
		TestTrue(TEXT("the committed entry is route-temporary"), AddedEntry->bTemporaryRouteCard);
		TestTrue(TEXT("the committed entry consumes one route capacity slot"), AddedEntry->bConsumesRouteCapacity);
		TestEqual(TEXT("the committed entry retains the dedicated acquisition ordinal"), AddedEntry->AcquisitionOrdinal, NextEntryBefore);
	}
	TestEqual(TEXT("the adapter-level commit leaves the saved tiered offer until the rules finish it"), State.CardRun.PendingReward.Options.Num(), 3);
	TestTrue(TEXT("the entry path never appends to legacy RouteCardIds"), State.CardRun.RouteCardIds.IsEmpty());
	TestTrue(TEXT("skipping the already-committed offer finishes the boss victory"), UGameXXKMVPRules::SkipPendingRouteRewardAndFinish(State, &Error));
	TestEqual(TEXT("the cleared fixed-dungeon boss advances to the next chapter map"), State.Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("the boss clear advances the route to chapter two"), State.CardRun.RouteProgress.CurrentChapter, 2);
	TestTrue(TEXT("the finished victory clears the saved tiered offer"), State.CardRun.PendingReward.Options.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardEntryEpicNeverMergesTest,
	"GameXXK.Integration.CardRoute.RewardEntryAcquisition.EpicCopyNeverMerges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardEntryEpicNeverMergesTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteRewardEntryAcquisitionTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("epic-copy fixture enters a canonical reward-ready boss route"), StartRewardReadyState(State, EGameXXKNodeKind::Boss)))
	{
		return false;
	}
	FName BossCardId;
	if (!TestTrue(TEXT("epic-copy fixture resolves the boss-card option"), SetPendingBossReward(State, BossCardId))
		|| !TestTrue(TEXT("epic-copy fixture plants one Epic boss-card copy"), AddRouteEntry(State, BossCardId, EGameXXKCardQuality::Epic))
		|| !TestTrue(TEXT("epic-copy fixture fills exactly twelve capacity entries"), FillCapacity(State, {BossCardId})))
	{
		return false;
	}
	State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 7;
	const int32 NextEntryBefore = State.CardRun.NextRouteCardEntryOrdinal;
	FName CandidateEntryId;
	FString Error;
	TestTrue(TEXT("epic-copy fixture resolves the candidate stable ID"),
		FGameXXKRouteCardRecipe::MakeStableEntryId(
			State.CardRun.RouteProgress.RootSeed,
			NextEntryBefore,
			CandidateEntryId,
			&Error));
	const FGameXXKRouteCardEntry* ExistingEpic = State.CardRun.RouteCardEntries.FindByPredicate([BossCardId](const FGameXXKRouteCardEntry& Entry)
	{
		return Entry.CardId == BossCardId && Entry.CurrentQuality == EGameXXKCardQuality::Epic;
	});
	TestNotNull(TEXT("the fixture contains the pre-owned Epic boss copy"), ExistingEpic);
	const FName ExistingEpicEntryId = ExistingEpic ? ExistingEpic->EntryId : NAME_None;

	FGameXXKRouteCardAcquisitionPreview Preview;
	TestTrue(FString::Printf(TEXT("the Epic boss reward previews at full capacity: %s"), *Error),
		FGameXXKCardBattleAdapter::PreviewPendingRouteReward(State, BossCardId, NAME_None, Preview, &Error));
	TestEqual(TEXT("an Epic pre-owned boss copy never merges with the offered candidate"), Preview.Decision, EGameXXKRouteCardAcquisitionDecision::RequiresReplacement);
	TestFalse(TEXT("Epic copies are excluded from merge resolution"), Preview.Merge.bWillMerge);
	TestEqual(TEXT("the preview keeps the candidate as its own survivor lineage"), Preview.Merge.SurvivorEntryId, CandidateEntryId);
	TestEqual(TEXT("the pre-owned Epic copy is itself an eligible replacement"),
		Preview.EligibleReplacementEntryIds.Contains(ExistingEpicEntryId), true);
	TestEqual(TEXT("the preview begins at twelve acquired slots"), Preview.CapacityBefore, FGameXXKRunDeckRules::MaxRouteCardCapacity);

	FGameXXKRuntimeState MissingReplacementState = State;
	const FString BeforeMissingReplacement = ExportState(MissingReplacementState);
	TestFalse(TEXT("the Epic pre-owned boss candidate rejects a missing replacement"),
		FGameXXKCardBattleAdapter::CommitBossCardReward(MissingReplacementState, BossCardId, NAME_None, &Error));
	TestEqual(TEXT("missing replacement rejection rolls back the complete runtime"),
		ExportState(MissingReplacementState),
		BeforeMissingReplacement);

	TestTrue(FString::Printf(TEXT("the pre-owned Epic copy commits as the replacement for its own CardId: %s"), *Error),
		FGameXXKCardBattleAdapter::CommitBossCardReward(State, BossCardId, ExistingEpicEntryId, &Error));
	TestNull(TEXT("the replaced pre-owned Epic copy is removed"), FindEntry(State, ExistingEpicEntryId));
	TestNotNull(TEXT("the new boss candidate is committed under its stable ID"), FindEntry(State, CandidateEntryId));
	TestEqual(TEXT("Epic replacement preserves the twelve-entry capacity"), CountCapacityEntries(State), FGameXXKRunDeckRules::MaxRouteCardCapacity);
	TestEqual(TEXT("Epic replacement advances the dedicated ordinal exactly once"), State.CardRun.NextRouteCardEntryOrdinal, NextEntryBefore + 1);
	TestEqual(TEXT("Epic replacement advances acquisition history exactly once"), State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardEntryStableReplacementTest,
	"GameXXK.Integration.CardRoute.RewardEntryAcquisition.StableReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardEntryStableReplacementTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteRewardEntryAcquisitionTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("replacement fixture enters a canonical reward-ready boss route"), StartRewardReadyState(State, EGameXXKNodeKind::Boss))
		|| !TestTrue(TEXT("first duplicate CardId entry is added"), AddRouteEntry(State, DuplicateCardId, EGameXXKCardQuality::Epic))
		|| !TestTrue(TEXT("second duplicate CardId entry is added"), AddRouteEntry(State, DuplicateCardId, EGameXXKCardQuality::Epic))
		|| !TestTrue(TEXT("replacement fixture fills exactly twelve capacity entries"), FillCapacity(State, {DuplicateCardId})))
	{
		return false;
	}
	FName BossCardId;
	if (!TestTrue(TEXT("replacement fixture resolves the boss-card option"), SetPendingBossReward(State, BossCardId)))
	{
		return false;
	}
	State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 4;
	TArray<const FGameXXKRouteCardEntry*> DuplicateEntries;
	for (const FGameXXKRouteCardEntry& Entry : State.CardRun.RouteCardEntries)
	{
		if (Entry.CardId == DuplicateCardId && Entry.bConsumesRouteCapacity)
		{
			DuplicateEntries.Add(&Entry);
		}
	}
	if (!TestEqual(TEXT("duplicate CardId fixture retains two stable capacity entries"), DuplicateEntries.Num(), 2))
	{
		return false;
	}
	const FName RemovedEntryId = DuplicateEntries[0]->EntryId;
	const FName PreservedEntryId = DuplicateEntries[1]->EntryId;
	const int32 NextEntryBefore = State.CardRun.NextRouteCardEntryOrdinal;
	FName CandidateEntryId;
	FString Error;
	TestTrue(TEXT("replacement fixture resolves the candidate stable ID"),
		FGameXXKRouteCardRecipe::MakeStableEntryId(
			State.CardRun.RouteProgress.RootSeed,
			NextEntryBefore,
			CandidateEntryId,
			&Error));

	FGameXXKRouteCardAcquisitionPreview Preview;
	TestTrue(TEXT("full non-merge boss candidate previews successfully without a replacement selection"),
		FGameXXKCardBattleAdapter::PreviewPendingRouteReward(State, BossCardId, NAME_None, Preview, &Error));
	TestEqual(TEXT("full non-merge boss candidate requires replacement"), Preview.Decision, EGameXXKRouteCardAcquisitionDecision::RequiresReplacement);
	TestTrue(TEXT("both duplicate CardId entries are independently eligible"),
		Preview.EligibleReplacementEntryIds.Contains(RemovedEntryId)
			&& Preview.EligibleReplacementEntryIds.Contains(PreservedEntryId));

	FGameXXKRuntimeState MissingReplacementState = State;
	const FString BeforeMissingReplacement = ExportState(MissingReplacementState);
	TestFalse(TEXT("full non-merge boss choice rejects a missing replacement"),
		FGameXXKCardBattleAdapter::CommitBossCardReward(MissingReplacementState, BossCardId, NAME_None, &Error));
	TestEqual(TEXT("missing replacement rejection rolls back the complete runtime"),
		ExportState(MissingReplacementState),
		BeforeMissingReplacement);

	FGameXXKRuntimeState CardIdReplacementState = State;
	const FString BeforeCardIdReplacement = ExportState(CardIdReplacementState);
	TestFalse(TEXT("CardId cannot masquerade as a stable replacement EntryId"),
		FGameXXKCardBattleAdapter::CommitBossCardReward(
			CardIdReplacementState,
			BossCardId,
			DuplicateCardId,
			&Error));
	TestEqual(TEXT("CardId-as-EntryId rejection rolls back the complete runtime"),
		ExportState(CardIdReplacementState),
		BeforeCardIdReplacement);

	TestTrue(FString::Printf(TEXT("an exact eligible EntryId commits the full non-merge boss reward: %s"), *Error),
		FGameXXKCardBattleAdapter::CommitBossCardReward(State, BossCardId, RemovedEntryId, &Error));
	TestNull(TEXT("only the selected duplicate entry is removed"), FindEntry(State, RemovedEntryId));
	TestNotNull(TEXT("the other duplicate CardId entry remains distinguishable"), FindEntry(State, PreservedEntryId));
	TestNotNull(TEXT("the replacement boss candidate is committed under its stable ID"), FindEntry(State, CandidateEntryId));
	TestEqual(TEXT("replacement preserves the twelve-entry capacity"), CountCapacityEntries(State), FGameXXKRunDeckRules::MaxRouteCardCapacity);
	TestEqual(TEXT("replacement advances the dedicated ordinal exactly once"), State.CardRun.NextRouteCardEntryOrdinal, NextEntryBefore + 1);
	TestEqual(TEXT("replacement advances acquisition history exactly once"), State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardEntryRollbackTest,
	"GameXXK.Integration.CardRoute.RewardEntryAcquisition.RollbackAndGateValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardEntryRollbackTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteRewardEntryAcquisitionTest;
	FGameXXKRuntimeState BaseState;
	if (!TestTrue(TEXT("rollback fixture enters a canonical reward-ready boss route"), StartRewardReadyState(BaseState, EGameXXKNodeKind::Boss)))
	{
		return false;
	}
	FName BossCardId;
	if (!TestTrue(TEXT("rollback fixture resolves the boss-card option"), SetPendingBossReward(BaseState, BossCardId)))
	{
		return false;
	}
	FString Error;
	const auto ExpectAtomicFailure = [this, &Error](
		const TCHAR* FailureLabel,
		FGameXXKRuntimeState State,
		const FName RewardCardId,
		const FName ReplacementEntryId)
	{
		const FString Before = ExportState(State);
		Error.Reset();
		const bool bSucceeded = FGameXXKCardBattleAdapter::CommitBossCardReward(
			State,
			RewardCardId,
			ReplacementEntryId,
			&Error);
		TestFalse(FailureLabel, bSucceeded);
		TestTrue(FString::Printf(TEXT("%s reports a concrete error"), FailureLabel), !Error.IsEmpty());
		TestEqual(FString::Printf(TEXT("%s leaves the complete runtime byte-identical"), FailureLabel), ExportState(State), Before);
	};

	const FName ExistingEntryId = BaseState.CardRun.RouteCardEntries[0].EntryId;
	ExpectAtomicFailure(TEXT("unnecessary replacement"), BaseState, BossCardId, ExistingEntryId);
	ExpectAtomicFailure(TEXT("unknown boss reward candidate"), BaseState, TEXT("Route.Unknown.Reward"), NAME_None);

	FGameXXKRuntimeState MixedAuthority = BaseState;
	MixedAuthority.CardRun.RouteCardIds.Add(TEXT("Route.General.PoJiaTuCi"));
	ExpectAtomicFailure(TEXT("mixed entry and legacy authority"), MixedAuthority, BossCardId, NAME_None);

	FGameXXKRuntimeState NegativeOrdinal = BaseState;
	NegativeOrdinal.CardRun.NextRouteCardEntryOrdinal = -1;
	ExpectAtomicFailure(TEXT("negative entry ordinal"), NegativeOrdinal, BossCardId, NAME_None);

	FGameXXKRuntimeState ExhaustedOrdinal = BaseState;
	ExhaustedOrdinal.CardRun.NextRouteCardEntryOrdinal = MAX_int32;
	ExpectAtomicFailure(TEXT("exhausted entry ordinal"), ExhaustedOrdinal, BossCardId, NAME_None);

	FGameXXKRuntimeState CommitFailure = BaseState;
	CommitFailure.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = MAX_int32;
	ExpectAtomicFailure(TEXT("shared acquisition commit rejection"), CommitFailure, BossCardId, NAME_None);

	FGameXXKRuntimeState InvalidRootSeed = BaseState;
	InvalidRootSeed.CardRun.RouteProgress.RootSeed = 0;
	ExpectAtomicFailure(TEXT("zero route root seed"), InvalidRootSeed, BossCardId, NAME_None);

	FGameXXKRuntimeState InactiveRoute = BaseState;
	InactiveRoute.bDungeonActive = false;
	ExpectAtomicFailure(TEXT("inactive route"), InactiveRoute, BossCardId, NAME_None);

	FGameXXKRuntimeState LegacyOnly = BaseState;
	LegacyOnly.CardRun.RouteCardEntries.Reset();
	LegacyOnly.CardRun.RouteCardIds = {TEXT("Route.General.PoJiaTuCi")};
	ExpectAtomicFailure(TEXT("legacy-only route authority"), LegacyOnly, BossCardId, NAME_None);

	FGameXXKRuntimeState InvalidGate = BaseState;
	InvalidGate.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Player;
	ExpectAtomicFailure(TEXT("non-victory reward gate"), InvalidGate, BossCardId, NAME_None);

	FGameXXKRuntimeState ResolvedGate = BaseState;
	ResolvedGate.CardRun.bActiveBattleRewardResolved = true;
	ExpectAtomicFailure(TEXT("already-resolved reward gate"), ResolvedGate, BossCardId, NAME_None);

	FGameXXKRuntimeState InvalidPending = BaseState;
	InvalidPending.CardRun.PendingReward.Options.SetNum(2);
	ExpectAtomicFailure(TEXT("incomplete pending reward metadata"), InvalidPending, BossCardId, NAME_None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardEntryOfferTest,
	"GameXXK.Integration.CardRoute.RewardEntryAcquisition.TransactionalOffer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardEntryOfferTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteRewardEntryAcquisitionTest;
	constexpr int32 RootSeed = 0x3579;
	constexpr int32 ChoiceSeed = 0x61A3;
	FGameXXKRuntimeState ProbeState;
	if (!TestTrue(TEXT("offer probe enters a canonical reward-ready battle route"), StartRewardReadyState(ProbeState, EGameXXKNodeKind::Battle, RootSeed)))
	{
		return false;
	}
	const int32 SourceNodeId = GetRewardSourceNodeId(ProbeState);
	FGameXXKRuntimeState EmptyFlagTrueState = ProbeState;
	EmptyFlagTrueState.CardRun.PendingReward.bRequiresRouteCardReplacement = true;
	FString Error;
	TestTrue(FString::Printf(TEXT("baseline tiered offer resolves three deterministic options: %s"), *Error),
		FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(
			ProbeState,
			EGameXXKNodeKind::Battle,
			SourceNodeId,
			ChoiceSeed,
			&Error));
	TestTrue(TEXT("an empty pending legacy flag does not block tiered offer creation"),
		FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(
			EmptyFlagTrueState,
			EGameXXKNodeKind::Battle,
			SourceNodeId,
			ChoiceSeed,
			&Error));
	TestEqual(TEXT("empty pending legacy flag does not change the committed runtime"),
		ExportState(EmptyFlagTrueState),
		ExportState(ProbeState));
	if (!TestEqual(TEXT("baseline tiered offer contains exactly three options"), ProbeState.CardRun.PendingReward.Options.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("the normal battle offer opens with a relic"), ProbeState.CardRun.PendingReward.Options[0].Kind, EGameXXKBattleRewardKind::Relic);
	TestEqual(TEXT("the normal battle offer follows with a second relic"), ProbeState.CardRun.PendingReward.Options[1].Kind, EGameXXKBattleRewardKind::Relic);
	TestEqual(TEXT("the normal battle offer ends with a deck-card upgrade"), ProbeState.CardRun.PendingReward.Options[2].Kind, EGameXXKBattleRewardKind::DeckCardUpgrade);

	FGameXXKRuntimeState FullState;
	if (!TestTrue(TEXT("full offer fixture enters the same canonical battle route"), StartRewardReadyState(FullState, EGameXXKNodeKind::Battle, RootSeed))
		|| !TestTrue(TEXT("full offer fixture fills exactly twelve capacity slots"), FillCapacity(FullState)))
	{
		return false;
	}
	FullState.CardRun.NextRewardOrdinal = 11;
	const int32 NextRewardBefore = FullState.CardRun.NextRewardOrdinal;
	const int32 NextEntryBefore = FullState.CardRun.NextRouteCardEntryOrdinal;
	TestTrue(FString::Printf(TEXT("a full-capacity deck still receives a tiered three-choice offer: %s"), *Error),
		FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(
			FullState,
			EGameXXKNodeKind::Battle,
			GetRewardSourceNodeId(FullState),
			ChoiceSeed,
			&Error));
	TestEqual(TEXT("the first new pending offer advances NextRewardOrdinal exactly once"),
		FullState.CardRun.NextRewardOrdinal,
		NextRewardBefore + 1);
	TestEqual(TEXT("creating an offer never advances the stable entry ordinal"),
		FullState.CardRun.NextRouteCardEntryOrdinal,
		NextEntryBefore);
	TestFalse(TEXT("new offers persist the legacy replacement flag as false"),
		FullState.CardRun.PendingReward.bRequiresRouteCardReplacement);

	const FString BeforeRepeatedOffer = ExportState(FullState);
	TestTrue(TEXT("repeating the same pending offer succeeds"),
		FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(
			FullState,
			EGameXXKNodeKind::Battle,
			GetRewardSourceNodeId(FullState),
			ChoiceSeed,
			&Error));
	TestEqual(TEXT("repeated pending offer does not mutate any runtime byte"), ExportState(FullState), BeforeRepeatedOffer);
	TestEqual(TEXT("repeated pending offer does not advance NextRewardOrdinal"), FullState.CardRun.NextRewardOrdinal, NextRewardBefore + 1);
	TestEqual(TEXT("repeated pending offer does not advance the stable entry ordinal"), FullState.CardRun.NextRouteCardEntryOrdinal, NextEntryBefore);

	FullState.CardRun.NextRewardOrdinal = MAX_int32;
	const FString BeforeRepeatedAtMax = ExportState(FullState);
	TestTrue(TEXT("an already-pending offer remains readable at the reward-ordinal limit"),
		FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(
			FullState,
			EGameXXKNodeKind::Battle,
			GetRewardSourceNodeId(FullState),
			ChoiceSeed,
			&Error));
	TestEqual(TEXT("pending-at-limit remains byte-pure"), ExportState(FullState), BeforeRepeatedAtMax);

	const auto ExpectOfferFailure = [this, &Error](
		const TCHAR* FailureLabel,
		FGameXXKRuntimeState State,
		const EGameXXKNodeKind NodeKind,
		const int32 SourceNode,
		const int32 Seed)
	{
		const FString Before = ExportState(State);
		Error.Reset();
		const bool bSucceeded = FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(
			State,
			NodeKind,
			SourceNode,
			Seed,
			&Error);
		TestFalse(FailureLabel, bSucceeded);
		TestTrue(FString::Printf(TEXT("%s reports a concrete error"), FailureLabel), !Error.IsEmpty());
		TestEqual(FString::Printf(TEXT("%s leaves the complete runtime byte-identical"), FailureLabel), ExportState(State), Before);
	};

	FGameXXKRuntimeState FreshFailureBase;
	if (!TestTrue(TEXT("offer rollback fixture enters a canonical battle route"), StartRewardReadyState(FreshFailureBase, EGameXXKNodeKind::Battle, RootSeed)))
	{
		return false;
	}
	FGameXXKRuntimeState EntryOrdinalExhaustedOffer = FreshFailureBase;
	EntryOrdinalExhaustedOffer.CardRun.NextRouteCardEntryOrdinal = MAX_int32;
	EntryOrdinalExhaustedOffer.CardRun.NextRewardOrdinal = 3;
	TestTrue(TEXT("offer creation does not require an incrementable entry ordinal"),
		FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(
			EntryOrdinalExhaustedOffer,
			EGameXXKNodeKind::Battle,
			GetRewardSourceNodeId(EntryOrdinalExhaustedOffer),
			ChoiceSeed,
			&Error));
	TestEqual(TEXT("entry-ordinal exhaustion still produces three options"), EntryOrdinalExhaustedOffer.CardRun.PendingReward.Options.Num(), 3);
	TestEqual(TEXT("offer creation preserves an exhausted entry ordinal"),
		EntryOrdinalExhaustedOffer.CardRun.NextRouteCardEntryOrdinal,
		MAX_int32);
	TestEqual(TEXT("entry-ordinal exhaustion advances only the reward ordinal"),
		EntryOrdinalExhaustedOffer.CardRun.NextRewardOrdinal,
		4);
	ExpectOfferFailure(TEXT("invalid reward source"), FreshFailureBase, EGameXXKNodeKind::Battle, INDEX_NONE, ChoiceSeed);
	ExpectOfferFailure(TEXT("zero reward seed"), FreshFailureBase, EGameXXKNodeKind::Battle, GetRewardSourceNodeId(FreshFailureBase), 0);
	ExpectOfferFailure(TEXT("invalid reward node kind"), FreshFailureBase, EGameXXKNodeKind::Event, GetRewardSourceNodeId(FreshFailureBase), ChoiceSeed);

	FGameXXKRuntimeState ExhaustedRewardOrdinal = FreshFailureBase;
	ExhaustedRewardOrdinal.CardRun.NextRewardOrdinal = MAX_int32;
	ExpectOfferFailure(TEXT("new offer reward-ordinal overflow"), ExhaustedRewardOrdinal, EGameXXKNodeKind::Battle, GetRewardSourceNodeId(ExhaustedRewardOrdinal), ChoiceSeed);

	FGameXXKRuntimeState MixedAuthority = FreshFailureBase;
	MixedAuthority.CardRun.RouteCardIds.Add(TEXT("Route.General.PoJiaTuCi"));
	ExpectOfferFailure(TEXT("offer mixed authority"), MixedAuthority, EGameXXKNodeKind::Battle, GetRewardSourceNodeId(MixedAuthority), ChoiceSeed);

	FGameXXKRuntimeState OverCapacity = FreshFailureBase;
	if (!TestTrue(TEXT("over-capacity fixture fills twelve slots"), FillCapacity(OverCapacity))
		|| !TestTrue(TEXT("over-capacity fixture adds the corrupt thirteenth slot"),
			AddRouteEntry(OverCapacity, TEXT("Route.Boss.HuPoZhenDan"), EGameXXKCardQuality::Epic)))
	{
		return false;
	}
	ExpectOfferFailure(TEXT("offer corrupt capacity above twelve"), OverCapacity, EGameXXKNodeKind::Battle, GetRewardSourceNodeId(OverCapacity), ChoiceSeed);

	FGameXXKRuntimeState InvalidGate = FreshFailureBase;
	InvalidGate.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Player;
	ExpectOfferFailure(TEXT("offer outside the victory gate"), InvalidGate, EGameXXKNodeKind::Battle, GetRewardSourceNodeId(InvalidGate), ChoiceSeed);

	FGameXXKRuntimeState InitializationLeak = FreshFailureBase;
	InitializationLeak.CardRun.ActiveTemporaryQuestNpcId = TEXT("Npc.TusiChief");
	InitializationLeak.CardRun.PartySelection.QuestNpc.NpcId = TEXT("Npc.YueBai");
	ExpectOfferFailure(TEXT("mismatched task-NPC provenance fails card-run initialization"), InitializationLeak, EGameXXKNodeKind::Battle, GetRewardSourceNodeId(InitializationLeak), ChoiceSeed);

	FGameXXKRuntimeState RecomputedSeedPending = ProbeState;
	const FString BeforeRecomputedSeed = ExportState(RecomputedSeedPending);
	TestTrue(TEXT("an already-saved pending offer ignores a newly recomputed choice seed"),
		FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(
			RecomputedSeedPending,
			EGameXXKNodeKind::Battle,
			GetRewardSourceNodeId(RecomputedSeedPending),
			ChoiceSeed + 1,
			&Error));
	TestEqual(TEXT("reading a saved offer with a recomputed seed is byte-pure"), ExportState(RecomputedSeedPending), BeforeRecomputedSeed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardResolveVictoryIdempotencyTest,
	"GameXXK.Integration.CardRoute.RewardEntryAcquisition.ResolveVictoryIdempotency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardResolveVictoryIdempotencyTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteRewardEntryAcquisitionTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("double-resolve fixture enters a canonical reward-ready battle route"), StartRewardReadyState(State, EGameXXKNodeKind::Battle)))
	{
		return false;
	}

	TestTrue(TEXT("the first victory resolution creates the saved tiered offer"),
		UGameXXKMVPRules::ResolveBattleVictory(State, false));
	TestEqual(TEXT("the first victory resolution persists exactly three options"), State.CardRun.PendingReward.Options.Num(), 3);
	const FString BeforeSecondResolve = ExportState(State);
	const FGameXXKPendingRouteCardReward PendingBeforeSecondResolve = State.CardRun.PendingReward;
	const int32 RewardOrdinalBeforeSecondResolve = State.CardRun.NextRewardOrdinal;

	TestTrue(TEXT("repeating victory resolution while the reward is pending remains successful"),
		UGameXXKMVPRules::ResolveBattleVictory(State, false));
	TestTrue(TEXT("the repeated victory resolution returns the same saved offer"),
		FGameXXKPendingRouteCardReward::StaticStruct()->CompareScriptStruct(
			&State.CardRun.PendingReward,
			&PendingBeforeSecondResolve,
			PPF_None));
	TestEqual(TEXT("the repeated victory resolution does not advance the reward ordinal"),
		State.CardRun.NextRewardOrdinal,
		RewardOrdinalBeforeSecondResolve);
	TestEqual(TEXT("the repeated victory resolution is byte-pure over the complete runtime"),
		ExportState(State),
		BeforeSecondResolve);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardEntrySkipTest,
	"GameXXK.Integration.CardRoute.RewardEntryAcquisition.Skip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardEntrySkipTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteRewardEntryAcquisitionTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("skip fixture enters a canonical reward-ready boss route"), StartRewardReadyState(State, EGameXXKNodeKind::Boss)))
	{
		return false;
	}
	FName BossCardId;
	if (!TestTrue(TEXT("skip fixture resolves the boss-card option"), SetPendingBossReward(State, BossCardId)))
	{
		return false;
	}
	State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 6;
	State.CardRun.NextRouteCardEntryOrdinal = MAX_int32;
	State.CardRun.PendingReward.bRequiresRouteCardReplacement = true;
	const int32 NextEntryBefore = State.CardRun.NextRouteCardEntryOrdinal;
	const TArray<FGameXXKRouteCardEntry> EntriesBefore = State.CardRun.RouteCardEntries;
	const FGameXXKCardBattleRuntime ActiveBattleBeforeSkip = State.CardRun.ActiveBattle;
	const TArray<FGameXXKCardEnemyIntent> EnemyIntentsBeforeSkip = State.CardRun.EnemyIntents;
	const int32 IntentCursorBeforeSkip = State.CardRun.NextEnemyIntentIndex;
	const FGameXXKRuntimeState SkipFailureBase = State;
	FString Error;
	TestTrue(FString::Printf(TEXT("skip resolves the valid tiered pending reward gate: %s"), *Error),
		FGameXXKCardBattleAdapter::SkipPendingRouteReward(State, &Error));
	TestTrue(TEXT("skip preserves the complete active battle snapshot"),
		ActiveBattlesEqual(State.CardRun.ActiveBattle, ActiveBattleBeforeSkip));
	TestTrue(TEXT("skip preserves all saved enemy intents"),
		EnemyIntentsEqual(State.CardRun.EnemyIntents, EnemyIntentsBeforeSkip));
	TestEqual(TEXT("skip preserves the enemy-intent cursor"),
		State.CardRun.NextEnemyIntentIndex,
		IntentCursorBeforeSkip);
	TestTrue(TEXT("skip clears the pending tiered reward"), State.CardRun.PendingReward.Options.IsEmpty());
	TestTrue(TEXT("skip clears the legacy pending reward cards"), State.CardRun.PendingReward.CardIds.IsEmpty());
	TestTrue(TEXT("skip resolves the battle reward gate"), State.CardRun.bActiveBattleRewardResolved);
	TestEqual(TEXT("skip does not advance the dedicated entry ordinal"), State.CardRun.NextRouteCardEntryOrdinal, NextEntryBefore);
	TestEqual(TEXT("skip does not advance acquisition history"), State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, 6);
	TestTrue(TEXT("skip preserves every stable route entry"), EntriesEqual(State.CardRun.RouteCardEntries, EntriesBefore));

	const auto ExpectSkipFailure = [this](const TCHAR* FailureLabel, FGameXXKRuntimeState Candidate)
	{
		const FString Before = ExportState(Candidate);
		FString FailureError;
		TestFalse(FailureLabel, FGameXXKCardBattleAdapter::SkipPendingRouteReward(Candidate, &FailureError));
		TestTrue(FString::Printf(TEXT("%s reports a concrete error"), FailureLabel), !FailureError.IsEmpty());
		TestEqual(FString::Printf(TEXT("%s leaves the complete runtime byte-identical"), FailureLabel),
			ExportState(Candidate),
			Before);
	};

	FGameXXKRuntimeState MixedAuthority = SkipFailureBase;
	MixedAuthority.CardRun.RouteCardIds.Add(TEXT("Route.General.PoJiaTuCi"));
	ExpectSkipFailure(TEXT("skip rejects mixed stable and legacy authority"), MixedAuthority);

	FGameXXKRuntimeState NonVictoryGate = SkipFailureBase;
	NonVictoryGate.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Player;
	ExpectSkipFailure(TEXT("skip rejects a non-victory battle gate"), NonVictoryGate);

	FGameXXKRuntimeState ResolvedGate = SkipFailureBase;
	ResolvedGate.CardRun.bActiveBattleRewardResolved = true;
	ExpectSkipFailure(TEXT("skip rejects an already-resolved reward gate"), ResolvedGate);

	FGameXXKRuntimeState MalformedPending = SkipFailureBase;
	MalformedPending.CardRun.PendingReward.Options.SetNum(2);
	ExpectSkipFailure(TEXT("skip rejects malformed pending reward metadata"), MalformedPending);
	return true;
}

#endif
