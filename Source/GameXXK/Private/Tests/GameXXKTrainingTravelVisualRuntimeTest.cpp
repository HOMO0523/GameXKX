#include "UI/GameXXKTrainingTravelVisualRuntime.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelVisualRuntimeScrollsAndLoopsTest,
	"GameXXK.DesktopTraining.TravelVisualRuntime.ScrollsAndLoops",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelVisualRuntimeScrollsAndLoopsTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingTravelVisualRuntime Runtime;

	Runtime.Tick(0.5f, EGameXXKTrainingTravelPhase::Walking);
	TestTrue(TEXT("walking advances the seamless lane offset"), Runtime.GetScrollOffset() > 0.0f);
	TestEqual(TEXT("walking advances the 12 fps walkloop by six frames"), Runtime.GetWalkFrameIndex(), 6);

	const float OffsetAtEncounter = Runtime.GetScrollOffset();
	const int32 FrameAtEncounter = Runtime.GetWalkFrameIndex();
	Runtime.Tick(1.0f, EGameXXKTrainingTravelPhase::Combat);
	TestTrue(TEXT("combat pauses the lane"), FMath::IsNearlyEqual(Runtime.GetScrollOffset(), OffsetAtEncounter));
	TestEqual(TEXT("combat pauses the walkloop frame"), Runtime.GetWalkFrameIndex(), FrameAtEncounter);

	Runtime.NotifyTravelStep(false, false);
	Runtime.Tick(0.5f, EGameXXKTrainingTravelPhase::Walking);
	TestTrue(TEXT("walking resumes after encounter settlement"), Runtime.GetScrollOffset() > OffsetAtEncounter);

	Runtime.NotifyTravelStep(true, true);
	TestTrue(TEXT("a completed stage increments the visual loop count"), Runtime.GetCompletedLoopCount() == 1);
	TestTrue(TEXT("a completed stage resets the lane to its seamless origin"), FMath::IsNearlyZero(Runtime.GetScrollOffset()));
	TestEqual(TEXT("a completed stage resets the walkloop to its first frame"), Runtime.GetWalkFrameIndex(), 0);
	return true;
}

#endif
