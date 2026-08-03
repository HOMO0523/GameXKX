#include "GameXXKMVPRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKBattlePresentation.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Components/InputComponent.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleUnitVisualWidget.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "UI/GameXXKMainMenuWidget.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UI/GameXXKTownHudWidget.h"
#include "UI/GameXXKTownOverlayWidget.h"
#include "UI/GameXXKWorldMapWidget.h"
#include "Town/GameXXKTownNpcActor.h"
#include "Town/GameXXKTownPlayerPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * Resolves a current-hand card through CardCheck rather than assuming a
	 * legacy command-menu action exists.  The fixed route seed below makes this
	 * deterministic, while the preview keeps the assertion coupled to the
	 * actual legal-target contract.
	 */
	bool FindManualEnemyTargetCardInCurrentHand(
		const FGameXXKRuntimeState& State,
		const FName ExcludedCardInstanceId,
		FName& OutCardInstanceId,
		FName& OutEnemyUnitId,
		FString& OutError)
	{
		OutCardInstanceId = NAME_None;
		OutEnemyUnitId = NAME_None;
		OutError.Reset();
		if (!State.CardRun.bHasActiveCardBattle)
		{
			OutError = TEXT("The player-flow fixture did not enter a card battle.");
			return false;
		}

		for (const FGameXXKCardInstance& CardInstance : State.CardRun.ActiveBattle.Deck.Hand)
		{
			if (CardInstance.InstanceId == ExcludedCardInstanceId)
			{
				continue;
			}

			FGameXXKCardPlayPreview Preview;
			FString PreviewError;
			if (!FGameXXKCardBattleAdapter::BuildCardPlayPreview(State, CardInstance.InstanceId, Preview, &PreviewError)
				|| !Preview.bCanPlay
				|| !Preview.TargetRequest.bRequiresManualSelection)
			{
				continue;
			}

			const FGameXXKCardTargetCandidateView* EnemyCandidate = Preview.TargetRequest.CandidateViews.FindByPredicate([](const FGameXXKCardTargetCandidateView& Candidate)
			{
				return Candidate.bCanSelect && Candidate.Side == EGameXXKCardTargetSide::Enemy && !Candidate.UnitId.IsNone();
			});
			if (EnemyCandidate)
			{
				OutCardInstanceId = CardInstance.InstanceId;
				OutEnemyUnitId = EnemyCandidate->UnitId;
				return true;
			}
		}

		OutError = TEXT("No affordable manual enemy-target card was available in the current hand.");
		return false;
	}
}

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
	TestNotNull(TEXT("player controller owns world map widget"), PlayerController->GetWorldMapWidgetForTest());
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
	const FGameXXKCompanionRosterState& StarterRoster = Subsystem->GetRuntimeState().CardRun.CompanionRoster;
	TestEqual(TEXT("StartNewGame grants exactly one permanent companion for the player flow"), StarterRoster.PermanentCompanions.Num(), 1);
	FName StarterCompanionId = NAME_None;
	if (StarterRoster.PermanentCompanions.Num() == 1)
	{
		const FGameXXKPermanentCompanion& StarterCompanion = StarterRoster.PermanentCompanions[0];
		StarterCompanionId = StarterCompanion.InstanceId;
		TestFalse(TEXT("StartNewGame gives the player flow companion a stable instance id"), StarterCompanionId.IsNone());
		TestTrue(TEXT("StartNewGame activates the starter companion for the route"), StarterCompanion.bIsActive);
		TestEqual(TEXT("StartNewGame synchronizes the active companion selection"),
			Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId,
			StarterCompanionId);
	}
	if (StarterCompanionId.IsNone())
	{
		return false;
	}
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("world map is visible after new game"), PlayerController->GetWorldMapWidgetForTest()->IsWorldMapVisibleForTest());
	TestFalse(TEXT("town overlay stays hidden before a town is selected"), PlayerController->GetTownOverlayWidgetForTest()->IsTownOverlayVisible());
	TestTrue(TEXT("controller-routed Qingshan click enters town"), PlayerController->GetWorldMapWidgetForTest()->TrySelectRegion(UGameXXKMVPRules::RegionQingshan()));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("main menu hides after town state"), PlayerController->GetMainMenuWidgetForTest()->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("town overlay appears after town state"), PlayerController->GetTownOverlayWidgetForTest()->IsTownOverlayVisible());
	TestFalse(TEXT("route map hidden before entering dungeon"), PlayerController->GetRouteMapWidgetForTest()->GetVisibility() == ESlateVisibility::Visible);
	UGameXXKTownHudWidget* TownHud = PlayerController->GetTownHudWidgetForTest();
	TestNotNull(TEXT("player controller owns town HUD for companion codex"), TownHud);
	TestTrue(TEXT("town HUD opens companion codex for Escape test"), TownHud && TownHud->OpenCompanionCodexForTest());
	TestTrue(TEXT("companion codex is visible before Escape"), TownHud && TownHud->IsCompanionCodexOpenForTest());
	TestTrue(TEXT("Escape closes the open companion codex"), PlayerController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape, IE_Pressed, 1.0f)));
	TestFalse(TEXT("Escape hides companion codex without leaving town"), TownHud && TownHud->IsCompanionCodexOpenForTest());
	TestEqual(TEXT("Escape leaves player in town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
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
	// Keep the generated route and its opening hand deterministic.  Seed 2 has
	// two independently playable enemy-target cards after the explicit task NPC
	// is selected, which lets this full flow cover both commit and cancellation.
	Subsystem->GetMutableRuntimeState().RouteSeed = 2;
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestFalse(TEXT("player-facing town overlay does not expose a route map button"), PlayerController->GetTownOverlayWidgetForTest()->HasTownActionForTest(FName(TEXT("EnterDungeon")), false));
	TestTrue(TEXT("town exit interaction opens route map"), Subsystem->OpenDungeonFromTownExit());
	FString TaskNpcError;
	TestTrue(FString::Printf(TEXT("an explicit task NPC joins the route before the player-flow battle test: %s"), *TaskNpcError),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(
			Subsystem->GetMutableRuntimeState(),
			TEXT("Npc.TusiChief"),
			{},
			&TaskNpcError));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("route map screen after town exit F interaction"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("route map widget visible after entering dungeon"), PlayerController->GetRouteMapWidgetForTest()->GetVisibility(), ESlateVisibility::Visible);

	TestTrue(TEXT("route map start node button executes"), PlayerController->GetRouteMapWidgetForTest()->ExecuteRouteNode(0));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	UGameXXKOneGameRouteMapWidget* const RouteWidgetBeforeBattle = PlayerController->GetRouteMapWidgetForTest();
	TestNotNull(TEXT("player flow retains a route widget before battle entry"), RouteWidgetBeforeBattle);
	if (!RouteWidgetBeforeBattle)
	{
		return false;
	}
	RouteWidgetBeforeBattle->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RouteWidgetBeforeBattle->RestoreScrollOffset(123.0f);
	const float RouteScrollBeforeBattle = RouteWidgetBeforeBattle->GetCurrentScrollOffset();
	PlayerController->bShowMouseCursor = false;
	PlayerController->bEnableClickEvents = false;
	PlayerController->bEnableMouseOverEvents = true;
	PlayerController->ResetIgnoreMoveInput();
	PlayerController->ResetIgnoreLookInput();
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);
	PlayerController->SetTrackedInputModeForTest(EGameXXKTrackedInputMode::GameOnly);
	TestFalse(TEXT("quest dialog is closed before battle entry"), PlayerController->IsQuestDialogOpenForTest());
	TestFalse(TEXT("companion roster is closed before battle entry"), PlayerController->IsCompanionRosterOpenForTest());
	TestFalse(TEXT("task panel is closed before battle entry"), PlayerController->IsTaskPanelOpenForTest());
	TestFalse(TEXT("route encounter panel is closed before battle entry"), PlayerController->IsRouteEncounterPanelOpenForTest());
	TestFalse(TEXT("route merchant input lock is inactive before battle entry"), PlayerController->IsRouteMerchantInputLockedForTest());
	TestTrue(TEXT("pre-existing move ignore is active before battle entry"), PlayerController->IsMoveInputIgnored());
	TestTrue(TEXT("pre-existing look ignore is active before battle entry"), PlayerController->IsLookInputIgnored());
	TestTrue(TEXT("route map battle node button executes"), PlayerController->GetRouteMapWidgetForTest()->ExecuteRouteNode(1));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("battle screen after route node button"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("battle board visible after route node button"), PlayerController->GetBattleBoardWidgetForTest()->IsBattleBoardVisible());
	TestTrue(TEXT("battle coordinator is active after route node entry"), PlayerController->IsBattleOverlayActive());
	TestTrue(TEXT("battle entry retains the exact route widget object"), PlayerController->GetRouteMapWidgetForTest() == RouteWidgetBeforeBattle);
	TestEqual(TEXT("battle entry collapses the retained route widget"), RouteWidgetBeforeBattle->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("battle entry applies tracked UI-only input"), PlayerController->GetTrackedInputModeForTest(), EGameXXKTrackedInputMode::UIOnly);
	TestTrue(TEXT("battle entry leaves a pre-existing move ignore active"), PlayerController->IsMoveInputIgnored());
	TestTrue(TEXT("battle entry leaves a pre-existing look ignore active"), PlayerController->IsLookInputIgnored());
	const FGameXXKRuntimeState& BattleState = Subsystem->GetRuntimeState();
	const FGameXXKCardBattleRuntime& BattleRuntime = BattleState.CardRun.ActiveBattle;
	TestEqual(TEXT("the Qingshan task battle keeps the fixed three-member party"), BattleState.ActiveBattleParty.Num(), 3);
	const FGameXXKCardCombatUnit* StarterCompanionUnit = BattleRuntime.Units.FindByPredicate([StarterCompanionId](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == StarterCompanionId && Unit.Side == EGameXXKCardTargetSide::Party;
	});
	const FGameXXKCardCombatUnit* HeroUnit = BattleRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player") && Unit.Side == EGameXXKCardTargetSide::Party && Unit.Role == EGameXXKCharacterRole::Hero;
	});
	const FGameXXKCardCombatUnit* TaskNpcUnit = BattleRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Npc.TusiChief") && Unit.Side == EGameXXKCardTargetSide::Party && Unit.Role == EGameXXKCharacterRole::QuestNpc;
	});
	TestTrue(TEXT("the Qingshan task battle includes the active permanent companion"), StarterCompanionUnit != nullptr);
	TestTrue(TEXT("the Qingshan task battle includes the fixed hero"), HeroUnit != nullptr);
	TestTrue(TEXT("the Qingshan task battle includes the route-local Tusi Chief"), TaskNpcUnit != nullptr);
	if (!StarterCompanionUnit || !HeroUnit || !TaskNpcUnit)
	{
		return false;
	}
	TestEqual(TEXT("the active permanent companion is presented at 我 1P"), FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, StarterCompanionUnit->UnitId), 1);
	TestEqual(TEXT("the hero is presented at the central 我 2P"), FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, HeroUnit->UnitId), 2);
	TestEqual(TEXT("the task NPC is presented at 我 3P"), FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, TaskNpcUnit->UnitId), 3);

	const TArray<FGameXXKBattlePresentationSlot> PresentationSlots = FGameXXKBattlePresentation::BuildSlots(BattleRuntime);
	TestTrue(TEXT("the presentation slot list carries the companion 1P identity"), PresentationSlots.ContainsByPredicate([StarterCompanionId](const FGameXXKBattlePresentationSlot& Slot)
	{
		return Slot.UnitId == StarterCompanionId && Slot.Side == EGameXXKCardTargetSide::Party && Slot.SlotNumber == 1;
	}));
	TestTrue(TEXT("the presentation slot list carries the hero 2P identity"), PresentationSlots.ContainsByPredicate([](const FGameXXKBattlePresentationSlot& Slot)
	{
		return Slot.UnitId == TEXT("Player") && Slot.Side == EGameXXKCardTargetSide::Party && Slot.SlotNumber == 2;
	}));
	TestTrue(TEXT("the presentation slot list carries the task NPC 3P identity"), PresentationSlots.ContainsByPredicate([](const FGameXXKBattlePresentationSlot& Slot)
	{
		return Slot.UnitId == TEXT("Npc.TusiChief") && Slot.Side == EGameXXKCardTargetSide::Party && Slot.SlotNumber == 3;
	}));
	TestTrue(TEXT("the battle exposes a current enemy intent for presentation"), !BattleState.CardRun.EnemyIntents.IsEmpty());
	if (!BattleState.CardRun.EnemyIntents.IsEmpty())
	{
		const FGameXXKCardEnemyIntent& FirstIntent = BattleState.CardRun.EnemyIntents[0];
		TestEqual(TEXT("the first enemy intent uses its source presentation slot"),
			FirstIntent.SourceSlotNumber,
			FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, FirstIntent.SourceUnitId));
		TestEqual(TEXT("the first enemy intent uses its target presentation slot"),
			FirstIntent.TargetSlotNumber,
			FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, FirstIntent.SuggestedTargetUnitId));
	}

	FName EnemyCardInstanceId;
	FName EnemyTargetUnitId;
	FString CardFixtureError;
	TestTrue(FString::Printf(TEXT("the full player flow exposes a current-hand enemy target card: %s"), *CardFixtureError),
		FindManualEnemyTargetCardInCurrentHand(
			Subsystem->GetRuntimeState(),
			NAME_None,
			EnemyCardInstanceId,
			EnemyTargetUnitId,
			CardFixtureError));
	if (EnemyCardInstanceId.IsNone() || EnemyTargetUnitId.IsNone())
	{
		return false;
	}

	const FGameXXKCardInstance* EnemyCardInstance = Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.FindByPredicate([EnemyCardInstanceId](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == EnemyCardInstanceId;
	});
	TestNotNull(TEXT("the selected card retains a stable owner unit id"), EnemyCardInstance);
	if (!EnemyCardInstance || EnemyCardInstance->OwnerUnitId.IsNone())
	{
		return false;
	}

	const FGameXXKBattleRuntimeUnit* EnemyRuntimeUnit = Subsystem->GetRuntimeState().ActiveBattleEnemies.FindByPredicate([EnemyTargetUnitId](const FGameXXKBattleRuntimeUnit& Unit)
	{
		return Unit.Id == EnemyTargetUnitId;
	});
	TestNotNull(TEXT("the legal card target resolves to a stable battle enemy"), EnemyRuntimeUnit);
	if (!EnemyRuntimeUnit)
	{
		return false;
	}

	const int32 EnemyHPBeforeCardCommit = EnemyRuntimeUnit->HP;
	UGameXXKBattleBoardWidget* BattleBoard = PlayerController->GetBattleBoardWidgetForTest();
	TestNotNull(TEXT("player controller retains its battle board for card interaction"), BattleBoard);
	if (!BattleBoard)
	{
		return false;
	}
	const FVector2D CardOwnerProjection(940.0f, 420.0f);
	const FVector2D TargetingPointer(640.0f, 360.0f);
	BattleBoard->RegisterBattleUnitScreenPosition(EnemyCardInstance->OwnerUnitId, CardOwnerProjection);
	const UGameXXKBattleUnitVisualWidget* const CardOwnerVisual =
		BattleBoard->GetUnitVisualForTest(EnemyCardInstance->OwnerUnitId);
	TestNotNull(TEXT("the controller-owned battle session keeps the card owner's fixed-slot visual"),
		CardOwnerVisual);
	TestTrue(TEXT("clicking the current-hand card enters card targeting"), BattleBoard->ClickCardInHand(EnemyCardInstanceId));
	TestTrue(TEXT("card targeting is active after a legal hand-card click"), BattleBoard->IsCardTargetingActive());
	TestEqual(TEXT("card arrow starts at the owner's fixed stage center, never legacy actor projection"),
		BattleBoard->GetTargetingSourcePositionForTest(),
		CardOwnerVisual ? CardOwnerVisual->GetStageCenter() : FVector2D::ZeroVector);
	TestTrue(TEXT("the legal enemy receives the current card-target highlight"), BattleBoard->IsTargetUnitHighlighted(EnemyTargetUnitId));
	TestFalse(TEXT("the card owner is not highlighted as an enemy target"), BattleBoard->IsTargetUnitHighlighted(EnemyCardInstance->OwnerUnitId));
	TestTrue(TEXT("controller forwards mouse movement to the active card arrow"), PlayerController->UpdateBattleTargetingPointerForTest(TargetingPointer));
	TestEqual(TEXT("active card arrow follows the controller pointer"), BattleBoard->GetTargetingPointerPositionForTest(), TargetingPointer);
	TestTrue(TEXT("controller confirms the highlighted enemy by stable UnitId without a scene actor"), PlayerController->ConfirmBattleTargetForUnitId(EnemyTargetUnitId));
	TestFalse(TEXT("committing a card target exits card targeting"), BattleBoard->IsCardTargetingActive());
	const FGameXXKBattleRuntimeUnit* EnemyAfterCardCommit = Subsystem->GetRuntimeState().ActiveBattleEnemies.FindByPredicate([EnemyTargetUnitId](const FGameXXKBattleRuntimeUnit& Unit)
	{
		return Unit.Id == EnemyTargetUnitId;
	});
	TestTrue(TEXT("the confirmed enemy-target card updates the battle projection"), EnemyAfterCardCommit && EnemyAfterCardCommit->HP < EnemyHPBeforeCardCommit);
	TestTrue(TEXT("controller-confirmed damage locks later card input until presentation drain"),
		BattleBoard->IsBattlePresentationLockedForTest());
	TestFalse(TEXT("the locked Board rejects another hand-card click before presentation drain"),
		BattleBoard->ClickCardInHand(EnemyCardInstanceId));
	BattleBoard->AdvanceVisualsAtRealTime(0.0);
	BattleBoard->AdvanceVisualsAtRealTime(100.0);
	TestFalse(TEXT("the controller-owned Board unlocks after its complete presentation drains"),
		BattleBoard->IsBattlePresentationLockedForTest());

	FName CancelCardInstanceId;
	FName CancelTargetUnitId;
	CardFixtureError.Reset();
	TestTrue(FString::Printf(TEXT("a second current-hand enemy target card remains available for cancellation: %s"), *CardFixtureError),
		FindManualEnemyTargetCardInCurrentHand(
			Subsystem->GetRuntimeState(),
			EnemyCardInstanceId,
			CancelCardInstanceId,
			CancelTargetUnitId,
			CardFixtureError));
	if (CancelCardInstanceId.IsNone() || CancelTargetUnitId.IsNone())
	{
		return false;
	}
	TestTrue(TEXT("a second current-hand card enters targeting for the cancel flow"), BattleBoard->ClickCardInHand(CancelCardInstanceId));
	TestTrue(TEXT("the second card highlights its legal enemy before cancellation"), BattleBoard->IsTargetUnitHighlighted(CancelTargetUnitId));
	TestTrue(TEXT("controller cancels battle targeting"), PlayerController->CancelBattleTargetingForTest());
	TestFalse(TEXT("controller cancel exits card targeting"), BattleBoard->IsCardTargetingActive());
	TestFalse(TEXT("controller cancel clears the current card target highlight"), BattleBoard->IsTargetUnitHighlighted(CancelTargetUnitId));
	TestTrue(TEXT("battle interactions leave a pre-existing move ignore active"), PlayerController->IsMoveInputIgnored());
	TestTrue(TEXT("battle interactions leave a pre-existing look ignore active"), PlayerController->IsLookInputIgnored());

	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::DungeonMap;
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestFalse(TEXT("leaving battle deactivates the coordinator"), PlayerController->IsBattleOverlayActive());
	TestTrue(TEXT("leaving battle restores the exact route widget object"), PlayerController->GetRouteMapWidgetForTest() == RouteWidgetBeforeBattle);
	TestEqual(TEXT("dungeon-map refresh becomes authoritative after overlay restoration"), RouteWidgetBeforeBattle->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("leaving battle restores exact route scroll"), RouteWidgetBeforeBattle->GetCurrentScrollOffset(), RouteScrollBeforeBattle);
	TestFalse(TEXT("leaving battle restores the hidden cursor"), PlayerController->bShowMouseCursor);
	TestFalse(TEXT("leaving battle restores disabled click events"), PlayerController->bEnableClickEvents);
	TestTrue(TEXT("leaving battle restores enabled mouse-over events"), PlayerController->bEnableMouseOverEvents);
	TestEqual(TEXT("leaving battle restores exact tracked input mode"), PlayerController->GetTrackedInputModeForTest(), EGameXXKTrackedInputMode::GameOnly);
	TestTrue(TEXT("pre-existing move ignore remains after overlay exit"), PlayerController->IsMoveInputIgnored());
	TestTrue(TEXT("pre-existing look ignore remains after overlay exit"), PlayerController->IsLookInputIgnored());
	PlayerController->SetIgnoreMoveInput(false);
	PlayerController->SetIgnoreLookInput(false);
	TestFalse(TEXT("battle entry did not stack an extra move-input ignore"), PlayerController->IsMoveInputIgnored());
	TestFalse(TEXT("battle entry did not stack an extra look-input ignore"), PlayerController->IsLookInputIgnored());

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
