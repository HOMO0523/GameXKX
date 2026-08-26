#include "GameXXKMVPRules.h"
#include "GameXXKTrainingRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKBattlePresentation.h"
#include "GameXXKCompanionRules.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/SaveGame.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKBattleUnitVisualWidget.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "UI/GameXXKMainMenuWidget.h"
#include "UI/GameXXKMetaShopWidget.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UI/GameXXKTownHudWidget.h"
#include "UI/GameXXKTownOverlayWidget.h"
#include "UI/GameXXKWorldMapWidget.h"
#include "Town/GameXXKTownNpcActor.h"
#include "Town/GameXXKTownPlayerPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString MakeRouteSettlementWorkbenchWorldPackageName()
	{
		static int32 FixtureSerial = 4100;
		for (;;)
		{
			const FString Candidate = FString::Printf(
				TEXT("/Game/GameXXK/Maps/UEDPIE_%d_L_DesktopTrainingHUD"),
				++FixtureSerial);
			if (!FindPackage(nullptr, *Candidate))
			{
				return Candidate;
			}
		}
	}

	struct FScopedSaveSlotBackup
	{
		FScopedSaveSlotBackup(const FString& InSlotName, int32 InUserIndex)
			: SlotName(InSlotName)
			, UserIndex(InUserIndex)
		{
			bHadExistingSave = UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
			if (bHadExistingSave)
			{
				ExistingSave = UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);
				if (!ExistingSave)
				{
					return;
				}
				ExistingSave->AddToRoot();
			}
			bReady = true;
			UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
		}

		~FScopedSaveSlotBackup()
		{
			if (!bReady)
			{
				return;
			}
			UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
			if (bHadExistingSave && ExistingSave)
			{
				UGameplayStatics::SaveGameToSlot(ExistingSave, SlotName, UserIndex);
				ExistingSave->RemoveFromRoot();
			}
		}

		FString SlotName;
		int32 UserIndex = 0;
		bool bHadExistingSave = false;
		bool bReady = false;
		USaveGame* ExistingSave = nullptr;
	};

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
			if (!EnemyCandidate)
			{
				continue;
			}
			// Some affordable enemy-target cards only apply statuses.  Resolve on a copy and
			// require the target to actually lose health so the commit assertion below never
			// depends on which utility card happens to precede the damage cards in hand.
			const FGameXXKCardCombatUnit* TargetBefore = State.CardRun.ActiveBattle.Units.FindByPredicate([&EnemyCandidate](const FGameXXKCardCombatUnit& Unit)
			{
				return Unit.UnitId == EnemyCandidate->UnitId;
			});
			if (!TargetBefore)
			{
				continue;
			}
			FGameXXKRuntimeState CandidateState = State;
			FGameXXKCardPlayResult PlayResult;
			FString ResolveError;
			if (!FGameXXKCardBattleAdapter::ResolveCardPlay(CandidateState, CardInstance.InstanceId, EnemyCandidate->UnitId, PlayResult, &ResolveError))
			{
				continue;
			}
			const FGameXXKCardCombatUnit* TargetAfter = CandidateState.CardRun.ActiveBattle.Units.FindByPredicate([&EnemyCandidate](const FGameXXKCardCombatUnit& Unit)
			{
				return Unit.UnitId == EnemyCandidate->UnitId;
			});
			if (!TargetAfter || TargetAfter->HP >= TargetBefore->HP)
			{
				continue;
			}
			OutCardInstanceId = CardInstance.InstanceId;
			OutEnemyUnitId = EnemyCandidate->UnitId;
			return true;
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

	TestFalse(TEXT("entering town does not auto-open the meta shop"), PlayerController->IsMetaShopOpenForTest());
	TestFalse(TEXT("entering town does not lock meta shop input"), PlayerController->IsMetaShopInputLockedForTest());

	TestTrue(TEXT("meta shop is open before the town exit"), PlayerController->OpenMetaShopWindow());
	TestTrue(TEXT("meta shop owns modal input before the town exit"), PlayerController->IsMetaShopInputLockedForTest());
	TestTrue(TEXT("town exit transitions to the route map"), Subsystem->OpenDungeonFromTownExit());
	PlayerController->RefreshPlayerFlowWidgetsForTest();

	TestEqual(TEXT("town exit clears the inventory window mode"), PlayerController->GetInventoryWindowWidgetForTest()->GetWindowModeForTest(), EGameXXKInventoryWindowMode::None);
	TestFalse(TEXT("town exit releases the inventory modal input lock"), PlayerController->IsInventoryWindowModalInputLockedForTest());
	TestFalse(TEXT("town exit hides the inventory window"), PlayerController->GetInventoryWindowWidgetForTest()->IsWindowVisibleForTest());
	TestFalse(TEXT("town exit closes the meta shop"), PlayerController->IsMetaShopOpenForTest());
	TestFalse(TEXT("town exit releases the meta shop modal input lock"), PlayerController->IsMetaShopInputLockedForTest());
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
	FScopedSaveSlotBackup TestSlotBackup(TestSlotName, 0);
	if (!TestTrue(TEXT("town overlay safely isolates the player's manual slot 1"), TestSlotBackup.bReady))
	{
		return false;
	}

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

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingLazyBootTest,
	"GameXXK.MVP.UI.DesktopTrainingLazyBoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingLazyBootTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);
	PlayerController->SetDesktopTrainingBootProfileForTest(true);
	TestTrue(TEXT("HUD-only fixture starts in town"), Subsystem->StartGame());
	TestEqual(TEXT("default desktop performance profile is empty"), PlayerController->GetDesktopTrainingPerfProfileForTest(), FString());

	TestTrue(TEXT("HUD-only ensure creates the workbench"), PlayerController->EnsureDesktopTrainingWidgetsForTest());
	TestNotNull(TEXT("HUD-only controller owns the workbench"), PlayerController->GetDesktopTrainingWorkbenchWidgetForTest());
	TestNull(TEXT("main menu stays lazy"), PlayerController->GetMainMenuWidgetForTest());
	TestNull(TEXT("world map stays lazy"), PlayerController->GetWorldMapWidgetForTest());
	TestNull(TEXT("town overlay stays lazy"), PlayerController->GetTownOverlayWidgetForTest());
	TestNull(TEXT("town HUD stays lazy"), PlayerController->GetTownHudWidgetForTest());
	TestNull(TEXT("route map stays lazy"), PlayerController->GetRouteMapWidgetForTest());
	TestNull(TEXT("battle board stays lazy"), PlayerController->GetBattleBoardWidgetForTest());
	TestNull(TEXT("legacy inventory stays lazy"), PlayerController->GetInventoryWindowWidgetForTest());
	TestNull(TEXT("shop stays lazy"), PlayerController->GetMetaShopWidgetForTest());
	TestNull(TEXT("roster stays lazy"), PlayerController->GetCompanionRosterWidgetForTest());
	TestNull(TEXT("quest dialog stays lazy"), PlayerController->GetQuestDialogWidgetForTest());
	TestNull(TEXT("task panel stays lazy"), PlayerController->GetTaskPanelWidgetForTest());
	TestNull(TEXT("encounter panel stays lazy"), PlayerController->GetRouteEncounterPanelWidgetForTest());
	TestNull(TEXT("merchant stays lazy"), PlayerController->GetRouteMerchantWidgetForTest());
	TestNull(TEXT("relic bar stays lazy"), PlayerController->GetRelicBarWidgetForTest());

	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestNull(TEXT("HUD refresh does not escalate to full creation"), PlayerController->GetMainMenuWidgetForTest());
	PlayerController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape, IE_Pressed, 1.0f));
	TestNull(TEXT("HUD Escape does not escalate to full creation"), PlayerController->GetMainMenuWidgetForTest());

	TestTrue(TEXT("explicit shop request succeeds"), PlayerController->OpenMetaShopWindow());
	TestNotNull(TEXT("explicit request creates the shop"), PlayerController->GetMetaShopWidgetForTest());
	TestNull(TEXT("shop request does not create the route map"), PlayerController->GetRouteMapWidgetForTest());
	TestNull(TEXT("shop request does not create the old inventory"), PlayerController->GetInventoryWindowWidgetForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingTravelPerfProfileTest,
	"GameXXK.MVP.UI.DesktopTrainingTravelPerfProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingTravelPerfProfileTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);
	PlayerController->SetDesktopTrainingBootProfileForTest(true);
	TestTrue(TEXT("travel profile fixture starts in town"), Subsystem->StartGame());
	TestTrue(TEXT("travel profile creates only the workbench"), PlayerController->EnsureDesktopTrainingWidgetsForTest());
	TestTrue(TEXT("travel profile applies"), PlayerController->ApplyDesktopTrainingPerfProfileForTest(TEXT("travel")));

	const FGameXXKTrainingTravelRuntime Runtime = Subsystem->GetTrainingTravelRuntimeCopy();
	TestEqual(TEXT("travel runner enters walking"), Runtime.Phase, EGameXXKTrainingTravelPhase::Walking);
	TestEqual(
		TEXT("travel profile selects cleared 1-1"),
		Runtime.StageId,
		FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1));
	TestNull(TEXT("travel profile keeps legacy route map lazy"), PlayerController->GetRouteMapWidgetForTest());
	TestNull(TEXT("travel profile keeps legacy battle board lazy"), PlayerController->GetBattleBoardWidgetForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingBattlePerfProfileTest,
	"GameXXK.MVP.UI.DesktopTrainingBattlePerfProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingBattlePerfProfileTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);
	PlayerController->SetDesktopTrainingBootProfileForTest(true);
	TestTrue(TEXT("battle profile fixture starts in town"), Subsystem->StartGame());
	TestTrue(TEXT("battle profile fixture creates the desktop workbench"), PlayerController->EnsureDesktopTrainingWidgetsForTest());
	TestTrue(TEXT("battle profile applies through the explicit measurement seam"),
		PlayerController->ApplyDesktopTrainingPerfProfileForTest(TEXT("challenge")));
	TestEqual(TEXT("battle profile enters the existing Battle screen"),
		Subsystem->GetRuntimeState().Screen,
		EGameXXKScreen::Battle);
	TestTrue(TEXT("battle profile owns an active CardBattle"),
		Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle);
	TestFalse(TEXT("battle profile closes the desktop workbench"),
		PlayerController->GetDesktopTrainingWorkbenchWidgetForTest()
		&& PlayerController->GetDesktopTrainingWorkbenchWidgetForTest()->IsWorkbenchVisibleForTest());
	TestNotNull(TEXT("battle profile uses the existing full BattleBoard"),
		PlayerController->GetBattleBoardWidgetForTest());
	TestTrue(TEXT("existing BattleBoard is visible for the battle profile"),
		PlayerController->GetBattleBoardWidgetForTest()
		&& PlayerController->GetBattleBoardWidgetForTest()->IsBattleBoardVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingDirectChallengeBattleSurfaceTest,
	"GameXXK.DesktopTraining.PlayerFlow.DirectChallengeCreatesBattleSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingDirectChallengeBattleSurfaceTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);
	PlayerController->SetDesktopTrainingBootProfileForTest(true);
	TestTrue(TEXT("direct challenge fixture starts in Town"), Subsystem->StartGame());
	TestTrue(TEXT("direct challenge fixture starts with only the HUD workbench"),
		PlayerController->EnsureDesktopTrainingWidgetsForTest());
	TestNull(TEXT("BattleBoard remains lazy before a challenge"), PlayerController->GetBattleBoardWidgetForTest());

	const EGameXXKQuestState QuestBefore = Subsystem->GetRuntimeState().QuestState;
	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	UGameXXKDesktopTrainingWorkbenchWidget* Workbench =
		PlayerController->GetDesktopTrainingWorkbenchWidgetForTest();
	if (!TestNotNull(TEXT("direct challenge fixture owns the Training workbench"), Workbench)
		|| !TestTrue(TEXT("direct challenge fixture opens the Backpack parent"), Workbench->OpenBackpack()))
	{
		return false;
	}
	Workbench->HandleActionClicked(4);
	TestTrue(TEXT("the default cleared stage is selected from the Training panel"),
		Workbench->SelectStageForTest(StageId));
	TestTrue(TEXT("the Training panel starts the direct replay"), Workbench->ClickChallengeForTest());
	PlayerController->RefreshPlayerFlowWidgetsForTest();

	TestEqual(TEXT("direct replay does not accept the town quest"), Subsystem->GetRuntimeState().QuestState, QuestBefore);
	TestTrue(TEXT("the challenge refresh creates the shared route owner"),
		PlayerController->EnsurePlayerFlowWidgetsForTest());
	UGameXXKOneGameRouteMapWidget* RouteMapWidget = PlayerController->GetRouteMapWidgetForTest();
	UGameXXKBattleBoardWidget* BattleBoard = PlayerController->GetBattleBoardWidgetForTest();
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("the direct challenge opens the Slay-the-Spire route map first"),
		Subsystem->IsTrainingChallengeRouteMapActive());
	TestTrue(TEXT("the challenge route map is visible after refresh"),
		RouteMapWidget
			&& RouteMapWidget->GetVisibility() != ESlateVisibility::Collapsed
			&& RouteMapWidget->GetVisibility() != ESlateVisibility::Hidden);
	TestNotNull(TEXT("Battle refresh lazily creates the shared BattleBoard"), BattleBoard);
	TestFalse(TEXT("the challenge route map keeps the BattleBoard hidden before a battle"),
		BattleBoard && BattleBoard->IsBattleBoardVisible());
	TestFalse(TEXT("the challenge route map does not activate the battle overlay"),
		PlayerController->IsBattleOverlayActive());

	TestTrue(TEXT("closing the direct challenge returns its state to the workbench"),
		Subsystem->CancelTrainingChallengeToWorkbench());
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("challenge exit returns to the 2D workbench screen state"),
		Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestFalse(TEXT("challenge exit deactivates the battle overlay"),
		PlayerController->IsBattleOverlayActive());
	TestTrue(TEXT("DesktopTrainingOnly automatically restores its workbench after battle"),
		PlayerController->GetDesktopTrainingWorkbenchWidgetForTest()
		&& PlayerController->GetDesktopTrainingWorkbenchWidgetForTest()->IsWorkbenchVisibleForTest());
	TestTrue(TEXT("returning from Challenge restores the Training panel"),
		PlayerController->GetDesktopTrainingWorkbenchWidgetForTest()
		&& PlayerController->GetDesktopTrainingWorkbenchWidgetForTest()->IsRightPanelOpenForTest());
	TestEqual(TEXT("returning from Challenge keeps Training selected"),
		PlayerController->GetDesktopTrainingWorkbenchWidgetForTest()
			? PlayerController->GetDesktopTrainingWorkbenchWidgetForTest()->GetActiveNavForTest()
			: EGameXXKDesktopTrainingNav::None,
		EGameXXKDesktopTrainingNav::Training);
	TestFalse(TEXT("the shared BattleBoard is hidden after returning to the workbench"),
		BattleBoard && BattleBoard->IsBattleBoardVisible());
	TestEqual(TEXT("the complete 2D loop never accepts the town quest"),
		Subsystem->GetRuntimeState().QuestState, QuestBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingChallengeEventAndAbandonTest,
	"GameXXK.DesktopTraining.PlayerFlow.ChallengeEventAndAbandon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingChallengeEventAndAbandonTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);
	PlayerController->SetDesktopTrainingBootProfileForTest(true);
	TestTrue(TEXT("challenge event fixture starts in Town"), Subsystem->StartGame());
	TestTrue(TEXT("challenge event fixture starts with only the HUD workbench"),
		PlayerController->EnsureDesktopTrainingWidgetsForTest());

	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("the default cleared stage starts a direct replay"), Subsystem->StartTrainingChallenge(StageId));
	PlayerController->RefreshPlayerFlowWidgetsForTest();

	int32 EventNodeId = INDEX_NONE;
	for (const FGameXXKRouteMapNode& Node : Subsystem->GetRuntimeState().RouteMapNodes)
	{
		if (Node.NodeKind == EGameXXKNodeKind::Event)
		{
			EventNodeId = Node.NodeId;
			break;
		}
	}
	TestTrue(TEXT("the generated challenge map contains an Event node"), EventNodeId != INDEX_NONE);

	// Make the event node the reachable choice so the pure-HUD challenge can
	// exercise the same SelectRouteNodeById path the route map button uses.
	Subsystem->GetMutableRuntimeState().ReachableRouteNodeIds = {EventNodeId};
	TestTrue(TEXT("the challenge route opens a generated Event node"), Subsystem->SelectRouteNodeById(EventNodeId));
	TestEqual(TEXT("an Event node opens the route encounter screen"),
		Subsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteEvent);
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestNotNull(TEXT("the desktop profile lazily creates the shared encounter panel"),
		PlayerController->GetRouteEncounterPanelWidgetForTest());
	TestTrue(TEXT("the desktop profile opens the encounter panel for the Event node"),
		PlayerController->IsRouteEncounterPanelOpenForTest());

	TestTrue(TEXT("the player resolves the generated event choice"), Subsystem->ResolveRouteEncounterChoice(0));
	TestEqual(TEXT("resolving the event returns to the challenge route map"),
		Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestFalse(TEXT("the encounter panel closes after the event resolves"),
		PlayerController->IsRouteEncounterPanelOpenForTest());

	UGameXXKOneGameRouteMapWidget* RouteMapWidget = PlayerController->GetRouteMapWidgetForTest();
	TestNotNull(TEXT("the challenge route map stays available for the abandon control"), RouteMapWidget);
	TestTrue(TEXT("closing the challenge opens the abandon confirmation"),
		RouteMapWidget && RouteMapWidget->OpenRouteAbandonConfirmationForTest());
	TestTrue(TEXT("the challenge abandon preview enables its confirm button"),
		RouteMapWidget && RouteMapWidget->IsRouteAbandonConfirmEnabledForTest());
	TestTrue(TEXT("confirming the abandon returns the desktop challenge to the workbench"),
		RouteMapWidget && RouteMapWidget->ConfirmRouteAbandonForTest());
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestFalse(TEXT("the abandoned challenge is no longer active"),
		Subsystem->GetRuntimeState().Training.bChallengeActive);
	TestEqual(TEXT("the abandoned challenge restores the 2D workbench screen state"),
		Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestTrue(TEXT("the desktop workbench is visible after abandoning the challenge"),
		PlayerController->GetDesktopTrainingWorkbenchWidgetForTest()
		&& PlayerController->GetDesktopTrainingWorkbenchWidgetForTest()->IsWorkbenchVisibleForTest());
	TestFalse(TEXT("abandoning the challenge clears the generated route map"),
		Subsystem->GetRuntimeState().bHasGeneratedRouteMap);
	TestEqual(TEXT("abandoning the challenge drops the pending route reward"),
		Subsystem->GetRuntimeState().CardRun.PendingReward.Options.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteSettlementRestoresIdleWorkbenchTest,
	"GameXXK.DesktopTraining.PlayerFlow.RouteSettlementRestoresIdleWorkbench",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteSettlementRestoresIdleWorkbenchTest::RunTest(const FString& Parameters)
{
	const FString FixturePackageName = MakeRouteSettlementWorkbenchWorldPackageName();
	UPackage* const WorldPackage = CreatePackage(*FixturePackageName);
	UWorld* const RuntimeWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		TEXT("L_DesktopTrainingHUD"),
		WorldPackage);
	if (!TestNotNull(TEXT("settlement restore fixture creates canonical game world"), RuntimeWorld))
	{
		return false;
	}
	RuntimeWorld->AddToRoot();
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(RuntimeWorld);
	RuntimeWorld->InitializeActorsForPlay(FURL());

	UGameInstance* const TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("settlement restore fixture starts in Town"), Subsystem->StartGame());

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGameXXKMVPPlayerController* const PlayerController = RuntimeWorld->SpawnActor<AGameXXKMVPPlayerController>(
		AGameXXKMVPPlayerController::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!TestNotNull(TEXT("settlement restore fixture spawns production controller"), PlayerController))
	{
		RuntimeWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(RuntimeWorld);
		RuntimeWorld->RemoveFromRoot();
		return false;
	}
	ULocalPlayer* const LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	PlayerController->SetPlayer(LocalPlayer);
	PlayerController->SetDesktopTrainingBootProfileForTest(true);
	PlayerController->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("canonical controller can host real viewport widgets"),
		PlayerController->CanAddPlayerWidgetsToViewportForTest());
	TestTrue(TEXT("canonical controller creates only the desktop workbench owner"),
		PlayerController->EnsureDesktopTrainingWidgetsForTest());
	TestTrue(TEXT("canonical controller opens the workbench before challenge"),
		PlayerController->OpenDesktopTrainingWorkbench());
	UGameXXKDesktopTrainingWorkbenchWidget* const Workbench =
		PlayerController->GetDesktopTrainingWorkbenchWidgetForTest();
	TestNotNull(TEXT("controller owns the canonical workbench instance"), Workbench);
	TestTrue(TEXT("canonical controller remains the workbench UObject owner"),
		Workbench && Workbench->GetOwningPlayer() == PlayerController);
	TestTrue(TEXT("pre-challenge workbench is visible"), Workbench && Workbench->IsWorkbenchVisibleForTest());

	const FName TravelStageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("fixture starts independent Travel before challenge"), Subsystem->StartTrainingTravel(TravelStageId));
	const FGameXXKTrainingTravelRuntime TravelBefore = Subsystem->GetTrainingTravelRuntimeCopy();
	TestTrue(TEXT("fixture expands backpack before launching challenge"), Workbench && Workbench->OpenBackpack());
	TestTrue(TEXT("fixture selects the challenge stage through the workbench"),
		Workbench && Workbench->SelectStageForTest(TravelStageId));
	TestTrue(TEXT("real workbench challenge action enters route"), Workbench && Workbench->ClickChallengeForTest());
	TestEqual(TEXT("challenge action enters DungeonMap"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("challenge action collapses its workbench"), Workbench && Workbench->IsWorkbenchVisibleForTest());
	UGameXXKOneGameRouteMapWidget* const RouteMap = PlayerController->GetRouteMapWidgetForTest();
	TestNotNull(TEXT("challenge action lazily creates controller-owned route map"), RouteMap);
	TestTrue(TEXT("controller-owned route map is visible before settlement"),
		RouteMap && RouteMap->GetVisibility() == ESlateVisibility::Visible);

	const int32 GoldBefore = Subsystem->GetRuntimeState().PlayerGold;
	const int32 ExpectedGoldAward = Subsystem->GetRuntimeState().CardRun.RouteTravelMoney / 20;
	TestTrue(TEXT("real route X path opens settlement modal"),
		RouteMap && RouteMap->OpenRouteAbandonConfirmationForTest());
	UButton* const ConfirmButton = RouteMap && RouteMap->WidgetTree
		? Cast<UButton>(RouteMap->WidgetTree->FindWidget(TEXT("RouteAbandonConfirmButton")))
		: nullptr;
	TestNotNull(TEXT("settlement restore fixture finds real confirm delegate"), ConfirmButton);
	TestTrue(TEXT("real settlement confirm delegate is bound"), ConfirmButton && ConfirmButton->OnClicked.IsBound());
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.Broadcast();
	}

	const FGameXXKRuntimeState& Settled = Subsystem->GetRuntimeState();
	TestEqual(TEXT("settlement returns authoritative state to Town"), Settled.Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("settlement remains on DesktopTrainingHUD"), Settled.CurrentMapId, FName(TEXT("DesktopTrainingHUD")));
	TestFalse(TEXT("settlement clears dungeon ownership"), Settled.bDungeonActive);
	TestEqual(TEXT("settlement grants previewed ordinary gold once"), Settled.PlayerGold, GoldBefore + ExpectedGoldAward);
	TestTrue(TEXT("settlement records an idempotency receipt"), Settled.CardRun.LastAppliedRouteSettlementId.IsValid());
	TestTrue(TEXT("settlement remains in the same canonical game world"), PlayerController->GetWorld() == RuntimeWorld);
	TestEqual(TEXT("route map collapses after settlement"),
		RouteMap ? RouteMap->GetVisibility() : ESlateVisibility::Visible,
		ESlateVisibility::Collapsed);
	TestFalse(TEXT("encounter overlay is closed after settlement"), PlayerController->IsRouteEncounterPanelOpenForTest());
	TestFalse(TEXT("merchant overlay is closed after settlement"), PlayerController->IsRouteMerchantWidgetOpenForTest());
	TestTrue(TEXT("controller keeps the exact same workbench UObject"),
		PlayerController->GetDesktopTrainingWorkbenchWidgetForTest() == Workbench);
	TestTrue(TEXT("settlement restores visible idle workbench instead of black screen"),
		Workbench && Workbench->IsWorkbenchVisibleForTest());
	TestFalse(TEXT("settlement restores collapsed idle strip, never old expanded backpack"),
		Workbench && Workbench->IsBackpackExpandedForTest());
	const FGameXXKTrainingTravelRuntime TravelAfter = Subsystem->GetTrainingTravelRuntimeCopy();
	TestTrue(TEXT("settlement preserves independent Travel runtime and party HP bit-identically"),
		FGameXXKTrainingTravelRuntime::StaticStruct()->CompareScriptStruct(
			&TravelAfter,
			&TravelBefore,
			PPF_None));
	TestEqual(TEXT("settlement restores GameAndUI input mode"),
		PlayerController->GetTrackedInputModeForTest(),
		EGameXXKTrackedInputMode::GameAndUI);
	TestFalse(TEXT("settlement releases route modal movement lock"), PlayerController->IsMoveInputIgnored());
	TestEqual(TEXT("settlement owns exactly one workbench widget"),
		PlayerController->GetAllDesktopTrainingWorkbenchWidgetsForTest().Num(),
		1);

	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("subsequent refresh keeps same workbench visible"),
		PlayerController->GetDesktopTrainingWorkbenchWidgetForTest() == Workbench
		&& Workbench->IsWorkbenchVisibleForTest());
	TestEqual(TEXT("subsequent refresh never creates a duplicate workbench"),
		PlayerController->GetAllDesktopTrainingWorkbenchWidgetsForTest().Num(),
		1);

	if (Workbench)
	{
		Workbench->RemoveFromParent();
	}
	if (RouteMap)
	{
		RouteMap->RemoveFromParent();
	}
	PlayerController->EndPlay(EEndPlayReason::Destroyed);
	RuntimeWorld->DestroyWorld(false);
	GEngine->DestroyWorldContext(RuntimeWorld);
	RuntimeWorld->RemoveFromRoot();
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
	TestNotNull(TEXT("player controller owns the new meta shop window"), PlayerController->GetMetaShopWidgetForTest());
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

	TestTrue(TEXT("start game opens Qingshan town for player controller flow"), Subsystem->StartGame());
	// The v22 starter grant is itself deterministic: one fixed profile for each
	// profession, with Blade active, so no test-only roster rewrite is needed.
	const FGameXXKCompanionRosterState& StarterRoster = Subsystem->GetRuntimeState().CardRun.CompanionRoster;
	TestEqual(TEXT("StartNewGame grants all six profession companions for the player flow"), StarterRoster.PermanentCompanions.Num(), 6);
	FName StarterCompanionId = NAME_None;
	if (StarterRoster.PermanentCompanions.Num() == 6)
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
	TestFalse(TEXT("world map stays hidden after direct-town new game"), PlayerController->GetWorldMapWidgetForTest()->IsWorldMapVisibleForTest());
	TestTrue(TEXT("town overlay is visible after direct-town new game"), PlayerController->GetTownOverlayWidgetForTest()->IsTownOverlayVisible());
	TestTrue(TEXT("player flow explicitly opens the world map"), Subsystem->OpenWorldMap());
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("world map becomes visible after explicit navigation"), PlayerController->GetWorldMapWidgetForTest()->IsWorldMapVisibleForTest());
	TestFalse(TEXT("town overlay hides while the world map is visible"), PlayerController->GetTownOverlayWidgetForTest()->IsTownOverlayVisible());
	TestTrue(TEXT("controller-routed Qingshan click enters town"), PlayerController->GetWorldMapWidgetForTest()->TrySelectRegion(UGameXXKMVPRules::RegionQingshan()));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("main menu hides after town state"), PlayerController->GetMainMenuWidgetForTest()->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("town overlay appears after town state"), PlayerController->GetTownOverlayWidgetForTest()->IsTownOverlayVisible());
	TestFalse(TEXT("route map hidden before entering dungeon"), PlayerController->GetRouteMapWidgetForTest()->GetVisibility() == ESlateVisibility::Visible);
	TestTrue(TEXT("legacy save fixture can carry the retired inventory panel mode"), Subsystem->OpenTownPanel(EGameXXKTownPanelMode::Inventory));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("player flow normalizes the retired inventory panel mode"), Subsystem->GetRuntimeState().TownPanelMode, EGameXXKTownPanelMode::None);
	TestEqual(TEXT("player flow never auto-opens free inventory from a persisted town panel"), PlayerController->GetInventoryWindowWidgetForTest()->GetWindowModeForTest(), EGameXXKInventoryWindowMode::None);
	TestTrue(TEXT("legacy save fixture can carry the retired trade panel mode"), Subsystem->OpenTownPanel(EGameXXKTownPanelMode::Trade));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("player flow normalizes the retired trade panel mode"), Subsystem->GetRuntimeState().TownPanelMode, EGameXXKTownPanelMode::None);
	TestEqual(TEXT("player flow never renders the retired trade panel during normalization"), PlayerController->GetTownOverlayWidgetForTest()->GetActiveTownPanelForTest(), EGameXXKTownPanelMode::None);
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
	TestFalse(TEXT("accept button keeps the guide in town instead of activating a follower"), QuestNpc->IsFollowerActive());
	TestNull(TEXT("accept button leaves no follower target before 入队"), QuestNpc->GetFollowTarget());
	TestTrue(TEXT("I key opens the independent free inventory window"), PlayerController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::I, IE_Pressed, 1.0f)));
	TestEqual(TEXT("I key records free inventory window mode"), PlayerController->GetInventoryWindowWidgetForTest()->GetWindowModeForTest(), EGameXXKInventoryWindowMode::FreeInventory);
	TestFalse(TEXT("I key free inventory keeps movement input unlocked"), PlayerController->IsInventoryWindowModalInputLockedForTest());
	TestEqual(TEXT("I key does not open the legacy town inventory panel"), Subsystem->GetRuntimeState().TownPanelMode, EGameXXKTownPanelMode::None);
	TestTrue(TEXT("I key closes the independent free inventory window"), PlayerController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::I, IE_Pressed, 1.0f)));
	TestEqual(TEXT("I key clears independent inventory window mode"), PlayerController->GetInventoryWindowWidgetForTest()->GetWindowModeForTest(), EGameXXKInventoryWindowMode::None);
	TestFalse(TEXT("legacy merchant trade entry is retired"), PlayerController->OpenMerchantTradeWindow());
	TestEqual(TEXT("legacy merchant entry never switches inventory into trade mode"), PlayerController->GetInventoryWindowWidgetForTest()->GetWindowModeForTest(), EGameXXKInventoryWindowMode::None);
	TestTrue(TEXT("merchant path opens the new meta shop"), PlayerController->OpenMetaShopWindow());
	TestTrue(TEXT("new meta shop reports open"), PlayerController->IsMetaShopOpenForTest());
	TestTrue(TEXT("new meta shop locks movement input"), PlayerController->IsMetaShopInputLockedForTest());
	TestEqual(TEXT("new meta shop exposes all seven catalog cards"), PlayerController->GetMetaShopWidgetForTest()->GetProductCardCountForTest(), 7);
	TestTrue(TEXT("repeated merchant interaction closes the new meta shop"), PlayerController->OpenMetaShopWindow());
	TestFalse(TEXT("repeated merchant interaction hides the new meta shop"), PlayerController->IsMetaShopOpenForTest());
	TestFalse(TEXT("repeated merchant interaction restores movement input"), PlayerController->IsMetaShopInputLockedForTest());
	TestTrue(TEXT("merchant path can reopen the new meta shop after toggle close"), PlayerController->OpenMetaShopWindow());
	TestTrue(TEXT("new meta shop closes independently"), PlayerController->CloseMetaShopWindow());
	TestFalse(TEXT("new meta shop independent close restores movement input"), PlayerController->IsMetaShopInputLockedForTest());

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
