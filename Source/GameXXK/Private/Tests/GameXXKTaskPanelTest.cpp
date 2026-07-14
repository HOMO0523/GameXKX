#include "GameXXKMVPRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskPanelRulesTest,
	"GameXXK.MVP.UI.TaskPanelRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskPanelRulesTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Town;

	const TArray<FGameXXKTaskView> MainTasks = UGameXXKMVPRules::BuildTaskViews(State, EGameXXKTaskCategory::Main);
	TestEqual(TEXT("main task list contains only Qingshan quest"), MainTasks.Num(), 1);
	if (MainTasks.Num() != 1)
	{
		return false;
	}

	const FGameXXKTaskView& MainTask = MainTasks[0];
	TestEqual(TEXT("main task has stable id"), MainTask.Id, UGameXXKMVPRules::TaskQingshanMain());
	TestEqual(TEXT("main task uses stable title"), MainTask.Title.ToString(), FString(TEXT("初入江湖")));
	TestEqual(TEXT("not accepted task describes finding the guide"), MainTask.Description.ToString(), FString(TEXT("前往青山镇寻找引路人")));
	TestEqual(TEXT("not accepted task starts at zero progress"), MainTask.ProgressCurrent, 0);
	TestEqual(TEXT("main task has one progress target"), MainTask.ProgressTarget, 1);
	TestEqual(TEXT("main task grants gold"), MainTask.Reward.Gold, 500);
	TestEqual(TEXT("main task grants experience"), MainTask.Reward.Experience, 1200);
	TestEqual(TEXT("main task grants token"), MainTask.Reward.Token, 10);
	TestFalse(TEXT("not accepted task cannot navigate"), MainTask.bCanNavigate);
	TestEqual(TEXT("not accepted task targets quest NPC"), MainTask.NavigationTarget, FName(TEXT("QingshanTown_QuestNpc")));

	const TArray<FGameXXKTaskView> SideTasks = UGameXXKMVPRules::BuildTaskViews(State, EGameXXKTaskCategory::Side);
	TestEqual(TEXT("side task list does not fabricate quests"), SideTasks.Num(), 0);

	const FName MainTaskId = UGameXXKMVPRules::TaskQingshanMain();
	TestTrue(TEXT("known main task toggles on"), UGameXXKMVPRules::ToggleTrackedTask(State, MainTaskId));
	TestEqual(TEXT("tracking stores main task id"), State.TrackedTaskId, MainTaskId);
	TestTrue(TEXT("known main task toggles off"), UGameXXKMVPRules::ToggleTrackedTask(State, MainTaskId));
	TestTrue(TEXT("tracking clears when toggled off"), State.TrackedTaskId.IsNone());
	TestFalse(TEXT("unknown task cannot be tracked"), UGameXXKMVPRules::ToggleTrackedTask(State, FName(TEXT("Task.Unknown"))));

	State.QuestState = EGameXXKQuestState::Accepted;
	const TArray<FGameXXKTaskView> AcceptedMainTasks = UGameXXKMVPRules::BuildTaskViews(State, EGameXXKTaskCategory::Main);
	TestEqual(TEXT("accepted main task list still contains one entry"), AcceptedMainTasks.Num(), 1);
	if (AcceptedMainTasks.Num() != 1)
	{
		return false;
	}

	const FGameXXKTaskView& AcceptedMainTask = AcceptedMainTasks[0];
	TestEqual(TEXT("accepted task describes the north exit"), AcceptedMainTask.Description.ToString(), FString(TEXT("与引路人同行，前往北门出口")));
	TestEqual(TEXT("accepted task completes its progress"), AcceptedMainTask.ProgressCurrent, 1);
	TestTrue(TEXT("accepted task can navigate"), AcceptedMainTask.bCanNavigate);
	TestEqual(TEXT("accepted task targets town exit"), AcceptedMainTask.NavigationTarget, FName(TEXT("QingshanInn_TownExit")));

	State.QuestState = EGameXXKQuestState::Completed;
	const TArray<FGameXXKTaskView> CompletedMainTasks = UGameXXKMVPRules::BuildTaskViews(State, EGameXXKTaskCategory::Main);
	TestEqual(TEXT("completed main task list still contains one entry"), CompletedMainTasks.Num(), 1);
	if (CompletedMainTasks.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("completed task describes Huangshan completion"), CompletedMainTasks[0].Description.ToString(), FString(TEXT("黄山之行已经完成")));
	TestEqual(TEXT("completed task retains completed progress"), CompletedMainTasks[0].ProgressCurrent, 1);

	return true;
}

#endif
