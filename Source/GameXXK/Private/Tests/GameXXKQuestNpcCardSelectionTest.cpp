#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKMVPRules.h"

#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<FName> BuildExpectedQuestNpcSelection(
		const FGameXXKQuestNpcDefinition& Definition,
		const int32 RouteSeed)
	{
		TArray<FName> Candidates = Definition.FixedCardIds;
		Candidates.Sort([](const FName Left, const FName Right)
		{
			return Left.ToString() < Right.ToString();
		});
		if (Candidates.Num() == 4)
		{
			const uint32 StableNpcSalt = FCrc::StrCrc32(*Definition.NpcId.ToString());
			const int32 OmittedIndex = static_cast<int32>(
				(static_cast<uint32>(RouteSeed) ^ StableNpcSalt) % 4U);
			Candidates.RemoveAt(OmittedIndex, 1, EAllowShrinking::No);
		}
		return Candidates;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQuestNpcCardSelectionTest,
	"GameXXK.Data.QuestNpcCardSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQuestNpcCardSelectionTest::RunTest(const FString& Parameters)
{
	bool bAllSelectionsValid = true;
	bool bAllNpcsReachEveryOmission = true;
	bool bAllOmissionsUniform = true;
	bool bAllSelectionsReplayExactly = true;

	for (const FGameXXKQuestNpcDefinition& Definition : FGameXXKCompanionCatalog::GetQuestNpcDefinitions())
	{
		TMap<FName, int32> OmissionCounts;
		for (const FName CandidateCardId : Definition.FixedCardIds)
		{
			OmissionCounts.Add(CandidateCardId, 0);
		}

		for (int32 RouteSeed = 1; RouteSeed <= 256; ++RouteSeed)
		{
			FGameXXKRuntimeState First = UGameXXKMVPRules::CreateNewGame();
			First.RouteSeed = RouteSeed;
			First.CardRun.RouteRandomSeed = 0;
			FString Error;
			if (!FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(
				First,
				Definition.NpcId,
				{},
				&Error))
			{
				AddError(FString::Printf(
					TEXT("seeded task-NPC selection failed (%s seed=%d): %s"),
					*Definition.NpcId.ToString(),
					RouteSeed,
					*Error));
				return false;
			}

			const TArray<FName>& Selected = First.CardRun.PartySelection.QuestNpc.SelectedCardIds;
			TSet<FName> UniqueSelected(Selected);
			TArray<FName> Omitted;
			for (const FName CandidateCardId : Definition.FixedCardIds)
			{
				if (!Selected.Contains(CandidateCardId))
				{
					Omitted.Add(CandidateCardId);
				}
			}
			bAllSelectionsValid &= Selected.Num() == 3
				&& UniqueSelected.Num() == 3
				&& Omitted.Num() == 1
				&& Selected == BuildExpectedQuestNpcSelection(Definition, RouteSeed);
			if (Omitted.Num() == 1)
			{
				++OmissionCounts.FindChecked(Omitted[0]);
			}

			FGameXXKRuntimeState Replay = UGameXXKMVPRules::CreateNewGame();
			Replay.RouteSeed = RouteSeed;
			Replay.CardRun.RouteRandomSeed = 0;
			bAllSelectionsReplayExactly &= FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(
				Replay,
				Definition.NpcId,
				{},
				nullptr)
				&& Replay.CardRun.PartySelection.QuestNpc.SelectedCardIds == Selected;
		}

		int32 MinimumOmissionCount = MAX_int32;
		int32 MaximumOmissionCount = 0;
		for (const TPair<FName, int32>& Pair : OmissionCounts)
		{
			MinimumOmissionCount = FMath::Min(MinimumOmissionCount, Pair.Value);
			MaximumOmissionCount = FMath::Max(MaximumOmissionCount, Pair.Value);
		}
		bAllNpcsReachEveryOmission &= MinimumOmissionCount > 0;
		bAllOmissionsUniform &= MinimumOmissionCount == 64 && MaximumOmissionCount == 64;
	}

	TestTrue(TEXT("all six task NPCs select exactly three valid cards using the route seed contract"), bAllSelectionsValid);
	TestTrue(TEXT("the same route seed and NPC identity replay the same saved three-card selection"), bAllSelectionsReplayExactly);
	TestTrue(TEXT("every one of each NPC's four cards can be the omitted route card"), bAllNpcsReachEveryOmission);
	TestTrue(TEXT("the four missing-card combinations are exactly uniform across a complete 256-seed cycle"), bAllOmissionsUniform);

	const FGameXXKQuestNpcDefinition* TusiChief =
		FGameXXKCompanionCatalog::FindQuestNpcDefinition(TEXT("Npc.TusiChief"));
	TestNotNull(TEXT("the route persistence fixture resolves the Tusi chief"), TusiChief);
	if (!TusiChief)
	{
		return false;
	}

	int32 RouteSeedWithNonDefaultSelection = 1;
	while (RouteSeedWithNonDefaultSelection < 256
		&& BuildExpectedQuestNpcSelection(*TusiChief, RouteSeedWithNonDefaultSelection)
			== TusiChief->DefaultRouteCardIds)
	{
		++RouteSeedWithNonDefaultSelection;
	}

	FGameXXKRuntimeState RouteState = UGameXXKMVPRules::CreateNewGame();
	bool bReachedTown = UGameXXKMVPRules::OpenWorldMap(RouteState)
		&& UGameXXKMVPRules::EnterWorldRegion(RouteState, UGameXXKMVPRules::RegionQingshan())
		&& UGameXXKMVPRules::AcceptTownQuest(RouteState);
	RouteState.RouteSeed = RouteSeedWithNonDefaultSelection;
	FString RouteError;
	bReachedTown &= FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(
		RouteState,
		TusiChief->NpcId,
		{},
		&RouteError);
	TestTrue(FString::Printf(TEXT("the task-NPC persistence fixture reaches town: %s"), *RouteError), bReachedTown);
	TestTrue(FString::Printf(TEXT("the selected task NPC enters the seeded route: %s"), *RouteError),
		bReachedTown && UGameXXKMVPRules::EnterDungeon(RouteState));

	const TArray<FName> ExpectedRouteSelection =
		BuildExpectedQuestNpcSelection(*TusiChief, RouteState.CardRun.RouteProgress.RootSeed);
	TestEqual(TEXT("route entry persists the exact seed-selected NPC cards"),
		RouteState.CardRun.PartySelection.QuestNpc.SelectedCardIds,
		ExpectedRouteSelection);

	TArray<FName> QuestNpcRouteEntryCardIds;
	for (const FGameXXKRouteCardEntry& Entry : RouteState.CardRun.RouteCardEntries)
	{
		if (Entry.SourceKind == EGameXXKRouteCardSourceKind::QuestNpcBase)
		{
			QuestNpcRouteEntryCardIds.Add(Entry.CardId);
		}
	}
	TestEqual(TEXT("the route recipe contains exactly the selected three NPC cards and never the omitted fourth"),
		QuestNpcRouteEntryCardIds,
		ExpectedRouteSelection);

	const TArray<FName> PersistedSelection = RouteState.CardRun.PartySelection.QuestNpc.SelectedCardIds;
	FGameXXKRuntimeState ReloadedState = RouteState;
	TestTrue(TEXT("card-run validation accepts the persisted seeded NPC selection after reload"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(ReloadedState, &RouteError));
	TestEqual(TEXT("reload never rerolls the persisted NPC cards"),
		ReloadedState.CardRun.PartySelection.QuestNpc.SelectedCardIds,
		PersistedSelection);

	return true;
}

#endif
