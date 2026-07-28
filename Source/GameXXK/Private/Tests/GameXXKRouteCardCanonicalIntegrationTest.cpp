#include "GameXXKCardBattleAdapter.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKRouteCardRecipe.h"
#include "GameXXKRunDeckRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKRouteCardCanonicalIntegrationTest
{
	constexpr int32 CanonicalEntryCount = FGameXXKRouteCardRecipe::BaseEntryCount;
	constexpr int32 FirstQuestNpcOrdinal = 13;
	constexpr int32 LastQuestNpcOrdinal = 15;
	const FName HeroUnitId(TEXT("Player"));
	const FName TransitionalRouteCardId(TEXT("Route.General.QingShenQuShi"));

	bool StartAcceptedRoute(
		FGameXXKRuntimeState& OutState,
		const int32 RootSeed,
		const bool bKeepQingshanFollower)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		if (!UGameXXKMVPRules::OpenWorldMap(OutState)
			|| !UGameXXKMVPRules::EnterWorldRegion(OutState, UGameXXKMVPRules::RegionQingshan())
			|| !UGameXXKMVPRules::AcceptTownQuest(OutState))
		{
			return false;
		}
		OutState.RouteSeed = RootSeed;
		OutState.bFollowerJoined = bKeepQingshanFollower;
		return UGameXXKMVPRules::EnterDungeon(OutState);
	}

	bool EntryEquals(const FGameXXKRouteCardEntry& A, const FGameXXKRouteCardEntry& B)
	{
		return FGameXXKRouteCardEntry::StaticStruct()->CompareScriptStruct(&A, &B, PPF_None);
	}

	bool EntriesEqual(const TArray<FGameXXKRouteCardEntry>& A, const TArray<FGameXXKRouteCardEntry>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (!EntryEquals(A[Index], B[Index]))
			{
				return false;
			}
		}
		return true;
	}

	const FGameXXKRouteCardEntry* FindEntryByOrdinal(
		const TArray<FGameXXKRouteCardEntry>& Entries,
		const int32 AcquisitionOrdinal)
	{
		return Entries.FindByPredicate([AcquisitionOrdinal](const FGameXXKRouteCardEntry& Entry)
		{
			return Entry.AcquisitionOrdinal == AcquisitionOrdinal;
		});
	}

	FGameXXKRouteCardEntry* FindEntryByOrdinal(
		TArray<FGameXXKRouteCardEntry>& Entries,
		const int32 AcquisitionOrdinal)
	{
		return Entries.FindByPredicate([AcquisitionOrdinal](const FGameXXKRouteCardEntry& Entry)
		{
			return Entry.AcquisitionOrdinal == AcquisitionOrdinal;
		});
	}

	TArray<FGameXXKCardInstance> CollectBattleInstances(const FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FGameXXKCardInstance> Instances = Runtime.Deck.DrawPile;
		Instances.Append(Runtime.Deck.Hand);
		Instances.Append(Runtime.Deck.DiscardPile);
		return Instances;
	}

	bool AppendAcquiredEntry(
		FGameXXKCardRunState& InOutRun,
		const EGameXXKCardQuality Quality,
		FString* OutError = nullptr)
	{
		FGameXXKRouteCardEntry Entry;
		Entry.CardId = TEXT("Route.General.PoJiaTuCi");
		Entry.CurrentQuality = Quality;
		Entry.SourceKind = EGameXXKRouteCardSourceKind::RouteReward;
		Entry.OwnerUnitId = HeroUnitId;
		Entry.bTemporaryRouteCard = true;
		Entry.bConsumesRouteCapacity = true;
		Entry.AcquisitionOrdinal = InOutRun.NextRouteCardEntryOrdinal;
		if (!FGameXXKRouteCardRecipe::MakeStableEntryId(
			InOutRun.RouteProgress.RootSeed,
			Entry.AcquisitionOrdinal,
			Entry.EntryId,
			OutError))
		{
			return false;
		}
		InOutRun.RouteCardEntries.Add(MoveTemp(Entry));
		++InOutRun.NextRouteCardEntryOrdinal;
		return true;
	}

	bool IsQuestNpcOrdinal(const int32 AcquisitionOrdinal)
	{
		return AcquisitionOrdinal >= FirstQuestNpcOrdinal
			&& AcquisitionOrdinal <= LastQuestNpcOrdinal;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCanonicalRouteEntryInitializationTest,
	"GameXXK.Integration.CardRoute.BattleEntry.CanonicalRouteEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCanonicalRouteEntryInitializationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteCardCanonicalIntegrationTest;
	constexpr int32 RootSeed = 0x2468;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("the fixed-seed accepted route opens"), StartAcceptedRoute(State, RootSeed, true)))
	{
		return false;
	}

	TestEqual(TEXT("route entry persists exactly the canonical eighteen entries"), State.CardRun.RouteCardEntries.Num(), CanonicalEntryCount);
	TestEqual(TEXT("the first acquired-entry sequence begins after the base recipe"), State.CardRun.NextRouteCardEntryOrdinal, CanonicalEntryCount);
	TestEqual(TEXT("the generated route seed becomes the three-chapter root seed"), State.CardRun.RouteProgress.RootSeed, RootSeed);
	TestEqual(TEXT("card-run randomness is aligned to the generated root seed"), State.CardRun.RouteRandomSeed, RootSeed);
	TestTrue(TEXT("new routes do not duplicate canonical entries into the legacy route-card list"), State.CardRun.RouteCardIds.IsEmpty());

	TArray<FGameXXKRouteCardEntry> ExpectedEntries;
	FString Error;
	TestTrue(TEXT("the persisted route entries can be replayed by the pure recipe"),
		FGameXXKRouteCardRecipe::BuildBaseEntries(State, RootSeed, ExpectedEntries, &Error));
	TestTrue(TEXT("route entry commits the exact canonical recipe"), EntriesEqual(State.CardRun.RouteCardEntries, ExpectedEntries));

	FGameXXKRuntimeState RejectedState = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("the rejected-route fixture reaches accepted Qingshan town"),
		UGameXXKMVPRules::OpenWorldMap(RejectedState)
		&& UGameXXKMVPRules::EnterWorldRegion(RejectedState, UGameXXKMVPRules::RegionQingshan())
		&& UGameXXKMVPRules::AcceptTownQuest(RejectedState));
	RejectedState.RouteSeed = 0x3579;
	RejectedState.bFollowerJoined = false;
	TestTrue(TEXT("the rejected-route fixture initializes its permanent card state"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(RejectedState, &Error));
	FGameXXKPermanentCompanion& Companion = RejectedState.CardRun.CompanionRoster.PermanentCompanions.AddDefaulted_GetRef();
	Companion.InstanceId = TEXT("Companion.CanonicalRoute.Invalid");
	Companion.Role = EGameXXKCharacterRole::Blade;
	Companion.bIsActive = true;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Owner == EGameXXKCardOwner::Profession
			&& Definition.Role == Companion.Role
			&& Companion.SelectedCardIds.Num() < 5)
		{
			Companion.SelectedCardIds.Add(Definition.Id);
		}
	}
	if (!TestEqual(TEXT("the corrupt companion fixture finds five valid same-role cards"), Companion.SelectedCardIds.Num(), 5))
	{
		return false;
	}
	Companion.PersonalCardIds = Companion.SelectedCardIds;
	Companion.UnlockedPersonalCardIds = Companion.SelectedCardIds;
	Companion.PersonalCardIds.Pop();
	Companion.UnlockedPersonalCardIds.Pop();
	const FGameXXKRuntimeState RejectedBefore = RejectedState;
	TestFalse(TEXT("route entry rejects a companion selection outside its personal unlocked pool"),
		UGameXXKMVPRules::EnterDungeon(RejectedState));
	TestTrue(TEXT("a late canonical-recipe failure preserves the complete caller state"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&RejectedState, &RejectedBefore, PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCanonicalBattleMaterializationTest,
	"GameXXK.Integration.CardBattleAdapter.CanonicalEntryMaterialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCanonicalBattleMaterializationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteCardCanonicalIntegrationTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("the battle materialization fixture enters a route"), StartAcceptedRoute(State, 0x468A, false)))
	{
		return false;
	}
	if (!TestEqual(TEXT("the battle fixture owns the full base recipe"), State.CardRun.RouteCardEntries.Num(), CanonicalEntryCount))
	{
		return false;
	}
	State.CardRun.RouteCardEntries[0].CurrentQuality = EGameXXKCardQuality::Rare;
	State.CardRun.RouteCardIds = {TransitionalRouteCardId, TransitionalRouteCardId};
	const TArray<FGameXXKRouteCardEntry> EntriesBeforeBattle = State.CardRun.RouteCardEntries;

	State.bHasGeneratedRouteMap = false;
	State.RouteMapNodes.Reset();
	State.RouteMapEdges.Reset();
	State.ReachableRouteNodeIds.Reset();
	State.DungeonNodeIndex = 1;
	if (!TestTrue(TEXT("persisted entries plus transitional legacy cards materialize into a battle"),
		UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Battle)))
	{
		return false;
	}
	TestTrue(TEXT("battle projection never mutates persisted route entries"), EntriesEqual(State.CardRun.RouteCardEntries, EntriesBeforeBattle));
	TestEqual(TEXT("eighteen entries plus two legacy cards create twenty battle instances"),
		State.CardRun.ActiveBattle.Deck.ActiveInstanceIds.Num(), CanonicalEntryCount + 2);

	const TArray<FGameXXKCardInstance> Instances = CollectBattleInstances(State.CardRun.ActiveBattle);
	TestEqual(TEXT("every active battle instance remains in exactly one materialized zone"), Instances.Num(), CanonicalEntryCount + 2);
	for (const FGameXXKRouteCardEntry& Entry : EntriesBeforeBattle)
	{
		TArray<const FGameXXKCardInstance*> Matches;
		for (const FGameXXKCardInstance& Instance : Instances)
		{
			if (Instance.SourceEntryId == Entry.EntryId)
			{
				Matches.Add(&Instance);
			}
		}
		TestEqual(FString::Printf(TEXT("entry %s projects exactly once"), *Entry.EntryId.ToString()), Matches.Num(), 1);
		if (Matches.Num() == 1)
		{
			const FGameXXKCardInstance& Instance = *Matches[0];
			TestEqual(TEXT("materialization preserves CardId"), Instance.CardId, Entry.CardId);
			TestEqual(TEXT("materialization preserves quality"), Instance.CurrentQuality, Entry.CurrentQuality);
			TestEqual(TEXT("materialization preserves owner"), Instance.OwnerUnitId, Entry.OwnerUnitId);
			TestEqual(TEXT("materialization preserves acquisition ordinal"), Instance.AcquisitionOrdinal, Entry.AcquisitionOrdinal);
			TestEqual(TEXT("the runtime instance id remains stable within this battle node"),
				Instance.InstanceId,
				FName(*FString::Printf(TEXT("CardRun.%d.%03d"), State.ActiveBattleNodeId, Entry.AcquisitionOrdinal)));
		}
	}

	const FGameXXKCardDefinition* TransitionalDefinition = FGameXXKCardCatalog::FindCardDefinition(TransitionalRouteCardId);
	TestNotNull(TEXT("the transitional compatibility card remains catalog-backed"), TransitionalDefinition);
	int32 TransitionalOnlyCount = 0;
	for (const FGameXXKCardInstance& Instance : Instances)
	{
		if (Instance.CardId != TransitionalRouteCardId
			|| EntriesBeforeBattle.ContainsByPredicate([&Instance](const FGameXXKRouteCardEntry& Entry)
			{
				return Entry.EntryId == Instance.SourceEntryId;
			}))
		{
			continue;
		}
		++TransitionalOnlyCount;
		if (TransitionalDefinition)
		{
			TestEqual(TEXT("legacy compatibility instances use catalog base quality"),
				Instance.CurrentQuality,
				TransitionalDefinition->BaseQuality);
		}
	}
	TestEqual(TEXT("both legacy compatibility entries are appended without the obsolete two-copy gate"), TransitionalOnlyCount, 2);

	const FGameXXKRuntimeState ActiveBattleBefore = State;
	FString Error;
	TestFalse(TEXT("a second BeginCardBattle call cannot overwrite a resumed active battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(
			State,
			EGameXXKNodeKind::Battle,
			EGameXXKCardTerrain::Plain,
			99173,
			&Error));
	TestTrue(TEXT("active-battle rejection reports a concrete error"), !Error.IsEmpty());
	TestTrue(TEXT("active-battle rejection preserves the complete resumed state"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &ActiveBattleBefore, PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCanonicalQuestNpcSlotReplacementTest,
	"GameXXK.Integration.CardRoute.QuestNpc.CanonicalSupportSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCanonicalQuestNpcSlotReplacementTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteCardCanonicalIntegrationTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("the support-slot fixture enters without an automatic NPC"), StartAcceptedRoute(State, 0x579B, false)))
	{
		return false;
	}
	for (int32 Ordinal = FirstQuestNpcOrdinal; Ordinal <= LastQuestNpcOrdinal; ++Ordinal)
	{
		FGameXXKRouteCardEntry* Entry = FindEntryByOrdinal(State.CardRun.RouteCardEntries, Ordinal);
		if (!TestNotNull(FString::Printf(TEXT("support slot %d exists"), Ordinal), Entry))
		{
			return false;
		}
		Entry->CurrentQuality = EGameXXKCardQuality::Rare;
	}
	FString Error;
	TestTrue(TEXT("the support fixture appends one acquired entry"),
		AppendAcquiredEntry(State.CardRun, EGameXXKCardQuality::Epic, &Error));
	State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 7;
	State.CardRun.NextRewardOrdinal = 31;
	State.CardRun.PendingReward.SourceNodeId = 41;
	State.CardRun.PendingReward.ChoiceSeed = 4101;
	State.CardRun.PendingReward.CardIds = {TEXT("Route.General.TuNaJue")};
	State.CardRun.PendingEvent.SourceNodeId = 42;
	State.CardRun.PendingEvent.ChoiceSeed = 4201;
	State.CardRun.PendingEvent.EventNpcId = TEXT("Npc.ZhouGuangZu");
	State.CardRun.PendingEvent.EncounterId = TEXT("Encounter.Event.CanonicalSupport");

	const TArray<FGameXXKRouteCardEntry> BeforeJoinEntries = State.CardRun.RouteCardEntries;
	const int32 NextEntryBefore = State.CardRun.NextRouteCardEntryOrdinal;
	const int32 AcquisitionCountBefore = State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount;
	const int32 NextRewardBefore = State.CardRun.NextRewardOrdinal;
	const FGameXXKPendingRouteCardReward PendingRewardBefore = State.CardRun.PendingReward;
	const FGameXXKPendingRouteEvent PendingEventBefore = State.CardRun.PendingEvent;
	if (!TestTrue(TEXT("a named task NPC atomically replaces only the three canonical support slots"),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, TEXT("Npc.TusiChief"), {}, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("joining an NPC does not change deck size"), State.CardRun.RouteCardEntries.Num(), BeforeJoinEntries.Num());
	TArray<FGameXXKRouteCardEntry> ExpectedJoinedBase;
	TestTrue(TEXT("the joined-NPC base recipe rebuilds for comparison"),
		FGameXXKRouteCardRecipe::BuildBaseEntries(State, State.CardRun.RouteProgress.RootSeed, ExpectedJoinedBase, &Error));
	for (const FGameXXKRouteCardEntry& BeforeEntry : BeforeJoinEntries)
	{
		const FGameXXKRouteCardEntry* AfterEntry = FindEntryByOrdinal(State.CardRun.RouteCardEntries, BeforeEntry.AcquisitionOrdinal);
		TestNotNull(TEXT("every pre-join ordinal remains present"), AfterEntry);
		if (!AfterEntry)
		{
			continue;
		}
		if (IsQuestNpcOrdinal(BeforeEntry.AcquisitionOrdinal))
		{
			const FGameXXKRouteCardEntry* ExpectedEntry = FindEntryByOrdinal(ExpectedJoinedBase, BeforeEntry.AcquisitionOrdinal);
			TestNotNull(TEXT("joined recipe contains the support ordinal"), ExpectedEntry);
			if (ExpectedEntry)
			{
				TestTrue(TEXT("the complete support slot is replaced, including base quality"), EntryEquals(*AfterEntry, *ExpectedEntry));
			}
		}
		else
		{
			TestTrue(TEXT("joining an NPC preserves every non-support entry exactly"), EntryEquals(*AfterEntry, BeforeEntry));
		}
	}
	TestEqual(TEXT("joining preserves the next stable-entry ordinal"), State.CardRun.NextRouteCardEntryOrdinal, NextEntryBefore);
	TestEqual(TEXT("joining preserves the actual acquisition count"), State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, AcquisitionCountBefore);
	TestEqual(TEXT("joining preserves the legacy reward sequence"), State.CardRun.NextRewardOrdinal, NextRewardBefore);
	TestTrue(TEXT("joining preserves the pending reward exactly"),
		FGameXXKPendingRouteCardReward::StaticStruct()->CompareScriptStruct(&State.CardRun.PendingReward, &PendingRewardBefore, PPF_None));
	TestTrue(TEXT("joining preserves the pending event exactly"),
		FGameXXKPendingRouteEvent::StaticStruct()->CompareScriptStruct(&State.CardRun.PendingEvent, &PendingEventBefore, PPF_None));

	for (int32 Ordinal = FirstQuestNpcOrdinal; Ordinal <= LastQuestNpcOrdinal; ++Ordinal)
	{
		FindEntryByOrdinal(State.CardRun.RouteCardEntries, Ordinal)->CurrentQuality = EGameXXKCardQuality::Epic;
	}
	const TArray<FGameXXKRouteCardEntry> BeforeRemovalEntries = State.CardRun.RouteCardEntries;
	if (!TestTrue(TEXT("clearing the task NPC restores the deterministic shared filler slots"),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, NAME_None, {}, &Error)))
	{
		return false;
	}
	TArray<FGameXXKRouteCardEntry> ExpectedRemovedBase;
	TestTrue(TEXT("the no-NPC base recipe rebuilds for comparison"),
		FGameXXKRouteCardRecipe::BuildBaseEntries(State, State.CardRun.RouteProgress.RootSeed, ExpectedRemovedBase, &Error));
	for (const FGameXXKRouteCardEntry& BeforeEntry : BeforeRemovalEntries)
	{
		const FGameXXKRouteCardEntry* AfterEntry = FindEntryByOrdinal(State.CardRun.RouteCardEntries, BeforeEntry.AcquisitionOrdinal);
		if (!AfterEntry)
		{
			AddError(TEXT("removal lost an existing acquisition ordinal"));
			continue;
		}
		if (IsQuestNpcOrdinal(BeforeEntry.AcquisitionOrdinal))
		{
			const FGameXXKRouteCardEntry* ExpectedEntry = FindEntryByOrdinal(ExpectedRemovedBase, BeforeEntry.AcquisitionOrdinal);
			TestTrue(TEXT("removal restores the whole filler slot and discards the old support upgrade"),
				ExpectedEntry && EntryEquals(*AfterEntry, *ExpectedEntry));
		}
		else
		{
			TestTrue(TEXT("removing an NPC preserves every non-support entry exactly"), EntryEquals(*AfterEntry, BeforeEntry));
		}
	}

	FGameXXKRouteCardEntry* InvalidSupport = FindEntryByOrdinal(State.CardRun.RouteCardEntries, FirstQuestNpcOrdinal);
	if (!TestNotNull(TEXT("the invalid-source fixture retains support ordinal thirteen"), InvalidSupport))
	{
		return false;
	}
	InvalidSupport->SourceKind = EGameXXKRouteCardSourceKind::HeroBase;
	const FGameXXKRuntimeState InvalidBefore = State;
	Error.Reset();
	TestFalse(TEXT("an unexpected support-slot source rejects NPC replacement"),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, TEXT("Npc.YueBai"), {}, &Error));
	TestTrue(TEXT("invalid support replacement reports a concrete error"), !Error.IsEmpty());
	TestTrue(TEXT("invalid support replacement is fully atomic"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &InvalidBefore, PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCanonicalRouteLocalCleanupTest,
	"GameXXK.Integration.CardRoute.Lifecycle.CanonicalEntryCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCanonicalRouteLocalCleanupTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteCardCanonicalIntegrationTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("the cleanup fixture enters a canonical route"), StartAcceptedRoute(State, 0x68AC, false)))
	{
		return false;
	}
	const TArray<FGameXXKRouteCardEntry> EntriesBefore = State.CardRun.RouteCardEntries;
	const int32 NextBefore = State.CardRun.NextRouteCardEntryOrdinal;
	FGameXXKCardBattleAdapter::ClearActiveCardBattle(State);
	TestTrue(TEXT("active-battle cleanup preserves route-card entries"), EntriesEqual(State.CardRun.RouteCardEntries, EntriesBefore));
	TestEqual(TEXT("active-battle cleanup preserves the next entry ordinal"), State.CardRun.NextRouteCardEntryOrdinal, NextBefore);

	FGameXXKRuntimeState DirectCleanup = State;
	FGameXXKCardBattleAdapter::ClearRouteLocalCardState(DirectCleanup);
	TestTrue(TEXT("route-local cleanup removes stable entries"), DirectCleanup.CardRun.RouteCardEntries.IsEmpty());
	TestEqual(TEXT("route-local cleanup resets the stable-entry sequence"), DirectCleanup.CardRun.NextRouteCardEntryOrdinal, 0);

	TestTrue(TEXT("route failure settles and returns to town"), UGameXXKMVPRules::FailDungeonToTown(State));
	TestTrue(TEXT("terminal route failure removes stable entries"), State.CardRun.RouteCardEntries.IsEmpty());
	TestEqual(TEXT("terminal route failure resets the stable-entry sequence"), State.CardRun.NextRouteCardEntryOrdinal, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCanonicalThreeChapterEntryLifecycleTest,
	"GameXXK.Route.ThreeChapter.CanonicalEntryLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCanonicalThreeChapterEntryLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteCardCanonicalIntegrationTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("the chapter fixture enters a canonical three-chapter route"), StartAcceptedRoute(State, 0x79BD, false)))
	{
		return false;
	}
	FString Error;
	TestTrue(TEXT("the chapter fixture appends one acquired entry"),
		AppendAcquiredEntry(State.CardRun, EGameXXKCardQuality::Rare, &Error));
	State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 1;
	const TArray<FGameXXKRouteCardEntry> EntriesBefore = State.CardRun.RouteCardEntries;
	const int32 NextBefore = State.CardRun.NextRouteCardEntryOrdinal;

	TestTrue(TEXT("chapter one boss clear advances"), UGameXXKMVPRules::ResolveBossClear(State));
	TestTrue(TEXT("chapter one transition preserves all stable entries"), EntriesEqual(State.CardRun.RouteCardEntries, EntriesBefore));
	TestEqual(TEXT("chapter one transition preserves the next entry ordinal"), State.CardRun.NextRouteCardEntryOrdinal, NextBefore);
	TestTrue(TEXT("chapter two boss clear advances"), UGameXXKMVPRules::ResolveBossClear(State));
	TestTrue(TEXT("chapter two transition preserves all stable entries"), EntriesEqual(State.CardRun.RouteCardEntries, EntriesBefore));
	TestEqual(TEXT("chapter two transition preserves the next entry ordinal"), State.CardRun.NextRouteCardEntryOrdinal, NextBefore);

	TestTrue(TEXT("chapter three boss clear settles the route"), UGameXXKMVPRules::ResolveBossClear(State));
	TestTrue(TEXT("terminal chapter cleanup removes stable entries"), State.CardRun.RouteCardEntries.IsEmpty());
	TestEqual(TEXT("terminal chapter cleanup resets the stable-entry sequence"), State.CardRun.NextRouteCardEntryOrdinal, 0);
	return true;
}

#endif
