#include "GameXXKTrainingRules.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchLayoutContractTest,
	"GameXXK.DesktopTraining.Workbench.LayoutContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchLayoutContractTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("workbench widget can be constructed without a live viewport"), Widget);
	if (!Widget)
	{
		return false;
	}
	TestEqual(TEXT("warehouse uses four columns"), Widget->GetWarehouseColumnCountForTest(), 4);
	const FVector2D BackpackRatio = Widget->GetBackpackAspectRatioForTest();
	TestTrue(TEXT("backpack aspect ratio keeps the real wide proportion"), FMath::IsNearlyEqual(BackpackRatio.X / BackpackRatio.Y, 1.76f, 0.001f));
	TestEqual(TEXT("three difficulty bands each expose nine stage definitions"), FGameXXKTrainingRules::GetStageDefinitions().Num(), 27);
	TestEqual(TEXT("normal 1-1 id remains stable"), FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1), FName(TEXT("Training.Normal.1-1")));
	return true;
}

#endif
