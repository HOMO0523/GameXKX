#include "Misc/AutomationTest.h"

#include "GameXXKEnemyTypes.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRelicRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "Engine/GameInstance.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKRuntimeState MakeStartedState()
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		return Subsystem && Subsystem->StartGame()
			? Subsystem->GetRuntimeStateCopy()
			: FGameXXKRuntimeState();
	}

	TArray<uint8> BuildRouteSignature(const FGameXXKRuntimeState& State)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		bool bGenerated = State.bHasGeneratedRouteMap;
		int32 RouteSeed = State.RouteSeed;
		int32 CurrentNode = State.CurrentRouteNodeId;
		int32 PendingNode = State.PendingRouteNodeId;
		Writer << bGenerated;
		Writer << RouteSeed;
		Writer << CurrentNode;
		Writer << PendingNode;
		int32 NodeCount = State.RouteMapNodes.Num();
		Writer << NodeCount;
		for (const FGameXXKRouteMapNode& Node : State.RouteMapNodes)
		{
			FGameXXKRouteMapNode Copy = Node;
			FGameXXKRouteMapNode::StaticStruct()->SerializeItem(Writer, &Copy, nullptr);
		}
		int32 EdgeCount = State.RouteMapEdges.Num();
		Writer << EdgeCount;
		for (const FGameXXKRouteMapEdge& Edge : State.RouteMapEdges)
		{
			FGameXXKRouteMapEdge Copy = Edge;
			FGameXXKRouteMapEdge::StaticStruct()->SerializeItem(Writer, &Copy, nullptr);
		}
		return Bytes;
	}

	FGameXXKRuntimeState MakeActiveLegacyRoute()
	{
		FGameXXKRuntimeState State = MakeStartedState();
		State.PlayerLevel = 11;
		State.bDungeonActive = true;
		UGameXXKMVPRules::GenerateRouteMapForSeed(State, 27011991);
		State.CardRun.RouteRandomSeed = State.RouteSeed;
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKThreeChapterRouteSchemaTest,
	"GameXXK.Route.ThreeChapter.Schema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKThreeChapterRouteSchemaTest::RunTest(const FString& Parameters)
{
	const FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TestEqual(TEXT("inactive chapter"), State.CardRun.RouteProgress.CurrentChapter, 0);
	TestEqual(TEXT("inactive combat level"), State.CardRun.RouteProgress.RouteCombatLevel, 0);
	TestTrue(TEXT("no enemy state"), State.CardRun.ActiveBattle.EnemyStates.IsEmpty());
	TestEqual(TEXT("no route-card acquisition count"), State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, 0);

	FGameXXKSaveState InvalidRouteProgress = UGameXXKMVPRules::MakeSaveState(State);
	InvalidRouteProgress.RuntimeState.CardRun.RouteProgress.SchemaVersion = 1;
	InvalidRouteProgress.RuntimeState.CardRun.RouteProgress.CurrentChapter = 1;
	InvalidRouteProgress.RuntimeState.CardRun.RouteProgress.RouteCombatLevel = 1;
	InvalidRouteProgress.RuntimeState.CardRun.RouteProgress.ChapterSeeds = { 0, 0, 0 };
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	TestFalse(TEXT("current save rejects an incomplete three-chapter route snapshot"),
		FGameXXKSaveMigration::MigrateToCurrent(InvalidRouteProgress, Migrated, Report));

	FGameXXKSaveState InvalidEnemyState = UGameXXKMVPRules::MakeSaveState(State);
	InvalidEnemyState.RuntimeState.CardRun.ActiveBattle.EnemyStates.Add(NAME_None, FGameXXKEnemyBattleState());
	TestFalse(TEXT("current save rejects an unnamed enemy state key"),
		FGameXXKSaveMigration::MigrateToCurrent(InvalidEnemyState, Migrated, Report));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKThreeChapterVersionMigrationTest,
	"GameXXK.MVP.SaveGame.ThreeChapterVersionMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKThreeChapterVersionMigrationTest::RunTest(const FString& Parameters)
{
	const FGameXXKRuntimeState ActiveLegacyRoute = MakeActiveLegacyRoute();
	const TArray<uint8> SourceRouteSignature = BuildRouteSignature(ActiveLegacyRoute);
	FGameXXKSaveState Legacy = UGameXXKMVPRules::MakeSaveState(ActiveLegacyRoute);
	Legacy.SaveVersion = FGameXXKSaveMigration::ThreeChapterRouteIntroducedSaveVersion - 1;

	FGameXXKSaveState MigratedSave;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("dispatcher migrates dependency save"),
		FGameXXKSaveMigration::MigrateToCurrent(Legacy, MigratedSave, Report));
	TestEqual(TEXT("dynamic report target"), Report.TargetVersion, UGameXXKMVPRules::GetCurrentSaveVersion());
	TestEqual(TEXT("old active route becomes chapter one"), MigratedSave.RuntimeState.CardRun.RouteProgress.CurrentChapter, 1);
	TestEqual(TEXT("snapshot uses saved level"),
		MigratedSave.RuntimeState.CardRun.RouteProgress.RouteCombatLevel,
		ActiveLegacyRoute.PlayerLevel);
	TestEqual(TEXT("root seed is the saved route seed"),
		MigratedSave.RuntimeState.CardRun.RouteProgress.RootSeed,
		ActiveLegacyRoute.RouteSeed);
	TestEqual(TEXT("existing map remains byte-stable"),
		BuildRouteSignature(MigratedSave.RuntimeState), SourceRouteSignature);

	FGameXXKRuntimeState Restored;
	FGameXXKSaveMigrationReport RestoreReport;
	TestTrue(TEXT("restore facade uses the unified dispatcher"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(Legacy, Restored, RestoreReport));
	TestTrue(TEXT("restore facade produces the dispatcher state"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Restored, &MigratedSave.RuntimeState, PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKThreeChapterLifecycleTest,
	"GameXXK.Route.ThreeChapter.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKThreeChapterLifecycleTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState EntryState = UGameXXKMVPRules::CreateNewGame();
	EntryState.Screen = EGameXXKScreen::Town;
	EntryState.CurrentRegion = UGameXXKMVPRules::RegionQingshan();
	EntryState.QuestState = EGameXXKQuestState::Accepted;
	EntryState.PlayerLevel = 7;
	TestTrue(TEXT("accepted Qingshan quest enters the three-chapter route"), UGameXXKMVPRules::EnterDungeon(EntryState));
	const FGameXXKRouteProgress& EntryProgress = EntryState.CardRun.RouteProgress;
	TestEqual(TEXT("route entry writes the three-chapter schema"), EntryProgress.SchemaVersion, 1);
	TestEqual(TEXT("route entry starts at chapter one"), EntryProgress.CurrentChapter, 1);
	TestEqual(TEXT("route entry snapshots the hero level once"), EntryProgress.RouteCombatLevel, 7);
	TestEqual(TEXT("route entry persists three deterministic chapter seeds"), EntryProgress.ChapterSeeds.Num(), 3);
	if (EntryProgress.ChapterSeeds.Num() != 3)
	{
		return false;
	}
	TestEqual(TEXT("chapter one seed is exactly the route root seed"), EntryProgress.ChapterSeeds[0], EntryProgress.RootSeed);
	TestTrue(TEXT("chapter two has a distinct deterministic seed"), EntryProgress.ChapterSeeds[1] != EntryProgress.ChapterSeeds[0]);
	TestTrue(TEXT("chapter three has a distinct deterministic seed"), EntryProgress.ChapterSeeds[2] != EntryProgress.ChapterSeeds[0] && EntryProgress.ChapterSeeds[2] != EntryProgress.ChapterSeeds[1]);
	EntryState.PlayerLevel = 20;
	TestEqual(TEXT("later player levels do not mutate the route combat snapshot"), EntryState.CardRun.RouteProgress.RouteCombatLevel, 7);

	FGameXXKRuntimeState FirstBossState = EntryState;
	FirstBossState.PlayerHP = 1;
	FirstBossState.PlayerMP = 0;
	FirstBossState.CardRun.ActiveTemporaryQuestNpcId = TEXT("Npc.TusiChief");
	FirstBossState.CardRun.PartySelection.QuestNpc.NpcId = TEXT("Npc.TusiChief");
	FirstBossState.CardRun.RouteTravelMoney = 321;
	FirstBossState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 4;
	FString RelicError;
	TestTrue(TEXT("chapter-transition fixture owns a route relic"),
		FGameXXKRelicRules::AcquireRelic(FirstBossState, TEXT("Relic.AncientCoin"), &RelicError));
	FirstBossState.CardRun.RouteMerchant.SourceNodeId = 0;
	FirstBossState.CardRun.RouteMerchant.OfferSeed = 0x612345;
	FirstBossState.CardRun.RouteMerchant.RefreshCount = 2;
	FGameXXKRouteMerchantOffer MerchantSentinel;
	MerchantSentinel.OfferId = TEXT("Merchant.ChapterOne.Sentinel");
	MerchantSentinel.Kind = EGameXXKRouteMerchantOfferKind::Relic;
	MerchantSentinel.ContentId = TEXT("Relic.JadeBell");
	MerchantSentinel.Quality = EGameXXKCardQuality::Common;
	MerchantSentinel.Price = 30;
	FirstBossState.CardRun.RouteMerchant.Offers.Add(MerchantSentinel);
	const TArray<FGameXXKRelicInstance> RelicsBeforeChapterAdvance = FirstBossState.CardRun.Relics;
	const int32 NextRelicOrdinalBeforeChapterAdvance = FirstBossState.CardRun.NextRelicAcquisitionOrdinal;
	const int32 TravelMoneyBeforeChapterAdvance = FirstBossState.CardRun.RouteTravelMoney;
	const bool bEconomyInitializedBeforeChapterAdvance = FirstBossState.CardRun.bRouteEconomyInitialized;
	const int32 AcquisitionCountBeforeChapterAdvance = FirstBossState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount;
	const int32 ChapterTwoSeed = FirstBossState.CardRun.RouteProgress.ChapterSeeds[1];
	TestTrue(TEXT("chapter one Boss clear advances the active route"), UGameXXKMVPRules::ResolveBossClear(FirstBossState));
	TestTrue(TEXT("chapter one clear keeps the run active"), FirstBossState.bDungeonActive);
	TestEqual(TEXT("chapter one clear advances exactly to chapter two"), FirstBossState.CardRun.RouteProgress.CurrentChapter, 2);
	TestEqual(TEXT("chapter two regenerates from its saved chapter seed"), FirstBossState.RouteSeed, ChapterTwoSeed);
	TestEqual(TEXT("chapter two returns to the unchanged route-map screen"), FirstBossState.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("chapter two has a freshly generated existing topology"), FirstBossState.bHasGeneratedRouteMap && !FirstBossState.RouteMapNodes.IsEmpty());
	TestEqual(TEXT("chapter transitions fully restore hero health"), FirstBossState.PlayerHP, FirstBossState.PlayerMaxHP);
	TestEqual(TEXT("chapter transitions fully restore hero MP"), FirstBossState.PlayerMP, FirstBossState.PlayerMaxMP);
	TestEqual(TEXT("chapter transitions preserve the active temporary task NPC"), FirstBossState.CardRun.ActiveTemporaryQuestNpcId, FName(TEXT("Npc.TusiChief")));
	const FGameXXKRouteMerchantState EmptyMerchant;
	TestTrue(TEXT("chapter transitions clear the prior chapter's merchant snapshot before node IDs are reused"),
		FGameXXKRouteMerchantState::StaticStruct()->CompareScriptStruct(
			&FirstBossState.CardRun.RouteMerchant,
			&EmptyMerchant,
			PPF_None));
	TestEqual(TEXT("chapter transitions preserve the route relic count"),
		FirstBossState.CardRun.Relics.Num(), RelicsBeforeChapterAdvance.Num());
	if (FirstBossState.CardRun.Relics.Num() == RelicsBeforeChapterAdvance.Num())
	{
		for (int32 Index = 0; Index < RelicsBeforeChapterAdvance.Num(); ++Index)
		{
			TestTrue(*FString::Printf(TEXT("chapter transitions preserve route relic %d"), Index),
				FGameXXKRelicInstance::StaticStruct()->CompareScriptStruct(
					&FirstBossState.CardRun.Relics[Index],
					&RelicsBeforeChapterAdvance[Index],
					PPF_None));
		}
	}
	TestEqual(TEXT("chapter transitions preserve the next relic ordinal"),
		FirstBossState.CardRun.NextRelicAcquisitionOrdinal, NextRelicOrdinalBeforeChapterAdvance);
	TestEqual(TEXT("chapter transitions preserve route-economy initialization"),
		FirstBossState.CardRun.bRouteEconomyInitialized, bEconomyInitializedBeforeChapterAdvance);
	TestEqual(TEXT("chapter transitions preserve route travel money"),
		FirstBossState.CardRun.RouteTravelMoney, TravelMoneyBeforeChapterAdvance);
	TestEqual(TEXT("chapter transitions preserve route card acquisition history"),
		FirstBossState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, AcquisitionCountBeforeChapterAdvance);

	const int32 ChapterThreeSeed = FirstBossState.CardRun.RouteProgress.ChapterSeeds[2];
	TestTrue(TEXT("chapter two Boss clear advances the active route"), UGameXXKMVPRules::ResolveBossClear(FirstBossState));
	TestTrue(TEXT("chapter two clear keeps the run active"), FirstBossState.bDungeonActive);
	TestEqual(TEXT("chapter two clear advances exactly to chapter three"), FirstBossState.CardRun.RouteProgress.CurrentChapter, 3);
	TestEqual(TEXT("chapter three regenerates from its saved chapter seed"), FirstBossState.RouteSeed, ChapterThreeSeed);

	FGameXXKRuntimeState InvalidProgressState = EntryState;
	InvalidProgressState.CardRun.RouteProgress.CurrentChapter = 0;
	const FGameXXKRuntimeState InvalidProgressBefore = InvalidProgressState;
	TestFalse(TEXT("a malformed active route cannot advance chapters"), UGameXXKMVPRules::ResolveBossClear(InvalidProgressState));
	TestTrue(TEXT("rejected chapter transition preserves the full state"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&InvalidProgressState, &InvalidProgressBefore, PPF_None));

	TestTrue(TEXT("chapter three Boss clear remains the terminal route outcome"), UGameXXKMVPRules::ResolveBossClear(FirstBossState));
	TestFalse(TEXT("chapter three terminal clear ends the route"), FirstBossState.bDungeonActive);
	TestEqual(TEXT("chapter three terminal clear resolves the quest"), FirstBossState.QuestState, EGameXXKQuestState::Completed);
	return true;
}

#endif
