#include "MVP/GameXXKOneGameIslandRouteMapBridge.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKOneGameIslandRouteMapBridgeTest,
	"GameXXK.MVP.OneGameIslandRouteMapBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKOneGameIslandRouteMapBridgeTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("target offset keeps a low node near the top padding"),
		AGameXXKOneGameIslandRouteMapBridge::CalculateTargetScrollOffset(3277.0f, 280.0f),
		2997.0f);
	TestEqual(
		TEXT("target offset clamps before the top of the scroll content"),
		AGameXXKOneGameIslandRouteMapBridge::CalculateTargetScrollOffset(180.0f, 280.0f),
		0.0f);

	return true;
}

#endif
