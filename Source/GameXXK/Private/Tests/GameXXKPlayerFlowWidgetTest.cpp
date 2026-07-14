#include "GameXXKMVPRules.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Components/InputComponent.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "UI/GameXXKMainMenuWidget.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UI/GameXXKTownOverlayWidget.h"
#include "Town/GameXXKTownNpcActor.h"
#include "Town/GameXXKTownPlayerPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPlayerControllerTownExitInventoryCleanupTest,
	"GameXXK.MVP.UI.PlayerControllerTownExitInventoryCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPlayerControllerTownExitInventoryCleanupTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);

	TestTrue(TEXT("controller creates the inventory window for town-exit cleanup"), PlayerController->EnsurePlayerFlowWidgetsForTest());
	TestTrue(TEXT("town-exit cleanup starts a game"), Subsystem->StartGame());
	TestTrue(TEXT("town-exit cleanup enters Qingshan"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("town-exit cleanup accepts the prerequisite quest"), Subsystem->AcceptQuest());
	PlayerController->RefreshPlayerFlowWidgetsForTest();

	TestTrue(TEXT("merchant inventory is open before the town exit"), PlayerController->OpenMerchantTradeWindow());
	TestTrue(TEXT("merchant inventory owns modal input before the town exit"), PlayerController->IsInventoryWindowModalInputLockedForTest());
	TestTrue(TEXT("town exit transitions to the route map"), Subsystem->OpenDungeonFromTownExit());
	PlayerController->RefreshPlayerFlowWidgetsForTest();

	TestEqual(TEXT("town exit clears the inventory window mode"), PlayerController->GetInventoryWindowWidgetForTest()->GetWindowModeForTest(), EGameXXKInventoryWindowMode::None);
	TestFalse(TEXT("town exit releases the inventory modal input lock"), PlayerController->IsInventoryWindowModalInputLockedForTest());
	TestFalse(TEXT("town exit hides the inventory window"), PlayerController->GetInventoryWindowWidgetForTest()->IsWindowVisibleForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPlayerControllerRouteClickDispatchTest,
	"GameXXK.MVP.UI.PlayerControllerRouteClickDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPlayerControllerRouteClickDispatchTest::RunTest(const FString& Parameters)
{
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	// The editor-context test owns no world, so seed the controller with an
	// unregistered input component before invoking the normal setup hook.
	PlayerController->InputComponent = NewObject<UInputComponent>(PlayerController, TEXT("RouteClickDispatchInputComponent"));
	PlayerController->SetupInputComponent();
	TestNotNull(TEXT("controller owns an input component after setup"), PlayerController->InputComponent.Get());

	int32 LeftMouseReleaseBindingCount = 0;
	if (PlayerController->InputComponent)
	{
		for (const FInputKeyBinding& Binding : PlayerController->InputComponent->KeyBindings)
		{
			if (Binding.Chord.Key == EKeys::LeftMouseButton && Binding.KeyEvent == IE_Released)
			{
				++LeftMouseReleaseBindingCount;
			}
		}
	}

	TestEqual(TEXT("route clicks are dispatched exclusively by InputKey, without a duplicate raw release binding"), LeftMouseReleaseBindingCount, 0);
	return true;
}

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
	TestNotNull(TEXT("player controller owns independent inventory window"), PlayerController->GetInventoryWindowWidgetForTest());
	TestNotNull(TEXT("player controller owns quest dialogue widget"), PlayerController->GetQuestDialogWidgetForTest());
	TestFalse(TEXT("quest dialogue starts closed"), PlayerController->IsQuestDialogOpenForTest());
	TestTrue(TEXT("controller can open an empty preview quest dialogue for visual test"), PlayerController->OpenQuestDialogPreviewForTest());
	TestTrue(TEXT("quest dialogue reports open after preview command"), PlayerController->IsQuestDialogOpenForTest());
	TestTrue(TEXT("quest dialogue is modal while it is visible"), PlayerController->IsQuestDialogModalInputLockedForTest());
	TestTrue(TEXT("controller cancel closes quest dialogue"), PlayerController->CloseQuestDialog());
	TestFalse(TEXT("quest dialogue reports closed after cancel"), PlayerController->IsQuestDialogOpenForTest());
	TestFalse(TEXT("quest dialogue restores town input after cancel"), PlayerController->IsQuestDialogModalInputLockedForTest());
	TestTrue(TEXT("player controller route map escapes the old fixed small viewport"), PlayerController->GetRouteMapWidgetForTest()->GetRouteContentSizeForTest().X >= 1000.0f);
	const FGameViewportWidgetSlot RouteViewportSlot = UGameViewportSubsystem::Get()->GetWidgetSlot(PlayerController->GetRouteMapWidgetForTest());
	TestEqual(TEXT("route map viewport slot anchors left edge"), RouteViewportSlot.Anchors.Minimum.X, 0.0);
	TestEqual(TEXT("route map viewport slot anchors top edge"), RouteViewportSlot.Anchors.Minimum.Y, 0.0);
	TestEqual(TEXT("route map viewport slot anchors right edge"), RouteViewportSlot.Anchors.Maximum.X, 1.0);
	TestEqual(TEXT("route map viewport slot anchors bottom edge"), RouteViewportSlot.Anchors.Maximum.Y, 1.0);
	TestEqual(TEXT("route map viewport slot has no left offset"), RouteViewportSlot.Offsets.Left, 0.0f);
	TestEqual(TEXT("route map viewport slot has no top offset"), RouteViewportSlot.Offsets.Top, 0.0f);
	TestEqual(TEXT("route map viewport slot has no right size override"), RouteViewportSlot.Offsets.Right, 0.0f);
	TestEqual(TEXT("route map viewport slot has no bottom size override"), RouteViewportSlot.Offsets.Bottom, 0.0f);
	const FGameViewportWidgetSlot InventoryViewportSlot = UGameViewportSubsystem::Get()->GetWidgetSlot(PlayerController->GetInventoryWindowWidgetForTest());
	TestEqual(TEXT("inventory window viewport slot anchors left edge"), InventoryViewportSlot.Anchors.Minimum.X, 0.0);
	TestEqual(TEXT("inventory window viewport slot anchors top edge"), InventoryViewportSlot.Anchors.Minimum.Y, 0.0);
	TestEqual(TEXT("inventory window viewport slot anchors right edge"), InventoryViewportSlot.Anchors.Maximum.X, 1.0);
	TestEqual(TEXT("inventory window viewport slot anchors bottom edge"), InventoryViewportSlot.Anchors.Maximum.Y, 1.0);
	TestEqual(TEXT("inventory window viewport slot has no left offset"), InventoryViewportSlot.Offsets.Left, 0.0f);
	TestEqual(TEXT("inventory window viewport slot has no top offset"), InventoryViewportSlot.Offsets.Top, 0.0f);
	TestEqual(TEXT("inventory window viewport slot has no right size override"), InventoryViewportSlot.Offsets.Right, 0.0f);
	TestEqual(TEXT("inventory window viewport slot has no bottom size override"), InventoryViewportSlot.Offsets.Bottom, 0.0f);

	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("main menu visible on initial main menu state"), PlayerController->GetMainMenuWidgetForTest()->GetVisibility(), ESlateVisibility::Visible);
	TestFalse(TEXT("town overlay hidden on initial main menu state"), PlayerController->GetTownOverlayWidgetForTest()->IsTownOverlayVisible());

	TestTrue(TEXT("start game opens world map for player controller flow"), Subsystem->StartGame());
	TestTrue(TEXT("select Qingshan for player controller flow"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("main menu hides after town state"), PlayerController->GetMainMenuWidgetForTest()->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("town overlay appears after town state"), PlayerController->GetTownOverlayWidgetForTest()->IsTownOverlayVisible());
	TestFalse(TEXT("route map hidden before entering dungeon"), PlayerController->GetRouteMapWidgetForTest()->GetVisibility() == ESlateVisibility::Visible);
	AGameXXKTownNpcActor* QuestNpc = NewObject<AGameXXKTownNpcActor>();
	AGameXXKTownPlayerPawn* QuestPawn = NewObject<AGameXXKTownPlayerPawn>();
	QuestNpc->SetNpcRole(EGameXXKTownNpcRole::Quest);
	QuestNpc->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("F quest path opens a dialogue before it mutates quest state"), PlayerController->OpenQuestDialogForNpc(QuestNpc, QuestPawn));
	TestEqual(TEXT("opening dialogue does not accept quest yet"), Subsystem->GetRuntimeState().QuestState, EGameXXKQuestState::NotAccepted);
	TestTrue(TEXT("accept button resolves the original quest NPC path"), PlayerController->AcceptQuestDialog());
	TestEqual(TEXT("accept button marks quest accepted"), Subsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Accepted);
	TestTrue(TEXT("accept button preserves follower activation"), QuestNpc->IsFollowerActive());
	TestTrue(TEXT("accept button preserves follower target"), QuestNpc->GetFollowTarget() == QuestPawn);
	TestTrue(TEXT("I key opens the independent free inventory window"), PlayerController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::I, IE_Pressed, 1.0f)));
	TestEqual(TEXT("I key records free inventory window mode"), PlayerController->GetInventoryWindowWidgetForTest()->GetWindowModeForTest(), EGameXXKInventoryWindowMode::FreeInventory);
	TestFalse(TEXT("I key free inventory keeps movement input unlocked"), PlayerController->IsInventoryWindowModalInputLockedForTest());
	TestEqual(TEXT("I key does not open the legacy town inventory panel"), Subsystem->GetRuntimeState().TownPanelMode, EGameXXKTownPanelMode::None);
	TestTrue(TEXT("I key closes the independent free inventory window"), PlayerController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::I, IE_Pressed, 1.0f)));
	TestEqual(TEXT("I key clears independent inventory window mode"), PlayerController->GetInventoryWindowWidgetForTest()->GetWindowModeForTest(), EGameXXKInventoryWindowMode::None);
	TestTrue(TEXT("merchant path opens independent trade inventory window"), PlayerController->OpenMerchantTradeWindow());
	TestEqual(TEXT("merchant path records trade inventory window mode"), PlayerController->GetInventoryWindowWidgetForTest()->GetWindowModeForTest(), EGameXXKInventoryWindowMode::MerchantTrade);
	TestTrue(TEXT("merchant trade inventory window locks movement input"), PlayerController->IsInventoryWindowModalInputLockedForTest());
	TestTrue(TEXT("merchant trade inventory window exposes its own close button"), PlayerController->GetInventoryWindowWidgetForTest()->HasCloseButtonForTest());
	PlayerController->GetInventoryWindowWidgetForTest()->SetVisibility(ESlateVisibility::Collapsed);
	TestTrue(TEXT("merchant F reopens a stale hidden trade window instead of closing it"), PlayerController->OpenMerchantTradeWindow());
	TestEqual(TEXT("stale hidden merchant reopen keeps trade mode"), PlayerController->GetInventoryWindowWidgetForTest()->GetWindowModeForTest(), EGameXXKInventoryWindowMode::MerchantTrade);
	TestNotEqual(TEXT("stale hidden merchant reopen makes the window visible"), PlayerController->GetInventoryWindowWidgetForTest()->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("repeated merchant interaction closes trade inventory window"), PlayerController->OpenMerchantTradeWindow());
	TestEqual(TEXT("repeated merchant interaction clears trade inventory window mode"), PlayerController->GetInventoryWindowWidgetForTest()->GetWindowModeForTest(), EGameXXKInventoryWindowMode::None);
	TestFalse(TEXT("repeated merchant interaction restores movement input"), PlayerController->IsInventoryWindowModalInputLockedForTest());
	TestTrue(TEXT("merchant path can reopen trade inventory window after toggle close"), PlayerController->OpenMerchantTradeWindow());
	TestTrue(TEXT("merchant trade inventory window closes independently"), PlayerController->CloseInventoryWindow());
	TestEqual(TEXT("merchant trade close clears inventory window mode"), PlayerController->GetInventoryWindowWidgetForTest()->GetWindowModeForTest(), EGameXXKInventoryWindowMode::None);

	TestEqual(TEXT("quest dialogue acceptance enables the in-world town route entrance"), Subsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Accepted);
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

	AGameXXKBattleSceneUnitActor* PartyActor = NewObject<AGameXXKBattleSceneUnitActor>();
	PartyActor->SetMVPSubsystemForTest(Subsystem);
	PartyActor->ConfigureFromRuntimeUnit(false, 0, Subsystem->GetRuntimeState().ActiveBattleParty[0]);
	AGameXXKBattleSceneUnitActor* FollowerActor = NewObject<AGameXXKBattleSceneUnitActor>();
	FollowerActor->SetMVPSubsystemForTest(Subsystem);
	FollowerActor->ConfigureFromRuntimeUnit(false, 1, Subsystem->GetRuntimeState().ActiveBattleParty[1]);
	AGameXXKBattleSceneUnitActor* EnemyActor = NewObject<AGameXXKBattleSceneUnitActor>();
	EnemyActor->SetMVPSubsystemForTest(Subsystem);
	EnemyActor->ConfigureFromRuntimeUnit(true, 0, Subsystem->GetRuntimeState().ActiveBattleEnemies[0]);
	const int32 EnemyHPBeforeControllerInput = Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP;
	const FVector2D EnemySideClickPosition(360.0f, 390.0f);
	const FVector2D PartyActorScreenPosition(940.0f, 420.0f);
	TestTrue(TEXT("controller toggles command menu for party actor"), PlayerController->ToggleBattleCommandMenuForUnitForTest(PartyActor, EnemySideClickPosition, PartyActorScreenPosition));
	TestTrue(TEXT("battle board menu is visible after controller opens it"), PlayerController->GetBattleBoardWidgetForTest()->IsCommandMenuVisibleForTest());
	TestEqual(TEXT("controller selects first party actor for commands"), PlayerController->GetBattleBoardWidgetForTest()->GetSelectedPartyIndexForTest(), 0);
	const FVector2D ControllerCommandMenuAnchor = PlayerController->GetBattleBoardWidgetForTest()->GetCommandMenuAnchorForTest();
	const FVector2D ControllerCommandMenuDefaultOffset(-500.0f, 0.0f);
	TestEqual(TEXT("controller command menu uses the tuned X offset"), ControllerCommandMenuAnchor.X, PartyActorScreenPosition.X + ControllerCommandMenuDefaultOffset.X);
	TestEqual(TEXT("controller command menu uses the tuned Y offset"), ControllerCommandMenuAnchor.Y, PartyActorScreenPosition.Y + ControllerCommandMenuDefaultOffset.Y);
	TestTrue(TEXT("controller toggles the same party command menu closed"), PlayerController->ToggleBattleCommandMenuForUnitForTest(PartyActor, EnemySideClickPosition, PartyActorScreenPosition));
	TestFalse(TEXT("same party toggle hides command menu"), PlayerController->GetBattleBoardWidgetForTest()->IsCommandMenuVisibleForTest());
	TestTrue(TEXT("controller reopens command menu for party actor"), PlayerController->ToggleBattleCommandMenuForUnitForTest(PartyActor, FVector2D(260.0f, 640.0f), FVector2D(830.0f, 420.0f)));
	TestTrue(TEXT("controller switches command menu to follower actor"), PlayerController->ToggleBattleCommandMenuForUnitForTest(FollowerActor, FVector2D(820.0f, 620.0f), FVector2D(910.0f, 500.0f)));
	TestEqual(TEXT("controller selects follower actor for commands"), PlayerController->GetBattleBoardWidgetForTest()->GetSelectedPartyIndexForTest(), 1);
	TestFalse(TEXT("controller refuses enemy actor as command source"), PlayerController->ToggleBattleCommandMenuForUnitForTest(EnemyActor, FVector2D(300.0f, 320.0f), FVector2D(300.0f, 320.0f)));
	TestEqual(TEXT("right-click enemy path no longer damages enemy"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP, EnemyHPBeforeControllerInput);
	TestTrue(TEXT("attack command enters targeting through battle board"), PlayerController->GetBattleBoardWidgetForTest()->ExecuteBasicAttackAction());
	TestTrue(TEXT("controller updates targeting pointer from mouse position"), PlayerController->UpdateBattleTargetingPointerForTest(FVector2D(640.0f, 360.0f)));
	TestEqual(TEXT("targeting arrow end follows controller mouse position"), PlayerController->GetBattleBoardWidgetForTest()->GetTargetingPointerPositionForTest(), FVector2D(640.0f, 360.0f));
	TestTrue(TEXT("controller confirms enemy target"), PlayerController->ConfirmBattleTargetForUnitForTest(EnemyActor));
	TestTrue(TEXT("confirmed target takes damage"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP < EnemyHPBeforeControllerInput);
	TestTrue(TEXT("controller opens command menu again for cancel flow"), PlayerController->OpenBattleCommandMenuForUnitForTest(PartyActor, FVector2D(900.0f, 520.0f), FVector2D(830.0f, 420.0f)));
	TestTrue(TEXT("second attack command enters targeting"), PlayerController->GetBattleBoardWidgetForTest()->ExecuteBasicAttackAction());
	TestTrue(TEXT("controller cancels battle targeting"), PlayerController->CancelBattleTargetingForTest());
	TestFalse(TEXT("controller cancel exits targeting"), PlayerController->GetBattleBoardWidgetForTest()->IsTargetingBattleActionForTest());

	FGameXXKRuntimeState& EncounterState = Subsystem->GetMutableRuntimeState();
	EncounterState = UGameXXKMVPRules::CreateNewGame();
	EncounterState.Screen = EGameXXKScreen::DungeonMap;
	EncounterState.CurrentMapId = TEXT("HuangshanRoute");
	EncounterState.QuestState = EGameXXKQuestState::Accepted;
	EncounterState.bDungeonActive = true;
	EncounterState.bHasGeneratedRouteMap = true;
	EncounterState.RouteMapNodes.Add(FGameXXKRouteMapNode{0, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.5f, 0.0f), TArray<int32>{1}});
	EncounterState.RouteMapNodes.Add(FGameXXKRouteMapNode{1, 1, 0, EGameXXKNodeKind::Event, FVector2D(0.5f, 0.5f), TArray<int32>{2}});
	EncounterState.RouteMapNodes.Add(FGameXXKRouteMapNode{2, 2, 0, EGameXXKNodeKind::Boss, FVector2D(0.5f, 1.0f), TArray<int32>{}});
	EncounterState.RouteMapEdges.Add(FGameXXKRouteMapEdge{0, 1});
	EncounterState.RouteMapEdges.Add(FGameXXKRouteMapEdge{1, 2});
	EncounterState.VisitedRouteNodeIds.Add(0);
	EncounterState.ReachableRouteNodeIds.Add(1);
	TestTrue(TEXT("test event route node enters route event screen"), UGameXXKMVPRules::SelectRouteNodeById(EncounterState, 1));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("route event screen active"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteEvent);
	TestEqual(TEXT("route event scene hides the old player-facing encounter overlay"), PlayerController->GetMainMenuWidgetForTest()->GetVisibility(), ESlateVisibility::Collapsed);

	return true;
}

#endif
