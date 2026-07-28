#include "GameXXKCardBattleAdapter.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKRouteCardRecipe.h"
#include "GameXXKRunDeckRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKRouteRewardEntryAcquisitionTest
{
	const FName PlayerUnitId(TEXT("Player"));
	const FName DirectRewardCardId(TEXT("Route.Rare.GuJuanCanZhang"));
	const FName MergeRewardCardId(TEXT("Route.General.PoJiaTuCi"));
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

	bool StartRewardReadyState(FGameXXKRuntimeState& OutState, const int32 RootSeed = 0x2468)
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
		OutState.DungeonNodeIndex = 1;
		if (!UGameXXKMVPRules::AdvanceDungeonNode(OutState, EGameXXKNodeKind::Battle))
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

	void SetPendingReward(FGameXXKRuntimeState& State, const FName RewardCardId)
	{
		FGameXXKPendingRouteCardReward& Pending = State.CardRun.PendingReward;
		Pending.SourceNodeId = GetRewardSourceNodeId(State);
		Pending.ChoiceSeed = 0x5A17;
		Pending.CardIds = {
			RewardCardId,
			RewardCardId == MergeRewardCardId ? DirectRewardCardId : MergeRewardCardId,
			FName(TEXT("Route.Terrain.DuanYaLuoShi"))};
		Pending.bRequiresRouteCardReplacement = false;
		State.CardRun.bActiveBattleRewardResolved = false;
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
			if (Definition.Owner == EGameXXKCardOwner::Route && !ExcludedCardIds.Contains(Definition.Id))
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
	if (!TestTrue(TEXT("preview fixture enters a canonical reward-ready route"), StartRewardReadyState(State)))
	{
		return false;
	}
	SetPendingReward(State, DirectRewardCardId);
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
	TestTrue(FString::Printf(TEXT("the first pending reward preview succeeds: %s"), *Error),
		FGameXXKCardBattleAdapter::PreviewPendingRouteReward(
			State,
			DirectRewardCardId,
			NAME_None,
			FirstPreview,
			&Error));
	TestEqual(TEXT("the first preview is byte-pure over the complete runtime"), ExportState(State), BeforePreview);
	TestTrue(TEXT("the repeated pending reward preview succeeds"),
		FGameXXKCardBattleAdapter::PreviewPendingRouteReward(
			State,
			DirectRewardCardId,
			NAME_None,
			SecondPreview,
			&Error));
	TestEqual(TEXT("the repeated preview remains byte-pure over the complete runtime"), ExportState(State), BeforePreview);
	TestTrue(TEXT("repeated previews are deterministic"), PreviewsEqual(FirstPreview, SecondPreview));
	TestEqual(TEXT("a non-full acquisition can commit without replacement"), FirstPreview.Decision, EGameXXKRouteCardAcquisitionDecision::CanCommit);
	TestFalse(TEXT("the distinct direct candidate does not merge"), FirstPreview.Merge.bWillMerge);
	TestEqual(TEXT("the preview exposes the exact stable candidate ID"), FirstPreview.Merge.SurvivorEntryId, ExpectedEntryId);
	const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(DirectRewardCardId);
	TestNotNull(TEXT("the direct reward candidate is catalog-backed"), Definition);
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
	TestTrue(FString::Printf(TEXT("the direct reward commits through the shared acquisition rules: %s"), *Error),
		FGameXXKCardBattleAdapter::ChoosePendingRouteReward(State, DirectRewardCardId, NAME_None, &Error));
	TestTrue(TEXT("direct reward commit preserves the complete active battle snapshot"),
		ActiveBattlesEqual(State.CardRun.ActiveBattle, ActiveBattleBeforeCommit));
	TestTrue(TEXT("direct reward commit preserves all saved enemy intents"),
		EnemyIntentsEqual(State.CardRun.EnemyIntents, EnemyIntentsBeforeCommit));
	TestEqual(TEXT("direct reward commit preserves the enemy-intent cursor"),
		State.CardRun.NextEnemyIntentIndex,
		IntentCursorBeforeCommit);
	TestEqual(TEXT("a direct commit advances the dedicated entry ordinal exactly once"), State.CardRun.NextRouteCardEntryOrdinal, NextEntryBefore + 1);
	TestEqual(TEXT("CommitAcquisition advances acquisition history exactly once"),
		State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount,
		AcquisitionCountBefore + 1);
	const FGameXXKRouteCardEntry* AddedEntry = FindEntry(State, ExpectedEntryId);
	TestNotNull(TEXT("the committed direct reward retains the previewed stable entry"), AddedEntry);
	if (AddedEntry && Definition)
	{
		TestEqual(TEXT("the committed entry retains the selected CardId"), AddedEntry->CardId, DirectRewardCardId);
		TestEqual(TEXT("the committed entry starts at catalog BaseQuality"), AddedEntry->CurrentQuality, Definition->BaseQuality);
		TestEqual(TEXT("the committed entry records route-reward provenance"), AddedEntry->SourceKind, EGameXXKRouteCardSourceKind::RouteReward);
		TestEqual(TEXT("the committed entry belongs to the player"), AddedEntry->OwnerUnitId, PlayerUnitId);
		TestTrue(TEXT("the committed entry is route-temporary"), AddedEntry->bTemporaryRouteCard);
		TestTrue(TEXT("the committed entry consumes one route capacity slot"), AddedEntry->bConsumesRouteCapacity);
		TestEqual(TEXT("the committed entry retains the dedicated acquisition ordinal"), AddedEntry->AcquisitionOrdinal, NextEntryBefore);
	}
	TestTrue(TEXT("successful commit clears the saved offer"), State.CardRun.PendingReward.CardIds.IsEmpty());
	TestTrue(TEXT("successful commit resolves the battle reward gate"), State.CardRun.bActiveBattleRewardResolved);
	TestTrue(TEXT("the entry path never appends to legacy RouteCardIds"), State.CardRun.RouteCardIds.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardEntryMergeAtCapacityTest,
	"GameXXK.Integration.CardRoute.RewardEntryAcquisition.MergeAtCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardEntryMergeAtCapacityTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteRewardEntryAcquisitionTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("merge fixture enters a canonical reward-ready route"), StartRewardReadyState(State))
		|| !TestTrue(TEXT("merge fixture fills exactly twelve capacity entries"), FillCapacity(State, {MergeRewardCardId})))
	{
		return false;
	}
	SetPendingReward(State, MergeRewardCardId);
	State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 7;
	const int32 NextEntryBefore = State.CardRun.NextRouteCardEntryOrdinal;
	FName CandidateEntryId;
	FString Error;
	TestTrue(TEXT("merge fixture resolves the candidate stable ID"),
		FGameXXKRouteCardRecipe::MakeStableEntryId(
			State.CardRun.RouteProgress.RootSeed,
			NextEntryBefore,
			CandidateEntryId,
			&Error));
	const FGameXXKRouteCardEntry* ExistingCommon = State.CardRun.RouteCardEntries.FindByPredicate([](const FGameXXKRouteCardEntry& Entry)
	{
		return Entry.CardId == MergeRewardCardId && Entry.CurrentQuality == EGameXXKCardQuality::Common;
	});
	TestNotNull(TEXT("canonical recipe contains the common merge survivor"), ExistingCommon);
	const FName ExistingCommonEntryId = ExistingCommon ? ExistingCommon->EntryId : NAME_None;

	FGameXXKRouteCardAcquisitionPreview Preview;
	TestTrue(FString::Printf(TEXT("the same-common reward previews at full capacity: %s"), *Error),
		FGameXXKCardBattleAdapter::PreviewPendingRouteReward(State, MergeRewardCardId, NAME_None, Preview, &Error));
	TestEqual(TEXT("merge-only acquisition can commit without replacement at twelve slots"), Preview.Decision, EGameXXKRouteCardAcquisitionDecision::CanCommit);
	TestTrue(TEXT("same common reward previews a merge"), Preview.Merge.bWillMerge);
	TestEqual(TEXT("the canonical non-capacity base entry survives"), Preview.Merge.SurvivorEntryId, ExistingCommonEntryId);
	TestTrue(TEXT("the new capacity candidate is consumed by the merge"), Preview.Merge.ConsumedEntryIds.Contains(CandidateEntryId));
	TestEqual(TEXT("full merge begins at twelve acquired slots"), Preview.CapacityBefore, FGameXXKRunDeckRules::MaxRouteCardCapacity);
	TestEqual(TEXT("full merge remains at twelve acquired slots"), Preview.CapacityAfter, FGameXXKRunDeckRules::MaxRouteCardCapacity);

	const FName UnnecessaryReplacementId = State.CardRun.RouteCardEntries.FindByPredicate([](const FGameXXKRouteCardEntry& Entry)
	{
		return Entry.bConsumesRouteCapacity;
	})->EntryId;
	FGameXXKRuntimeState UnnecessaryReplacementState = State;
	const FString BeforeUnnecessaryReplacement = ExportState(UnnecessaryReplacementState);
	TestFalse(TEXT("a merge-only candidate rejects an unnecessary replacement"),
		FGameXXKCardBattleAdapter::ChoosePendingRouteReward(
			UnnecessaryReplacementState,
			MergeRewardCardId,
			UnnecessaryReplacementId,
			&Error));
	TestEqual(TEXT("unnecessary replacement rejection rolls back the complete runtime"),
		ExportState(UnnecessaryReplacementState),
		BeforeUnnecessaryReplacement);

	FGameXXKRuntimeState LegacyFalseState = State;
	FGameXXKRuntimeState LegacyTrueState = State;
	LegacyFalseState.CardRun.PendingReward.bRequiresRouteCardReplacement = false;
	LegacyTrueState.CardRun.PendingReward.bRequiresRouteCardReplacement = true;
	const FGameXXKCardBattleRuntime ActiveBattleBeforeMerge = State.CardRun.ActiveBattle;
	const TArray<FGameXXKCardEnemyIntent> EnemyIntentsBeforeMerge = State.CardRun.EnemyIntents;
	const int32 IntentCursorBeforeMerge = State.CardRun.NextEnemyIntentIndex;
	TestTrue(TEXT("merge-only commit succeeds when the legacy flag is false"),
		FGameXXKCardBattleAdapter::ChoosePendingRouteReward(LegacyFalseState, MergeRewardCardId, NAME_None, &Error));
	TestTrue(TEXT("merge-only commit succeeds when the legacy flag is flipped true"),
		FGameXXKCardBattleAdapter::ChoosePendingRouteReward(LegacyTrueState, MergeRewardCardId, NAME_None, &Error));
	TestTrue(TEXT("merge-only commit preserves the complete active battle snapshot"),
		ActiveBattlesEqual(LegacyFalseState.CardRun.ActiveBattle, ActiveBattleBeforeMerge));
	TestTrue(TEXT("merge-only commit preserves all saved enemy intents"),
		EnemyIntentsEqual(LegacyFalseState.CardRun.EnemyIntents, EnemyIntentsBeforeMerge));
	TestEqual(TEXT("merge-only commit preserves the enemy-intent cursor"),
		LegacyFalseState.CardRun.NextEnemyIntentIndex,
		IntentCursorBeforeMerge);
	TestEqual(TEXT("the legacy replacement flag has no behavioral effect"), ExportState(LegacyTrueState), ExportState(LegacyFalseState));
	TestEqual(TEXT("merge-only commit advances the dedicated entry ordinal exactly once"),
		LegacyFalseState.CardRun.NextRouteCardEntryOrdinal,
		NextEntryBefore + 1);
	TestEqual(TEXT("merge-only CommitAcquisition advances history exactly once"),
		LegacyFalseState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount,
		8);
	TestEqual(TEXT("merge-only commit retains exactly twelve capacity entries"),
		CountCapacityEntries(LegacyFalseState),
		FGameXXKRunDeckRules::MaxRouteCardCapacity);
	const FGameXXKRouteCardEntry* Survivor = FindEntry(LegacyFalseState, ExistingCommonEntryId);
	TestNotNull(TEXT("the canonical merge survivor remains present"), Survivor);
	if (Survivor)
	{
		TestEqual(TEXT("the common survivor upgrades to Rare"), Survivor->CurrentQuality, EGameXXKCardQuality::Rare);
	}
	TestNull(TEXT("the consumed candidate stable ID is not persisted after merge"), FindEntry(LegacyFalseState, CandidateEntryId));
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
	if (!TestTrue(TEXT("replacement fixture enters a canonical reward-ready route"), StartRewardReadyState(State))
		|| !TestTrue(TEXT("first duplicate CardId entry is added"), AddRouteEntry(State, DuplicateCardId, EGameXXKCardQuality::Epic))
		|| !TestTrue(TEXT("second duplicate CardId entry is added"), AddRouteEntry(State, DuplicateCardId, EGameXXKCardQuality::Epic))
		|| !TestTrue(TEXT("replacement fixture fills exactly twelve capacity entries"), FillCapacity(State, {DirectRewardCardId, DuplicateCardId})))
	{
		return false;
	}
	SetPendingReward(State, DirectRewardCardId);
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
	TestTrue(TEXT("full non-merge candidate previews successfully without a replacement selection"),
		FGameXXKCardBattleAdapter::PreviewPendingRouteReward(State, DirectRewardCardId, NAME_None, Preview, &Error));
	TestEqual(TEXT("full non-merge candidate requires replacement"), Preview.Decision, EGameXXKRouteCardAcquisitionDecision::RequiresReplacement);
	TestTrue(TEXT("both duplicate CardId entries are independently eligible"),
		Preview.EligibleReplacementEntryIds.Contains(RemovedEntryId)
			&& Preview.EligibleReplacementEntryIds.Contains(PreservedEntryId));

	FGameXXKRuntimeState MissingReplacementState = State;
	const FString BeforeMissingReplacement = ExportState(MissingReplacementState);
	TestFalse(TEXT("full non-merge choice rejects a missing replacement"),
		FGameXXKCardBattleAdapter::ChoosePendingRouteReward(MissingReplacementState, DirectRewardCardId, NAME_None, &Error));
	TestEqual(TEXT("missing replacement rejection rolls back the complete runtime"),
		ExportState(MissingReplacementState),
		BeforeMissingReplacement);

	FGameXXKRuntimeState CardIdReplacementState = State;
	const FString BeforeCardIdReplacement = ExportState(CardIdReplacementState);
	TestFalse(TEXT("CardId cannot masquerade as a stable replacement EntryId"),
		FGameXXKCardBattleAdapter::ChoosePendingRouteReward(
			CardIdReplacementState,
			DirectRewardCardId,
			DuplicateCardId,
			&Error));
	TestEqual(TEXT("CardId-as-EntryId rejection rolls back the complete runtime"),
		ExportState(CardIdReplacementState),
		BeforeCardIdReplacement);

	TestTrue(FString::Printf(TEXT("an exact eligible EntryId commits the full non-merge reward: %s"), *Error),
		FGameXXKCardBattleAdapter::ChoosePendingRouteReward(State, DirectRewardCardId, RemovedEntryId, &Error));
	TestNull(TEXT("only the selected duplicate entry is removed"), FindEntry(State, RemovedEntryId));
	TestNotNull(TEXT("the other duplicate CardId entry remains distinguishable"), FindEntry(State, PreservedEntryId));
	TestNotNull(TEXT("the replacement candidate is committed under its stable ID"), FindEntry(State, CandidateEntryId));
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
	if (!TestTrue(TEXT("rollback fixture enters a canonical reward-ready route"), StartRewardReadyState(BaseState)))
	{
		return false;
	}
	SetPendingReward(BaseState, DirectRewardCardId);
	FString Error;
	const auto ExpectAtomicFailure = [this, &Error](
		const TCHAR* FailureLabel,
		FGameXXKRuntimeState State,
		const FName RewardCardId,
		const FName ReplacementEntryId)
	{
		const FString Before = ExportState(State);
		Error.Reset();
		const bool bSucceeded = FGameXXKCardBattleAdapter::ChoosePendingRouteReward(
			State,
			RewardCardId,
			ReplacementEntryId,
			&Error);
		TestFalse(FailureLabel, bSucceeded);
		TestTrue(FString::Printf(TEXT("%s reports a concrete error"), FailureLabel), !Error.IsEmpty());
		TestEqual(FString::Printf(TEXT("%s leaves the complete runtime byte-identical"), FailureLabel), ExportState(State), Before);
	};

	const FName ExistingEntryId = BaseState.CardRun.RouteCardEntries[0].EntryId;
	ExpectAtomicFailure(TEXT("unnecessary replacement"), BaseState, DirectRewardCardId, ExistingEntryId);
	ExpectAtomicFailure(TEXT("unknown reward candidate"), BaseState, TEXT("Route.Unknown.Reward"), NAME_None);

	FGameXXKRuntimeState MixedAuthority = BaseState;
	MixedAuthority.CardRun.RouteCardIds.Add(MergeRewardCardId);
	ExpectAtomicFailure(TEXT("mixed entry and legacy authority"), MixedAuthority, DirectRewardCardId, NAME_None);

	FGameXXKRuntimeState NegativeOrdinal = BaseState;
	NegativeOrdinal.CardRun.NextRouteCardEntryOrdinal = -1;
	ExpectAtomicFailure(TEXT("negative entry ordinal"), NegativeOrdinal, DirectRewardCardId, NAME_None);

	FGameXXKRuntimeState ExhaustedOrdinal = BaseState;
	ExhaustedOrdinal.CardRun.NextRouteCardEntryOrdinal = MAX_int32;
	ExpectAtomicFailure(TEXT("exhausted entry ordinal"), ExhaustedOrdinal, DirectRewardCardId, NAME_None);

	FGameXXKRuntimeState CommitFailure = BaseState;
	CommitFailure.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = MAX_int32;
	ExpectAtomicFailure(TEXT("shared acquisition commit rejection"), CommitFailure, DirectRewardCardId, NAME_None);

	FGameXXKRuntimeState InvalidRootSeed = BaseState;
	InvalidRootSeed.CardRun.RouteProgress.RootSeed = 0;
	ExpectAtomicFailure(TEXT("zero route root seed"), InvalidRootSeed, DirectRewardCardId, NAME_None);

	FGameXXKRuntimeState InactiveRoute = BaseState;
	InactiveRoute.bDungeonActive = false;
	ExpectAtomicFailure(TEXT("inactive route"), InactiveRoute, DirectRewardCardId, NAME_None);

	FGameXXKRuntimeState LegacyOnly = BaseState;
	LegacyOnly.CardRun.RouteCardEntries.Reset();
	LegacyOnly.CardRun.RouteCardIds = {MergeRewardCardId};
	ExpectAtomicFailure(TEXT("legacy-only route authority"), LegacyOnly, DirectRewardCardId, NAME_None);

	FGameXXKRuntimeState InvalidGate = BaseState;
	InvalidGate.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Player;
	ExpectAtomicFailure(TEXT("non-victory reward gate"), InvalidGate, DirectRewardCardId, NAME_None);

	FGameXXKRuntimeState ResolvedGate = BaseState;
	ResolvedGate.CardRun.bActiveBattleRewardResolved = true;
	ExpectAtomicFailure(TEXT("already-resolved reward gate"), ResolvedGate, DirectRewardCardId, NAME_None);

	FGameXXKRuntimeState InvalidPending = BaseState;
	InvalidPending.CardRun.PendingReward.CardIds.SetNum(2);
	ExpectAtomicFailure(TEXT("incomplete pending reward metadata"), InvalidPending, DirectRewardCardId, NAME_None);

	FGameXXKRuntimeState InvalidCatalog = BaseState;
	InvalidCatalog.CardRun.PendingReward.CardIds[0] = TEXT("Route.Unknown.Reward");
	ExpectAtomicFailure(TEXT("unknown pending catalog card"), InvalidCatalog, TEXT("Route.Unknown.Reward"), NAME_None);
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
	if (!TestTrue(TEXT("offer probe enters a canonical reward-ready route"), StartRewardReadyState(ProbeState, RootSeed)))
	{
		return false;
	}
	const int32 SourceNodeId = GetRewardSourceNodeId(ProbeState);
	FGameXXKRuntimeState EmptyFlagTrueState = ProbeState;
	EmptyFlagTrueState.CardRun.PendingReward.bRequiresRouteCardReplacement = true;
	TArray<FName> BaselineOffer;
	TArray<FName> EmptyFlagTrueOffer;
	FString Error;
	TestTrue(FString::Printf(TEXT("baseline offer resolves three deterministic cards: %s"), *Error),
		FGameXXKCardBattleAdapter::CreateRouteRewardOffer(
			ProbeState,
			EGameXXKNodeKind::Battle,
			SourceNodeId,
			ChoiceSeed,
			BaselineOffer,
			&Error));
	TestTrue(TEXT("an empty pending legacy flag does not block offer creation"),
		FGameXXKCardBattleAdapter::CreateRouteRewardOffer(
			EmptyFlagTrueState,
			EGameXXKNodeKind::Battle,
			SourceNodeId,
			ChoiceSeed,
			EmptyFlagTrueOffer,
			&Error));
	TestEqual(TEXT("empty pending legacy flag does not change offered IDs"), EmptyFlagTrueOffer, BaselineOffer);
	TestEqual(TEXT("empty pending legacy flag does not change the committed runtime"),
		ExportState(EmptyFlagTrueState),
		ExportState(ProbeState));
	if (!TestEqual(TEXT("baseline offer contains exactly three cards"), BaselineOffer.Num(), 3))
	{
		return false;
	}

	FGameXXKRuntimeState FullState;
	if (!TestTrue(TEXT("full offer fixture enters the same canonical reward-ready route"), StartRewardReadyState(FullState, RootSeed)))
	{
		return false;
	}
	for (const FName OfferedCardId : BaselineOffer)
	{
		if (!TestTrue(TEXT("first pre-existing offered-card copy is added"),
			AddRouteEntry(FullState, OfferedCardId, EGameXXKCardQuality::Common))
			|| !TestTrue(TEXT("second pre-existing offered-card copy is added"),
				AddRouteEntry(FullState, OfferedCardId, EGameXXKCardQuality::Rare)))
		{
			return false;
		}
	}
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (CountCapacityEntries(FullState) >= FGameXXKRunDeckRules::MaxRouteCardCapacity)
		{
			break;
		}
		if (Definition.Owner == EGameXXKCardOwner::Route && !BaselineOffer.Contains(Definition.Id))
		{
			if (!TestTrue(TEXT("full offer fixture adds a non-Epic capacity filler"),
				AddRouteEntry(FullState, Definition.Id, EGameXXKCardQuality::Common)))
			{
				return false;
			}
		}
	}
	if (!TestEqual(TEXT("offer fixture reaches exactly twelve acquired slots"),
		CountCapacityEntries(FullState),
		FGameXXKRunDeckRules::MaxRouteCardCapacity))
	{
		return false;
	}
	for (const FName OfferedCardId : BaselineOffer)
	{
		TestTrue(TEXT("each deterministic offer remains eligible despite at least two existing copies"),
			CountEntriesByCardId(FullState, OfferedCardId) >= 2);
	}

	FullState.CardRun.NextRewardOrdinal = 11;
	const int32 NextRewardBefore = FullState.CardRun.NextRewardOrdinal;
	const int32 NextEntryBefore = FullState.CardRun.NextRouteCardEntryOrdinal;
	TArray<FName> FullOffer;
	TestTrue(FString::Printf(TEXT("a full-capacity deck still receives a three-card offer: %s"), *Error),
		FGameXXKCardBattleAdapter::CreateRouteRewardOffer(
			FullState,
			EGameXXKNodeKind::Battle,
			GetRewardSourceNodeId(FullState),
			ChoiceSeed,
			FullOffer,
			&Error));
	TestEqual(TEXT("full-capacity offer ignores the obsolete two-copy exclusion"), FullOffer, BaselineOffer);
	TestEqual(TEXT("the first new pending offer advances NextRewardOrdinal exactly once"),
		FullState.CardRun.NextRewardOrdinal,
		NextRewardBefore + 1);
	TestEqual(TEXT("creating an offer never advances the stable entry ordinal"),
		FullState.CardRun.NextRouteCardEntryOrdinal,
		NextEntryBefore);
	TestFalse(TEXT("new offers persist the legacy replacement flag as false"),
		FullState.CardRun.PendingReward.bRequiresRouteCardReplacement);

	const FString BeforeRepeatedOffer = ExportState(FullState);
	TArray<FName> RepeatedOffer;
	TestTrue(TEXT("repeating the same pending offer succeeds"),
		FGameXXKCardBattleAdapter::CreateRouteRewardOffer(
			FullState,
			EGameXXKNodeKind::Battle,
			GetRewardSourceNodeId(FullState),
			ChoiceSeed,
			RepeatedOffer,
			&Error));
	TestEqual(TEXT("repeated pending offer returns identical card IDs"), RepeatedOffer, FullOffer);
	TestEqual(TEXT("repeated pending offer does not mutate any runtime byte"), ExportState(FullState), BeforeRepeatedOffer);
	TestEqual(TEXT("repeated pending offer does not advance NextRewardOrdinal"), FullState.CardRun.NextRewardOrdinal, NextRewardBefore + 1);
	TestEqual(TEXT("repeated pending offer does not advance the stable entry ordinal"), FullState.CardRun.NextRouteCardEntryOrdinal, NextEntryBefore);

	FullState.CardRun.NextRewardOrdinal = MAX_int32;
	const FString BeforeRepeatedAtMax = ExportState(FullState);
	TArray<FName> RepeatedAtMax;
	TestTrue(TEXT("an already-pending offer remains readable at the reward-ordinal limit"),
		FGameXXKCardBattleAdapter::CreateRouteRewardOffer(
			FullState,
			EGameXXKNodeKind::Battle,
			GetRewardSourceNodeId(FullState),
			ChoiceSeed,
			RepeatedAtMax,
			&Error));
	TestEqual(TEXT("pending-at-limit returns the same three IDs"), RepeatedAtMax, FullOffer);
	TestEqual(TEXT("pending-at-limit remains byte-pure"), ExportState(FullState), BeforeRepeatedAtMax);

	const auto ExpectOfferFailure = [this, &Error](
		const TCHAR* FailureLabel,
		FGameXXKRuntimeState State,
		const EGameXXKNodeKind NodeKind,
		const int32 SourceNode,
		const int32 Seed)
	{
		const FString Before = ExportState(State);
		TArray<FName> OutIds = {TEXT("Sentinel.Should.Clear")};
		Error.Reset();
		const bool bSucceeded = FGameXXKCardBattleAdapter::CreateRouteRewardOffer(
			State,
			NodeKind,
			SourceNode,
			Seed,
			OutIds,
			&Error);
		TestFalse(FailureLabel, bSucceeded);
		TestTrue(FString::Printf(TEXT("%s reports a concrete error"), FailureLabel), !Error.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s clears the output IDs"), FailureLabel), OutIds.IsEmpty());
		TestEqual(FString::Printf(TEXT("%s leaves the complete runtime byte-identical"), FailureLabel), ExportState(State), Before);
	};

	FGameXXKRuntimeState FreshFailureBase;
	if (!TestTrue(TEXT("offer rollback fixture enters a canonical reward-ready route"), StartRewardReadyState(FreshFailureBase, RootSeed)))
	{
		return false;
	}
	FGameXXKRuntimeState EntryOrdinalExhaustedOffer = FreshFailureBase;
	EntryOrdinalExhaustedOffer.CardRun.NextRouteCardEntryOrdinal = MAX_int32;
	EntryOrdinalExhaustedOffer.CardRun.NextRewardOrdinal = 3;
	TArray<FName> EntryOrdinalExhaustedIds;
	TestTrue(TEXT("offer creation does not require an incrementable entry ordinal"),
		FGameXXKCardBattleAdapter::CreateRouteRewardOffer(
			EntryOrdinalExhaustedOffer,
			EGameXXKNodeKind::Battle,
			GetRewardSourceNodeId(EntryOrdinalExhaustedOffer),
			ChoiceSeed,
			EntryOrdinalExhaustedIds,
			&Error));
	TestEqual(TEXT("entry-ordinal exhaustion still produces three offers"), EntryOrdinalExhaustedIds.Num(), 3);
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
	MixedAuthority.CardRun.RouteCardIds.Add(MergeRewardCardId);
	ExpectOfferFailure(TEXT("offer mixed authority"), MixedAuthority, EGameXXKNodeKind::Battle, GetRewardSourceNodeId(MixedAuthority), ChoiceSeed);

	FGameXXKRuntimeState OverCapacity = FreshFailureBase;
	if (!TestTrue(TEXT("over-capacity fixture fills twelve slots"), FillCapacity(OverCapacity))
		|| !TestTrue(TEXT("over-capacity fixture adds the corrupt thirteenth slot"),
			AddRouteEntry(OverCapacity, TEXT("Route.Boss.HuPoZhenDan"), EGameXXKCardQuality::Epic)))
	{
		return false;
	}
	ExpectOfferFailure(TEXT("offer corrupt capacity above twelve"), OverCapacity, EGameXXKNodeKind::Battle, GetRewardSourceNodeId(OverCapacity), ChoiceSeed);

	FGameXXKRuntimeState InsufficientCatalog = FreshFailureBase;
	int32 KeptEligible = 0;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Owner != EGameXXKCardOwner::Route)
		{
			continue;
		}
		if (KeptEligible < 2)
		{
			++KeptEligible;
			continue;
		}
		if (!TestTrue(TEXT("insufficient-catalog fixture marks a route card at Epic without consuming capacity"),
			AddRouteEntry(
				InsufficientCatalog,
				Definition.Id,
				EGameXXKCardQuality::Epic,
				false,
				EGameXXKRouteCardSourceKind::RouteBase)))
		{
			return false;
		}
	}
	ExpectOfferFailure(TEXT("fewer than three eligible reward candidates"), InsufficientCatalog, EGameXXKNodeKind::Battle, GetRewardSourceNodeId(InsufficientCatalog), ChoiceSeed);

	FGameXXKRuntimeState InvalidGate = FreshFailureBase;
	InvalidGate.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Player;
	ExpectOfferFailure(TEXT("offer outside the victory gate"), InvalidGate, EGameXXKNodeKind::Battle, GetRewardSourceNodeId(InvalidGate), ChoiceSeed);

	FGameXXKRuntimeState InitializationLeak = FreshFailureBase;
	InitializationLeak.CardRun.HeroUnlockedCardIds.Reset();
	InitializationLeak.CardRun.HeroSelectedCardIds = {TEXT("Hero.Invalid")};
	ExpectOfferFailure(TEXT("failed card-run initialization"), InitializationLeak, EGameXXKNodeKind::Battle, GetRewardSourceNodeId(InitializationLeak), ChoiceSeed);

	FGameXXKRuntimeState RecomputedSeedPending = ProbeState;
	const FString BeforeRecomputedSeed = ExportState(RecomputedSeedPending);
	TArray<FName> RecomputedSeedOffer = {TEXT("Sentinel.Should.Replace")};
	TestTrue(TEXT("an already-saved pending offer ignores a newly recomputed choice seed"),
		FGameXXKCardBattleAdapter::CreateRouteRewardOffer(
			RecomputedSeedPending,
			EGameXXKNodeKind::Battle,
			GetRewardSourceNodeId(RecomputedSeedPending),
			ChoiceSeed + 1,
			RecomputedSeedOffer,
			&Error));
	TestEqual(TEXT("the saved pending offer remains authoritative over a recomputed seed"), RecomputedSeedOffer, BaselineOffer);
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
	if (!TestTrue(TEXT("double-resolve fixture enters a canonical reward-ready route"), StartRewardReadyState(State)))
	{
		return false;
	}

	TestTrue(TEXT("the first victory resolution creates the saved three-card offer"),
		UGameXXKMVPRules::ResolveBattleVictory(State, false));
	TestEqual(TEXT("the first victory resolution persists exactly three cards"), State.CardRun.PendingReward.CardIds.Num(), 3);
	const FString BeforeSecondResolve = ExportState(State);
	const TArray<FName> PendingBeforeSecondResolve = State.CardRun.PendingReward.CardIds;
	const int32 RewardOrdinalBeforeSecondResolve = State.CardRun.NextRewardOrdinal;

	TestTrue(TEXT("repeating victory resolution while the reward is pending remains successful"),
		UGameXXKMVPRules::ResolveBattleVictory(State, false));
	TestEqual(TEXT("the repeated victory resolution returns the same saved offer"),
		State.CardRun.PendingReward.CardIds,
		PendingBeforeSecondResolve);
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
	if (!TestTrue(TEXT("skip fixture enters a canonical reward-ready route"), StartRewardReadyState(State)))
	{
		return false;
	}
	SetPendingReward(State, DirectRewardCardId);
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
	TestTrue(FString::Printf(TEXT("skip resolves the valid pending reward gate: %s"), *Error),
		FGameXXKCardBattleAdapter::SkipPendingRouteReward(State, &Error));
	TestTrue(TEXT("skip preserves the complete active battle snapshot"),
		ActiveBattlesEqual(State.CardRun.ActiveBattle, ActiveBattleBeforeSkip));
	TestTrue(TEXT("skip preserves all saved enemy intents"),
		EnemyIntentsEqual(State.CardRun.EnemyIntents, EnemyIntentsBeforeSkip));
	TestEqual(TEXT("skip preserves the enemy-intent cursor"),
		State.CardRun.NextEnemyIntentIndex,
		IntentCursorBeforeSkip);
	TestTrue(TEXT("skip clears the pending reward"), State.CardRun.PendingReward.CardIds.IsEmpty());
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
	MixedAuthority.CardRun.RouteCardIds.Add(MergeRewardCardId);
	ExpectSkipFailure(TEXT("skip rejects mixed stable and legacy authority"), MixedAuthority);

	FGameXXKRuntimeState NonVictoryGate = SkipFailureBase;
	NonVictoryGate.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Player;
	ExpectSkipFailure(TEXT("skip rejects a non-victory battle gate"), NonVictoryGate);

	FGameXXKRuntimeState ResolvedGate = SkipFailureBase;
	ResolvedGate.CardRun.bActiveBattleRewardResolved = true;
	ExpectSkipFailure(TEXT("skip rejects an already-resolved reward gate"), ResolvedGate);

	FGameXXKRuntimeState MalformedPending = SkipFailureBase;
	MalformedPending.CardRun.PendingReward.CardIds.SetNum(2);
	ExpectSkipFailure(TEXT("skip rejects malformed pending reward metadata"), MalformedPending);
	return true;
}

#endif
