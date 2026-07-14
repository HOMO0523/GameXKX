#include "GameXXKMVPRules.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Town/GameXXKTownNpcActor.h"
#include "UI/GameXXKTaskPanelWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTownTaskNavigationWidgetTest,
	"GameXXK.MVP.UI.TownTaskNavigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTownTaskNavigationWidgetTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Town;

	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);

	AGameXXKTownNpcActor* QuestNpc = NewObject<AGameXXKTownNpcActor>();
	QuestNpc->SetNpcRole(EGameXXKTownNpcRole::Quest);
	QuestNpc->SetMVPSubsystemForTest(Subsystem);
	APawn* QuestPawn = NewObject<APawn>();

	TestTrue(TEXT("F quest interaction opens the available-task offer panel"), PlayerController->OpenTaskOfferPanelForNpc(QuestNpc, QuestPawn));
	UGameXXKTaskPanelWidget* Widget = PlayerController->GetTaskPanelWidgetForTest();
	TestNotNull(TEXT("task offer panel exists"), Widget);
	if (!Widget)
	{
		return false;
	}

	TestTrue(TEXT("task panel is visible after opening"), Widget->IsTaskPanelOpenForTest());
	TestTrue(TEXT("task offer panel has only main and side filters"), Widget->HasMainAndSideTaskFiltersForTest());
	TestTrue(TEXT("F interaction opens available tasks rather than accepted tasks"), Widget->IsShowingTaskOffersForTest());
	TestEqual(TEXT("main is the default task filter"), Widget->GetActiveCategoryForTest(), EGameXXKTaskCategory::Main);
	TestEqual(TEXT("main offer lists the available runtime main quest"), Widget->GetVisibleTaskCountForTest(), 1);
	TestTrue(TEXT("side filter is selectable"), Widget->SelectTaskCategoryForTest(EGameXXKTaskCategory::Side));
	TestEqual(TEXT("side offer does not fabricate a quest"), Widget->GetVisibleTaskCountForTest(), 0);
	TestTrue(TEXT("task offer can be closed before a new F interaction"), PlayerController->CloseTaskPanel());
	TestTrue(TEXT("a later F interaction reopens the available-task offer panel"), PlayerController->OpenTaskOfferPanelForNpc(QuestNpc, QuestPawn));
	TestEqual(TEXT("each F task offer starts on the main tab even after the prior side tab"), Widget->GetActiveCategoryForTest(), EGameXXKTaskCategory::Main);
	TestEqual(TEXT("reopened main offer restores its available quest"), Widget->GetVisibleTaskCountForTest(), 1);

	TestFalse(TEXT("an offer cannot accept a task id that is not visible in the active offer panel"), PlayerController->AcceptTaskOfferById(FName(TEXT("Task.NotVisible"))));
	TestTrue(TEXT("accept action resolves through the visible task id and pending quest NPC"), Widget->TriggerPrimaryTaskActionForTest());
	TestEqual(TEXT("accept action marks the quest accepted"), State.QuestState, EGameXXKQuestState::Accepted);
	TestTrue(TEXT("accept action preserves follower activation"), QuestNpc->IsFollowerActive());
	TestTrue(TEXT("accept action preserves follower target"), QuestNpc->GetFollowTarget() == QuestPawn);
	TestTrue(TEXT("offer panel stays open after acceptance"), Widget->IsTaskPanelOpenForTest());
	TestTrue(TEXT("offer panel remains in available-task mode after acceptance"), Widget->IsShowingTaskOffersForTest());
	TestEqual(TEXT("accepted task disappears from the available list"), Widget->GetVisibleTaskCountForTest(), 0);
	TestEqual(TEXT("task offer acceptance never creates a tracked task"), State.TrackedTaskId, NAME_None);

	PlayerController->CloseTaskPanel();
	TestTrue(TEXT("Q/HUD task path opens accepted tasks"), PlayerController->OpenTaskPanel());
	TestTrue(TEXT("Q/HUD task panel is accepted-only"), Widget->IsShowingAcceptedTasksForTest());
	TestEqual(TEXT("accepted main quest appears in Q/HUD panel"), Widget->GetVisibleTaskCountForTest(), 1);
	TestFalse(TEXT("accepted-task rows expose no tracking or navigation action"), Widget->HasPrimaryTaskActionForTest());
	TestEqual(TEXT("accepted-task panel does not mutate tracking state"), State.TrackedTaskId, NAME_None);
	TestTrue(TEXT("accepted-task panel can temporarily select the empty side tab"), Widget->SelectTaskCategoryForTest(EGameXXKTaskCategory::Side));
	TestEqual(TEXT("accepted side tab has no fabricated task"), Widget->GetVisibleTaskCountForTest(), 0);
	TestTrue(TEXT("accepted-task panel can close after selecting side"), PlayerController->CloseTaskPanel());
	TestTrue(TEXT("Q/HUD reopening accepted tasks succeeds after a prior side selection"), PlayerController->OpenTaskPanel());
	TestEqual(TEXT("Q/HUD reopening accepted tasks resets to main"), Widget->GetActiveCategoryForTest(), EGameXXKTaskCategory::Main);
	TestEqual(TEXT("Q/HUD reopening accepted tasks shows the accepted main task"), Widget->GetVisibleTaskCountForTest(), 1);

	PlayerController->CloseTaskPanel();
	TestFalse(TEXT("task panel closes cleanly"), Widget->IsTaskPanelOpenForTest());

	UGameXXKMVPSubsystem* ModalSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FGameXXKRuntimeState& ModalState = ModalSubsystem->GetMutableRuntimeState();
	ModalState = UGameXXKMVPRules::CreateNewGame();
	ModalState.Screen = EGameXXKScreen::Town;
	AGameXXKMVPPlayerController* ModalController = NewObject<AGameXXKMVPPlayerController>();
	ModalController->SetMVPSubsystemForTest(ModalSubsystem);
	AGameXXKTownNpcActor* ModalQuestNpc = NewObject<AGameXXKTownNpcActor>();
	ModalQuestNpc->SetNpcRole(EGameXXKTownNpcRole::Quest);
	ModalQuestNpc->SetMVPSubsystemForTest(ModalSubsystem);
	APawn* ModalPawn = NewObject<APawn>();
	TestTrue(TEXT("modal test opens a task offer"), ModalController->OpenTaskOfferPanelForNpc(ModalQuestNpc, ModalPawn));
	TestFalse(TEXT("legacy quest dialog cannot replace an active task offer"), ModalController->OpenQuestDialogForNpc(ModalQuestNpc, ModalPawn));
	TestTrue(TEXT("rejecting a legacy dialog request keeps the original task offer open"), ModalController->IsTaskPanelOpenForTest());
	TestTrue(TEXT("modal test can close the active task offer"), ModalController->CloseTaskPanel());
	TestTrue(TEXT("legacy quest dialog opens after the task offer closes"), ModalController->OpenQuestDialogForNpc(ModalQuestNpc, ModalPawn));
	TestFalse(TEXT("task offer cannot replace an active legacy quest dialog"), ModalController->OpenTaskOfferPanelForNpc(ModalQuestNpc, ModalPawn));
	TestTrue(TEXT("rejecting a task offer request keeps the legacy dialog open"), ModalController->IsQuestDialogOpenForTest());
	TestTrue(TEXT("modal test can close the active legacy dialog"), ModalController->CloseQuestDialog());

	UGameXXKMVPSubsystem* CloseSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FGameXXKRuntimeState& CloseState = CloseSubsystem->GetMutableRuntimeState();
	CloseState = UGameXXKMVPRules::CreateNewGame();
	CloseState.Screen = EGameXXKScreen::Town;
	AGameXXKMVPPlayerController* CloseController = NewObject<AGameXXKMVPPlayerController>();
	CloseController->SetMVPSubsystemForTest(CloseSubsystem);
	AGameXXKTownNpcActor* CloseQuestNpc = NewObject<AGameXXKTownNpcActor>();
	CloseQuestNpc->SetNpcRole(EGameXXKTownNpcRole::Quest);
	CloseQuestNpc->SetMVPSubsystemForTest(CloseSubsystem);
	APawn* ClosePawn = NewObject<APawn>();
	TestTrue(TEXT("state-close test opens a task offer"), CloseController->OpenTaskOfferPanelForNpc(CloseQuestNpc, ClosePawn));
	TestTrue(TEXT("opening the task offer locks movement"), CloseController->IsMoveInputIgnored());
	CloseState.Screen = EGameXXKScreen::DungeonMap;
	CloseController->RefreshPlayerFlowWidgetsForTest();
	TestFalse(TEXT("state transition closes the task panel"), CloseController->IsTaskPanelOpenForTest());
	TestFalse(TEXT("state transition releases task-panel movement input"), CloseController->IsMoveInputIgnored());
	return true;
}

#endif
