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

	const TArray<FGameXXKTaskView> AvailableMainTasks = UGameXXKMVPRules::BuildAvailableTaskViews(State, EGameXXKTaskCategory::Main);
	TestEqual(TEXT("available main task list contains only Qingshan quest"), AvailableMainTasks.Num(), 1);
	if (AvailableMainTasks.Num() != 1)
	{
		return false;
	}

	const FGameXXKTaskView& AvailableMainTask = AvailableMainTasks[0];
	TestEqual(TEXT("available main task has stable id"), AvailableMainTask.Id, UGameXXKMVPRules::TaskQingshanMain());
	TestEqual(TEXT("available main task uses stable title"), AvailableMainTask.Title.ToString(), FString(TEXT("初入江湖")));
	TestEqual(TEXT("available task describes finding the guide"), AvailableMainTask.Description.ToString(), FString(TEXT("前往青山镇寻找引路人")));
	TestEqual(TEXT("available task starts at zero progress"), AvailableMainTask.ProgressCurrent, 0);
	TestEqual(TEXT("available main task has one progress target"), AvailableMainTask.ProgressTarget, 1);
	TestEqual(TEXT("available main task grants gold"), AvailableMainTask.Reward.Gold, 500);
	TestEqual(TEXT("available main task grants experience"), AvailableMainTask.Reward.Experience, 1200);
	TestEqual(TEXT("available main task grants token"), AvailableMainTask.Reward.Token, 10);
	TestFalse(TEXT("available task does not expose navigation"), AvailableMainTask.bCanNavigate);
	TestTrue(TEXT("available task has no navigation target"), AvailableMainTask.NavigationTarget.IsNone());

	const TArray<FGameXXKTaskView> NotAcceptedTasks = UGameXXKMVPRules::BuildAcceptedTaskViews(State, EGameXXKTaskCategory::Main);
	TestEqual(TEXT("unaccepted quest does not appear in accepted list"), NotAcceptedTasks.Num(), 0);

	const TArray<FGameXXKTaskView> AvailableSideTasks = UGameXXKMVPRules::BuildAvailableTaskViews(State, EGameXXKTaskCategory::Side);
	const TArray<FGameXXKTaskView> AcceptedSideTasks = UGameXXKMVPRules::BuildAcceptedTaskViews(State, EGameXXKTaskCategory::Side);
	TestEqual(TEXT("available side task list does not fabricate quests"), AvailableSideTasks.Num(), 0);
	TestEqual(TEXT("accepted side task list does not fabricate quests"), AcceptedSideTasks.Num(), 0);

	TestTrue(TEXT("rules accept the town quest"), UGameXXKMVPRules::AcceptTownQuest(State));
	TestTrue(TEXT("accepting never writes a tracked-task id"), State.TrackedTaskId.IsNone());
	TestEqual(TEXT("accepted quest disappears from available list"), UGameXXKMVPRules::BuildAvailableTaskViews(State, EGameXXKTaskCategory::Main).Num(), 0);

	const TArray<FGameXXKTaskView> AcceptedMainTasks = UGameXXKMVPRules::BuildAcceptedTaskViews(State, EGameXXKTaskCategory::Main);
	TestEqual(TEXT("accepted main task list contains one entry"), AcceptedMainTasks.Num(), 1);
	if (AcceptedMainTasks.Num() != 1)
	{
		return false;
	}

	const FGameXXKTaskView& AcceptedMainTask = AcceptedMainTasks[0];
	TestEqual(TEXT("accepted task describes the north exit"), AcceptedMainTask.Description.ToString(), FString(TEXT("与引路人同行，前往北门出口")));
	TestEqual(TEXT("accepted task completes its progress"), AcceptedMainTask.ProgressCurrent, 1);
	TestFalse(TEXT("accepted task does not expose navigation"), AcceptedMainTask.bCanNavigate);
	TestTrue(TEXT("accepted task has no navigation target"), AcceptedMainTask.NavigationTarget.IsNone());

	State.QuestState = EGameXXKQuestState::Completed;
	TestEqual(TEXT("completed quest is not presented as an accepted task"), UGameXXKMVPRules::BuildAcceptedTaskViews(State, EGameXXKTaskCategory::Main).Num(), 0);

	return true;
}

#endif
