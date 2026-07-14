#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Misc/AutomationTest.h"
#include "UI/GameXXKTaskPanelWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTownTaskNavigationWidgetTest,
	"GameXXK.MVP.UI.TownTaskNavigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTownTaskNavigationWidgetTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>();
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Town;

	UGameXXKTaskPanelWidget* Widget = NewObject<UGameXXKTaskPanelWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->TakeWidget();
	Widget->OpenTaskPanel();

	TestTrue(TEXT("task panel is visible after opening"), Widget->IsTaskPanelOpenForTest());
	TestTrue(TEXT("task panel builds four task filters"), Widget->HasAllTaskFiltersForTest());
	TestEqual(TEXT("main is the default task filter"), Widget->GetActiveCategoryForTest(), EGameXXKTaskCategory::Main);
	TestEqual(TEXT("main filter lists the runtime main quest"), Widget->GetVisibleTaskCountForTest(), 1);

	TestTrue(TEXT("daily filter is selectable"), Widget->SelectTaskCategoryForTest(EGameXXKTaskCategory::Daily));
	TestEqual(TEXT("daily becomes active"), Widget->GetActiveCategoryForTest(), EGameXXKTaskCategory::Daily);
	TestEqual(TEXT("daily does not fabricate a quest"), Widget->GetVisibleTaskCountForTest(), 0);

	TestTrue(TEXT("adventure filter is selectable"), Widget->SelectTaskCategoryForTest(EGameXXKTaskCategory::Adventure));
	TestEqual(TEXT("adventure becomes active"), Widget->GetActiveCategoryForTest(), EGameXXKTaskCategory::Adventure);
	TestEqual(TEXT("adventure does not fabricate a quest"), Widget->GetVisibleTaskCountForTest(), 0);

	TestTrue(TEXT("main filter can be restored"), Widget->SelectTaskCategoryForTest(EGameXXKTaskCategory::Main));
	TestTrue(TEXT("primary task action toggles tracking"), Widget->TriggerPrimaryTaskActionForTest());
	TestEqual(TEXT("task action stores the tracked quest"), State.TrackedTaskId, UGameXXKMVPRules::TaskQingshanMain());

	Widget->CloseTaskPanel();
	TestFalse(TEXT("task panel closes cleanly"), Widget->IsTaskPanelOpenForTest());
	return true;
}

#endif
