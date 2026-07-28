#include "GameXXKMVPRules.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKRouteEncounterSceneActor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKRuntimeState BuildPendingEncounterState(EGameXXKNodeKind NodeKind, EGameXXKScreen Screen)
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::DungeonMap;
		State.CurrentMapId = TEXT("HuangshanRoute");
		State.QuestState = EGameXXKQuestState::Accepted;
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{0, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.5f, 0.0f), TArray<int32>{1}});
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{1, 1, 0, NodeKind, FVector2D(0.5f, 0.5f), TArray<int32>{2}});
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{2, 2, 0, EGameXXKNodeKind::Boss, FVector2D(0.5f, 1.0f), TArray<int32>{}});
		State.RouteMapEdges.Add(FGameXXKRouteMapEdge{0, 1});
		State.RouteMapEdges.Add(FGameXXKRouteMapEdge{1, 2});
		State.VisitedRouteNodeIds.Add(0);
		State.ReachableRouteNodeIds.Add(1);
		UGameXXKMVPRules::SelectRouteNodeById(State, 1);
		State.Screen = Screen;
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterSceneActorTest,
	"GameXXK.MVP.RouteEncounter.SceneActorInteraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterSceneActorTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();

	UGameXXKMVPSubsystem* EventSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	EventSubsystem->GetMutableRuntimeState() = BuildPendingEncounterState(EGameXXKNodeKind::Event, EGameXXKScreen::RouteEvent);
	// A generated event can deterministically invite either a task NPC or an event-only NPC.
	// This case exercises the latter: event-only NiuHuan keeps the normal gold resolution.
	EventSubsystem->GetMutableRuntimeState().CardRun.PendingEvent.SourceNodeId = 1;
	EventSubsystem->GetMutableRuntimeState().CardRun.PendingEvent.EventNpcId = TEXT("Npc.Event.NiuHuan");
	AGameXXKRouteEncounterSceneActor* EventActor = NewObject<AGameXXKRouteEncounterSceneActor>();
	EventActor->SetMVPSubsystemForTest(EventSubsystem);
	EventActor->SetEncounterScreenForTest(EGameXXKScreen::RouteEvent);
	const int32 GoldBeforeEvent = EventSubsystem->GetRuntimeState().PlayerGold;
	TestFalse(TEXT("the route event actor never resolves an event before a player chooses in the encounter panel"), EventActor->ApplyDefaultInteraction(nullptr));
	TestFalse(TEXT("a controller-less actor does not report a completed encounter"), EventActor->WasLastInteractionSuccessful());
	TestEqual(TEXT("opening an event interaction leaves the route event active"), EventSubsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteEvent);
	TestEqual(TEXT("opening an event interaction grants no automatic gold"), EventSubsystem->GetRuntimeState().PlayerGold, GoldBeforeEvent);
	TestFalse(TEXT("opening an event interaction does not visit the pending node"), EventSubsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(1));

	UGameXXKMVPSubsystem* SupportSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	SupportSubsystem->GetMutableRuntimeState() = BuildPendingEncounterState(EGameXXKNodeKind::Event, EGameXXKScreen::RouteEvent);
	SupportSubsystem->GetMutableRuntimeState().CardRun.PendingEvent.SourceNodeId = 1;
	SupportSubsystem->GetMutableRuntimeState().CardRun.PendingEvent.EventNpcId = TEXT("Npc.YueBai");
	AGameXXKRouteEncounterSceneActor* SupportActor = NewObject<AGameXXKRouteEncounterSceneActor>();
	SupportActor->SetMVPSubsystemForTest(SupportSubsystem);
	SupportActor->SetEncounterScreenForTest(EGameXXKScreen::RouteEvent);
	const int32 GoldBeforeSupport = SupportSubsystem->GetRuntimeState().PlayerGold;
	TestFalse(TEXT("opening a named task-NPC event cannot silently accept temporary support"), SupportActor->ApplyDefaultInteraction(nullptr));
	TestTrue(TEXT("opening a named task-NPC event leaves the temporary support slot empty"), SupportSubsystem->GetRuntimeState().CardRun.ActiveTemporaryQuestNpcId.IsNone());
	TestEqual(TEXT("opening a named task-NPC event does not silently grant gold"), SupportSubsystem->GetRuntimeState().PlayerGold, GoldBeforeSupport);
	TestEqual(TEXT("opening a named task-NPC event keeps the route event screen active"), SupportSubsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteEvent);

	UGameXXKMVPSubsystem* OccupiedSupportSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	OccupiedSupportSubsystem->GetMutableRuntimeState() = BuildPendingEncounterState(EGameXXKNodeKind::Event, EGameXXKScreen::RouteEvent);
	OccupiedSupportSubsystem->GetMutableRuntimeState().CardRun.PendingEvent.SourceNodeId = 1;
	OccupiedSupportSubsystem->GetMutableRuntimeState().CardRun.PendingEvent.EventNpcId = TEXT("Npc.YueBai");
	OccupiedSupportSubsystem->GetMutableRuntimeState().CardRun.ActiveTemporaryQuestNpcId = TEXT("Npc.TusiChief");
	OccupiedSupportSubsystem->GetMutableRuntimeState().CardRun.PartySelection.QuestNpc.NpcId = TEXT("Npc.TusiChief");
	AGameXXKRouteEncounterSceneActor* OccupiedSupportActor = NewObject<AGameXXKRouteEncounterSceneActor>();
	OccupiedSupportActor->SetMVPSubsystemForTest(OccupiedSupportSubsystem);
	OccupiedSupportActor->SetEncounterScreenForTest(EGameXXKScreen::RouteEvent);
	TestFalse(TEXT("a task-NPC event without a presentation controller cannot replace the occupied support slot"), OccupiedSupportActor->ApplyDefaultInteraction(nullptr));
	TestEqual(TEXT("the occupied support remains unchanged after the rejected event"),
		OccupiedSupportSubsystem->GetRuntimeState().CardRun.PartySelection.QuestNpc.NpcId, FName(TEXT("Npc.TusiChief")));

	UGameXXKMVPSubsystem* CampSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	CampSubsystem->GetMutableRuntimeState() = BuildPendingEncounterState(EGameXXKNodeKind::Camp, EGameXXKScreen::RouteCamp);
	CampSubsystem->GetMutableRuntimeState().PlayerHP = 33;
	AGameXXKRouteEncounterSceneActor* CampActor = NewObject<AGameXXKRouteEncounterSceneActor>();
	CampActor->SetMVPSubsystemForTest(CampSubsystem);
	CampActor->SetEncounterScreenForTest(EGameXXKScreen::RouteCamp);
	TestFalse(TEXT("opening camp interaction never heals before the player selects a camp reward"), CampActor->ApplyDefaultInteraction(nullptr));
	TestEqual(TEXT("opening camp interaction leaves player health unchanged"), CampSubsystem->GetRuntimeState().PlayerHP, 33);
	TestEqual(TEXT("opening camp interaction keeps the camp screen active"), CampSubsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteCamp);

	UGameXXKMVPSubsystem* MerchantSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	MerchantSubsystem->GetMutableRuntimeState() = BuildPendingEncounterState(EGameXXKNodeKind::Merchant, EGameXXKScreen::RouteMerchant);
	AGameXXKRouteEncounterSceneActor* MerchantActor = NewObject<AGameXXKRouteEncounterSceneActor>();
	MerchantActor->SetMVPSubsystemForTest(MerchantSubsystem);
	MerchantActor->SetEncounterScreenForTest(EGameXXKScreen::RouteMerchant);
	TestFalse(TEXT("opening merchant interaction never completes the node before an explicit leave choice"), MerchantActor->ApplyDefaultInteraction(nullptr));
	TestEqual(TEXT("opening merchant interaction keeps the merchant screen active"), MerchantSubsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteMerchant);

	UGameXXKMVPSubsystem* MismatchSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	MismatchSubsystem->GetMutableRuntimeState() = BuildPendingEncounterState(EGameXXKNodeKind::Event, EGameXXKScreen::RouteEvent);
	AGameXXKRouteEncounterSceneActor* MismatchActor = NewObject<AGameXXKRouteEncounterSceneActor>();
	MismatchActor->SetMVPSubsystemForTest(MismatchSubsystem);
	MismatchActor->SetEncounterScreenForTest(EGameXXKScreen::RouteCamp);
	TestFalse(TEXT("mismatched actor does not consume the wrong encounter screen"), MismatchActor->ApplyDefaultInteraction(nullptr));
	TestFalse(TEXT("mismatched actor records failure"), MismatchActor->WasLastInteractionSuccessful());
	TestEqual(TEXT("mismatch leaves event screen active"), MismatchSubsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteEvent);

	return true;
}

#endif
