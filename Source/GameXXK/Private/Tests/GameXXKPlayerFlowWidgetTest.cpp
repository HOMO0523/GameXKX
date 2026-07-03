#include "GameXXKMVPRules.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKMainMenuWidget.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UI/GameXXKTownOverlayWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTownOverlayCommandsTest,
	"GameXXK.MVP.UI.TownOverlayCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTownOverlayCommandsTest::RunTest(const FString& Parameters)
{
	const FString TestSlotName = UGameXXKMVPSubsystem::GetManualSaveSlotName(0);
	UGameplayStatics::DeleteGameInSlot(TestSlotName, 0);

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("start game opens world map for town overlay setup"), Subsystem->StartGame());
	TestTrue(TEXT("select Qingshan for town overlay setup"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));

	UGameXXKTownOverlayWidget* TownOverlay = NewObject<UGameXXKTownOverlayWidget>();
	TownOverlay->SetMVPSubsystem(Subsystem);
	TownOverlay->RefreshFromState();

	TestTrue(TEXT("town overlay is visible on town screen"), TownOverlay->IsTownOverlayVisible());
	TestTrue(TEXT("town overlay exposes manual slot save"), TownOverlay->HasTownActionForTest(FName(TEXT("SaveSlot1")), true));
	TestTrue(TEXT("town overlay save writes slot 1"), TownOverlay->SaveToSlotOne());
	TestTrue(TEXT("town overlay slot 1 save exists"), UGameplayStatics::DoesSaveGameExist(TestSlotName, 0));
	TestFalse(TEXT("town overlay hides player-facing route button before F quest acceptance"), TownOverlay->HasTownActionForTest(FName(TEXT("EnterDungeon")), false));
	TestFalse(TEXT("town overlay rejects route map before quest acceptance"), TownOverlay->EnterRouteMap());

	TestTrue(TEXT("F quest acceptance path enables dungeon gate"), Subsystem->AcceptQuest());
	TownOverlay->RefreshFromState();
	TestFalse(TEXT("town overlay keeps route entry as an in-world F entrance after quest acceptance"), TownOverlay->HasTownActionForTest(FName(TEXT("EnterDungeon")), false));
	TestTrue(TEXT("town exit F interaction enters route map after quest acceptance"), Subsystem->OpenDungeonFromTownExit());
	TownOverlay->RefreshFromState();
	TestEqual(TEXT("town exit changes state to dungeon map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("town overlay hides after entering route map"), TownOverlay->IsTownOverlayVisible());

	UGameplayStatics::DeleteGameInSlot(TestSlotName, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPlayerControllerOwnsFlowWidgetsTest,
	"GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPlayerControllerOwnsFlowWidgetsTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);

	TestTrue(TEXT("player controller creates the full player-flow widget set"), PlayerController->EnsurePlayerFlowWidgetsForTest());
	TestNotNull(TEXT("player controller owns main menu widget"), PlayerController->GetMainMenuWidgetForTest());
	TestNotNull(TEXT("player controller owns town overlay widget"), PlayerController->GetTownOverlayWidgetForTest());
	TestNotNull(TEXT("player controller owns route map widget"), PlayerController->GetRouteMapWidgetForTest());
	TestNotNull(TEXT("player controller owns battle board widget"), PlayerController->GetBattleBoardWidgetForTest());

	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("main menu visible on initial main menu state"), PlayerController->GetMainMenuWidgetForTest()->GetVisibility(), ESlateVisibility::Visible);
	TestFalse(TEXT("town overlay hidden on initial main menu state"), PlayerController->GetTownOverlayWidgetForTest()->IsTownOverlayVisible());

	TestTrue(TEXT("start game opens world map for player controller flow"), Subsystem->StartGame());
	TestTrue(TEXT("select Qingshan for player controller flow"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("main menu hides after town state"), PlayerController->GetMainMenuWidgetForTest()->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("town overlay appears after town state"), PlayerController->GetTownOverlayWidgetForTest()->IsTownOverlayVisible());
	TestFalse(TEXT("route map hidden before entering dungeon"), PlayerController->GetRouteMapWidgetForTest()->GetVisibility() == ESlateVisibility::Visible);

	TestTrue(TEXT("quest acceptance enables the in-world town route entrance"), Subsystem->AcceptQuest());
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestFalse(TEXT("player-facing town overlay does not expose a route map button"), PlayerController->GetTownOverlayWidgetForTest()->HasTownActionForTest(FName(TEXT("EnterDungeon")), false));
	TestTrue(TEXT("town exit interaction opens route map"), Subsystem->OpenDungeonFromTownExit());
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("route map screen after town exit F interaction"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("route map widget visible after entering dungeon"), PlayerController->GetRouteMapWidgetForTest()->GetVisibility(), ESlateVisibility::Visible);

	TestTrue(TEXT("route map start node button executes"), PlayerController->GetRouteMapWidgetForTest()->ExecuteRouteNode(0));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("route map battle node button executes"), PlayerController->GetRouteMapWidgetForTest()->ExecuteRouteNode(1));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("battle screen after route node button"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("battle board visible after route node button"), PlayerController->GetBattleBoardWidgetForTest()->IsBattleBoardVisible());

	return true;
}

#endif
