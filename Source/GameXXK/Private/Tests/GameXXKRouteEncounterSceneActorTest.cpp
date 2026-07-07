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
	AGameXXKRouteEncounterSceneActor* EventActor = NewObject<AGameXXKRouteEncounterSceneActor>();
	EventActor->SetMVPSubsystemForTest(EventSubsystem);
	EventActor->SetEncounterScreenForTest(EGameXXKScreen::RouteEvent);
	const int32 GoldBeforeEvent = EventSubsystem->GetRuntimeState().PlayerGold;
	TestTrue(TEXT("F interaction on event actor resolves route event"), EventActor->ApplyDefaultInteraction(nullptr));
	TestTrue(TEXT("event actor records success"), EventActor->WasLastInteractionSuccessful());
	TestEqual(TEXT("event actor returns to route map"), EventSubsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("event actor grants gold reward"), EventSubsystem->GetRuntimeState().PlayerGold, GoldBeforeEvent + 12);
	TestTrue(TEXT("event actor marks pending node visited"), EventSubsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(1));

	UGameXXKMVPSubsystem* CampSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	CampSubsystem->GetMutableRuntimeState() = BuildPendingEncounterState(EGameXXKNodeKind::Camp, EGameXXKScreen::RouteCamp);
	CampSubsystem->GetMutableRuntimeState().PlayerHP = 33;
	AGameXXKRouteEncounterSceneActor* CampActor = NewObject<AGameXXKRouteEncounterSceneActor>();
	CampActor->SetMVPSubsystemForTest(CampSubsystem);
	CampActor->SetEncounterScreenForTest(EGameXXKScreen::RouteCamp);
	TestTrue(TEXT("F interaction on camp actor resolves camp"), CampActor->ApplyDefaultInteraction(nullptr));
	TestEqual(TEXT("camp actor heals player"), CampSubsystem->GetRuntimeState().PlayerHP, CampSubsystem->GetRuntimeState().PlayerMaxHP);
	TestEqual(TEXT("camp actor returns to route map"), CampSubsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);

	UGameXXKMVPSubsystem* MerchantSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	MerchantSubsystem->GetMutableRuntimeState() = BuildPendingEncounterState(EGameXXKNodeKind::Merchant, EGameXXKScreen::RouteMerchant);
	AGameXXKRouteEncounterSceneActor* MerchantActor = NewObject<AGameXXKRouteEncounterSceneActor>();
	MerchantActor->SetMVPSubsystemForTest(MerchantSubsystem);
	MerchantActor->SetEncounterScreenForTest(EGameXXKScreen::RouteMerchant);
	TestTrue(TEXT("F interaction on merchant actor completes merchant node"), MerchantActor->ApplyDefaultInteraction(nullptr));
	TestEqual(TEXT("merchant actor returns to route map"), MerchantSubsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);

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
